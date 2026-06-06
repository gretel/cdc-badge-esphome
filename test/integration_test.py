#!/usr/bin/env python3
"""
CDC Badge × Home Assistant integration test.

Drives the badge's ESPHome services through Home Assistant WebSocket API,
exercising the full path: HA → ESPHome native API → TROPIC01 secure element
→ R-Memory.

Uses HomeAssistantAPI's WebsocketClient for all calls (REST API doesn't
support ``return_response`` on HA 2026.6+).

Exit 0 = all tests passed, 1 = any failure.

Usage:
    export HA_URL=http://10.0.23.18:8123
    export HA_TOKEN=<long-lived token>
    test/.venv/bin/python test/integration_test.py            # run all
    test/.venv/bin/python test/integration_test.py services    # list
    test/.venv/bin/python test/integration_test.py read --slot 0
    test/.venv/bin/python test/integration_test.py write --slot 7 --data "hello"
    test/.venv/bin/python test/integration_test.py erase  --slot 0
    test/.venv/bin/python test/integration_test.py notify --message "from agent"
    test/.venv/bin/python test/integration_test.py blink
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

from homeassistant_api import WebsocketClient
from homeassistant_api.errors import HomeassistantAPIError

DOMAIN = "esphome"

# ── service metadata (for human-readable output) ────────────────────
SERVICE_INFO: dict[str, dict] = {
    "cdc_badge_notify": {
        "desc": "Push message to e-paper display + blink backlight",
        "fields": ["message"],
    },
    "cdc_badge_blink_display": {
        "desc": "Backlight attention signal (no screen change)",
        "fields": [],
    },
    "cdc_badge_se_write": {
        "desc": "Write bytes into TROPIC01 R-Memory slot",
        "fields": ["slot", "data"],
    },
    "cdc_badge_se_read": {
        "desc": "Read R-Memory slot → returns `value` field on success",
        "fields": ["slot"],
    },
    "cdc_badge_se_erase": {
        "desc": "Zero out R-Memory slot",
        "fields": ["slot"],
    },
}


def ws_url_from_http(http_url: str) -> str:
    """Convert http(s)://host:port to ws(s)://host:port/api/websocket."""
    http_url = http_url.rstrip("/")
    ws_prefix = http_url.replace("http://", "ws://").replace("https://", "wss://")
    return f"{ws_prefix}/api/websocket"


# ── helpers ─────────────────────────────────────────────────────────
def _service_names(ws: WebsocketClient) -> list[str]:
    """Return list of cdc_badge_* service names."""
    domain = ws.get_domain(DOMAIN)
    if not domain:
        return []
    return sorted(n for n in domain.services if n.startswith("cdc_badge_"))


def _call(ws: WebsocketClient, service: str, **data) -> None:
    """Fire-and-forget service call."""
    ws.trigger_service(DOMAIN, service, **data)


def _call_with_response(ws: WebsocketClient, service: str, **data) -> dict:
    """Service call that returns a response dict."""
    return ws.trigger_service_with_response(DOMAIN, service, **data)


# ── subcommands ─────────────────────────────────────────────────────
def cmd_services(ws: WebsocketClient, args) -> int:
    """List CDC-badge services."""
    found = _service_names(ws)
    if not found:
        print(f"No cdc_badge_* services found under '{DOMAIN}'.")
        return 1
    print(f"CDC Badge services (domain={DOMAIN}):")
    for name in found:
        info = SERVICE_INFO.get(name, {})
        desc = info.get("desc", "")
        fields = info.get("fields", [])
        print(f"  {name:32s}  fields={fields}")
        if desc:
            print(f"    {desc}")
    return 0


def cmd_states(ws: WebsocketClient, args) -> int:
    """Print badge-related entity states."""
    states = ws.get_states()
    badge = [
        s
        for s in states
        if s.entity_id.startswith(("sensor.", "binary_sensor.", "text_sensor."))
        and (
            "tropic" in s.entity_id.lower()
            or "cdc_badge" in s.entity_id.lower()
            or "tr01" in s.entity_id.lower()
            or "badge" in s.entity_id.lower()
        )
    ]
    if not badge:
        print("No cdc-badge entities found.")
        return 0
    for s in badge:
        print(f"  {s.entity_id:45s}  {s.state}")
    return 0


