## Prereqs

```sh
uv sync
```

Creates `.venv` and installs `HomeAssistantAPI`.

HA token: Profile → Long-Lived Access Tokens on your HA instance.

## Secrets

`.env` (gitignored):

```
HA_URL=http://10.0.23.18:8123
HA_TOKEN=<token>
```

`HA_URL` defaults to `http://10.0.23.18:8123`.

## Smoke test

```sh
uv run integration_test.py all
```

Exits 0 on pass, 1 on any failure.

## Commands

```sh
uv run integration_test.py services
uv run integration_test.py states
uv run integration_test.py read     --slot 0
uv run integration_test.py write    --slot 7 --data hello
uv run integration_test.py erase    --slot 0
uv run integration_test.py notify   --message "from agent"
uv run integration_test.py blink
uv run integration_test.py roundtrip --slot 0 --data "verify me"
```

## Services exercised

| Service | Purpose |
|---|---|
| `cdc_badge_notify` | message → e-paper, backlight 5s |
| `cdc_badge_blink_display` | backlight blink |
| `cdc_badge_se_write` | write TROPIC01 R-Memory slot |
| `cdc_badge_se_read` | read R-Memory slot → `value` |
| `cdc_badge_se_erase` | zero slot |