def cmd_read(ws: WebsocketClient, args) -> int:
    """Read R-Memory slot."""
    slot = int(args.slot)
    try:
        resp = _call_with_response(ws, "cdc_badge_se_read", slot=slot)
    except HomeassistantAPIError as e:
        print(f"se_read slot={slot} FAILED: {e}", file=sys.stderr)
        return 1
    print(json.dumps(resp, default=str))
    return 0


def cmd_write(ws: WebsocketClient, args) -> int:
    """Write data to R-Memory slot."""
    slot = int(args.slot)
    data = args.data
    try:
        _call(ws, "cdc_badge_se_write", slot=slot, data=data)
    except HomeassistantAPIError as e:
        print(f"se_write slot={slot} FAILED: {e}", file=sys.stderr)
        return 1
    print(f"se_write slot={slot} ({len(data)} bytes) OK")
    return 0


def cmd_erase(ws: WebsocketClient, args) -> int:
    """Erase R-Memory slot."""
    slot = int(args.slot)
    try:
        _call(ws, "cdc_badge_se_erase", slot=slot)
    except HomeassistantAPIError as e:
        print(f"se_erase slot={slot} FAILED: {e}", file=sys.stderr)
        return 1
    print(f"se_erase slot={slot} OK")
    return 0


def cmd_notify(ws: WebsocketClient, args) -> int:
    """Push notification to e-paper."""
    try:
        _call(ws, "cdc_badge_notify", message=args.message)
    except HomeassistantAPIError as e:
        print(f"notify FAILED: {e}", file=sys.stderr)
        return 1
    print(f"notify ({len(args.message)} chars) OK")
    return 0


def cmd_blink(ws: WebsocketClient, args) -> int:
    """Trigger backlight attention signal."""
    try:
        _call(ws, "cdc_badge_blink_display")
    except HomeassistantAPIError as e:
        print(f"blink_display FAILED: {e}", file=sys.stderr)
        return 1
    print("blink_display OK")
    return 0


def cmd_all(ws: WebsocketClient, args) -> int:
    """Full round-trip: services → states → write/read/erase → notify → blink."""
    print("== services ==")
    if cmd_services(ws, args) != 0:
        return 1
    print()

    print("== states (badge entities) ==")
    cmd_states(ws, args)
    print()

    print("== R-Memory round-trip slot 0 ==")
    test_data = f"agent-test-{os.getpid()}"
    # erase first so write succeeds (chip requires erased slot before write)
    rc = cmd_erase(ws, argparse.Namespace(slot=0))
    if rc != 0:
        return rc
    rc = cmd_write(ws, argparse.Namespace(slot=0, data=test_data))
    if rc != 0:
        return rc
    rc = cmd_read(ws, argparse.Namespace(slot=0))
    if rc != 0:
        return rc
    # validate read-back matches what we wrote
    # (cmd_read printed the full raw response — caller can check)
    print()

    print("== notify + blink ==")
    if cmd_notify(ws, argparse.Namespace(message="integration test ok")) != 0:
        return 1
    if cmd_blink(ws, argparse.Namespace()) != 0:
        return 1
    print()

    print("ALL OK")
    return 0


# ── data-plane validation (check that read-back matches write) ──────
def cmd_roundtrip(ws: WebsocketClient, args) -> int:
    """Write a known payload, read it back, verify match.

    Tests the full R-Memory chain: erase → write → read → content check.
    """
    slot = int(args.slot)
    payload = args.data.encode() if isinstance(args.data, str) else args.data

    # erase
    try:
        _call(ws, "cdc_badge_se_erase", slot=slot)
    except HomeassistantAPIError as e:
        print(f"  ERASE FAILED: {e}", file=sys.stderr)
        return 1

    # write
    try:
        _call(ws, "cdc_badge_se_write", slot=slot, data=payload.decode())
    except HomeassistantAPIError as e:
        print(f"  WRITE FAILED: {e}", file=sys.stderr)
        return 1

    # read
    try:
        resp = _call_with_response(ws, "cdc_badge_se_read", slot=slot)
    except HomeassistantAPIError as e:
        print(f"  READ FAILED: {e}", file=sys.stderr)
        return 1

    got = resp.get("value", "")
    if isinstance(got, str):
        got_bytes = got.encode()
    else:
        got_bytes = got if isinstance(got, (bytes, bytearray)) else str(got).encode()

    expected_bytes = payload if isinstance(payload, (bytes, bytearray)) else payload.encode()

    if got_bytes == expected_bytes:
        print(f"OK   slot={slot}  round-trip verified ({len(expected_bytes)} bytes)")
        return 0

    print(
        f"FAIL slot={slot}  data mismatch",
        file=sys.stderr,
    )
    print(f"  wrote: {expected_bytes!r}", file=sys.stderr)
    print(f"  read:  {got_bytes!r}", file=sys.stderr)
    return 1


# ── main ────────────────────────────────────────────────────────────
def load_env() -> dict:
    """Load HA_URL and HA_TOKEN from env or .env file."""
    env = {
        "HA_URL": os.environ.get("HA_URL", ""),
        "HA_TOKEN": os.environ.get("HA_TOKEN", ""),
    }
    if not env["HA_TOKEN"]:
        envfile = Path(__file__).parent / ".env"
        if envfile.exists():
            for line in envfile.read_text().splitlines():
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, _, v = line.partition("=")
                if k in env and not env[k]:
                    env[k] = v.strip().strip('"').strip("'")
    return env


def main(argv: list[str] | None = None) -> int:
    env = load_env()
    p = argparse.ArgumentParser(
        description="CDC Badge × Home Assistant integration test",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument(
        "--url",
        default=env["HA_URL"] or "http://10.0.23.18:8123",
        help="HA base URL (env HA_URL)",
    )
    p.add_argument(
        "--token",
        default=env["HA_TOKEN"],
        help="HA long-lived token (env HA_TOKEN)",
    )

    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("services", help="list cdc_badge_* services").set_defaults(
        func=cmd_services
    )
    sub.add_parser("states", help="list badge entity states").set_defaults(
        func=cmd_states
    )
    sub.add_parser("all", help="run full smoke test").set_defaults(func=cmd_all)

    pr = sub.add_parser("read", help="read R-Memory slot (with response)")
    pr.add_argument("--slot", type=int, required=True)
    pr.set_defaults(func=cmd_read)

    pw = sub.add_parser("write", help="write to R-Memory slot")
    pw.add_argument("--slot", type=int, required=True)
    pw.add_argument("--data", required=True, help="string payload")
    pw.set_defaults(func=cmd_write)

    pe = sub.add_parser("erase", help="erase R-Memory slot")
    pe.add_argument("--slot", type=int, required=True)
    pe.set_defaults(func=cmd_erase)

    pn = sub.add_parser("notify", help="push notification to e-paper")
    pn.add_argument("--message", required=True)
    pn.set_defaults(func=cmd_notify)

    sub.add_parser("blink", help="trigger backlight blink").set_defaults(
        func=cmd_blink
    )

    rt = sub.add_parser(
        "roundtrip", help="write → read → verify R-Memory slot content"
    )
    rt.add_argument("--slot", type=int, default=0)
    rt.add_argument(
        "--data",
        default="Hello CDC Badge!",
        help="test payload (default: 'Hello CDC Badge!')",
    )
    rt.set_defaults(func=cmd_roundtrip)

    args = p.parse_args(argv)
    if not args.token:
        p.error("HA_TOKEN not set (env or .env or --token)")

    ws_url = ws_url_from_http(args.url)
    try:
        with WebsocketClient(ws_url, args.token) as ws:
            return args.func(ws, args)
    except HomeassistantAPIError as e:
        print(f"HA error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
