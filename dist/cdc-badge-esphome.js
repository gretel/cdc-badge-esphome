// CDC Badge — custom Lovelace card
// https://github.com/gretel/cdc-badge-esphome
//
// A dashboard card for the CDC Badge (ESP32-S3 + TROPIC01 + BQ25895).
// Shows battery, network, hardware, keypad, secure element,
// and lets you push notifications to the e-paper display.

import { LitElement, html, css, nothing } from "https://unpkg.com/lit@3.2.0/index.js?module";

// ── Helpers ──────────────────────────────────────────────────

/** Resolve a state object from hass given an entity_id string or undefined. */
function _state(hass, eid) {
  return eid ? hass.states[eid] : undefined;
}

/** Format seconds to "Xh Ym". */
function _fmtUptime(s) {
  if (s == null || isNaN(s)) return "—";
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  return h > 0 ? `${h}h ${m}m` : `${m}m`;
}

/** Clamp a number between lo and hi. */
function clamp(v, lo, hi) {
  return Math.min(Math.max(v, lo), hi);
}

/** Return a battery emoji+bar string from a 0-100 percentage. */
function _batteryBar(pct, charging) {
  if (pct == null || isNaN(pct)) return "—";
  const blocks = Math.round(clamp(pct, 0, 100) / 10);
  const empty = 10 - blocks;
  const icon = charging ? "⚡" : "🔋";
  const bar = "█".repeat(blocks) + "░".repeat(empty);
  return `${icon} ${bar} ${Math.round(pct)}%`;
}

/** WiFi signal bar from dBm. */
function _wifiBar(dbm) {
  if (dbm == null || isNaN(dbm)) return "—";
  const bars = clamp(Math.round((dbm + 100) / 12.5), 0, 8);
  return "▂▄▆█".slice(0, Math.ceil(bars / 2)) + "──".repeat(Math.max(0, 4 - Math.ceil(bars / 2)));
}

/** Colour for a sensor value (used as CSS var reference). */
function _valueColour(states, eid) {
  if (!eid) return "var(--primary-text-color)";
  const s = states[eid];
  if (!s) return "var(--disabled-text-color)";
  return "var(--primary-text-color)";
}

// ── Card component ──────────────────────────────────────────

class CdcBadgeCard extends LitElement {
  static get properties() {
    return { hass: {}, config: {} };
  }

  static getStubConfig() {
    return {};
  }

  setConfig(config) {
    if (!config || typeof config !== "object") {
      throw new Error("CDC Badge: invalid config");
    }
    this.config = { ...config };
  }

  getCardSize() {
    return 8;
  }

  getGridOptions() {
    return { rows: 8, columns: 6, min_rows: 4 };
  }

  // ── State helpers ─────────────────────────────────────────

  _s(eid) {
    return _state(this.hass, eid || this.config[eid]);
  }

  _num(eid) {
    const s = this._s(eid);
    return s ? Number(s.state) : undefined;
  }

  _str(eid) {
    const s = this._s(eid);
    return s ? s.state : undefined;
  }

  _bool(eid) {
    const s = this._s(eid);
    return s ? s.state === "on" || s.state === "true" : undefined;
  }

  _on(eid) {
    return this._bool(eid) === true;
  }

  _attr(eid, key) {
    const s = this._s(eid);
    return s ? s.attributes[key] : undefined;
  }

  // ── Services ──────────────────────────────────────────────

  _callService(domain, service, data) {
    if (!this.hass) return;
    this.hass.callService(domain, service, data);
  }

  _toggleEntity(eid) {
    if (!eid) return;
    this._callService("homeassistant", "toggle", { entity_id: eid });
  }

  _pressButton(eid) {
    if (!eid) return;
    this._callService("button", "press", { entity_id: eid });
  }

  _setNumber(eid, val) {
    if (!eid) return;
    this._callService("number", "set_value", { entity_id: eid, value: val });
  }

  _setLight(eid, state) {
    if (!eid) return;
    this._callService("light", state ? "turn_on" : "turn_off", { entity_id: eid });
  }

  _sendNotify() {
    const msg = this.shadowRoot?.getElementById("cdc-notify-input")?.value?.trim();
    if (!msg || !this.config.service_notify) return;
    this._callService("esphome", this.config.service_notify.replace(/^esphome\./, ""), {
      message: msg,
    });
    // Store in history
    const now = new Date();
    const ts = now.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    this._notifHistory = [{ msg, ts }, ...(this._notifHistory || [])].slice(0, 5);
    this.requestUpdate();
    // Clear input
    const inp = this.shadowRoot?.getElementById("cdc-notify-input");
    if (inp) inp.value = "";
  }

  _handleNotifyKey(e) {
    if (e.key === "Enter") this._sendNotify();
  }

  // ── Render keypad ─────────────────────────────────────────

  _renderKey() {
    const keys = [
      ["key_0", "key_1", "key_2", "key_yes"],
      ["key_3", "key_4", "key_5"],
      ["key_6", "key_7", "key_8", "key_no"],
      [null, "key_9", null],
    ];

    // Column layout: rows with 4 columns max
    return html`
      <div class="section-title"><ha-icon icon="mdi:keyboard"></ha-icon> Keypad</div>
      <div class="keypad-grid">
        ${keys.map(
          (row) => html`
            <div class="keypad-row">
              ${row.map((k) => {
                if (!k) return html`<span class="keypad-spacer"></span>`;
                const eid = this.config[k];
                const pressed = eid ? this._on(eid) : false;
                const label = k.replace("key_", "").toUpperCase();
                const cls = pressed ? "key pressed" : "key";
                return html`<span class="${cls}" title="${eid || ""}">${label}</span>`;
              })}
            </div>
          `
        )}
      </div>
    `;
  }

  // ── Render ────────────────────────────────────────────────

  render() {
    if (!this.hass || !this.config) return html``;

    const c = this.config;
    const $ = this.hass.states;

    // Resolve entity IDs (config key → entity_id string)
    const eid = (key) => c[key];

    const cfg = {
      version: eid("entity_badge_version"),
      battery: eid("entity_battery_level"),
      voltage: eid("entity_battery_voltage"),
      current: eid("entity_charge_current"),
      chargeStatus: eid("entity_charge_status"),
      usbStatus: eid("entity_usb_bus_status"),
      runtime: eid("entity_battery_runtime"),
      ip: eid("entity_ip"),
      ssid: eid("entity_ssid"),
      wifiSig: eid("entity_wifi_signal"),
      wifiStrength: eid("entity_wifi_strength"),
      temp: eid("entity_core_temp"),
      motion: eid("entity_motion"),
      uptime: eid("entity_uptime"),
      sao1: eid("entity_sao_gpio1"),
      sao2: eid("entity_sao_gpio2"),
      seAlarm: eid("entity_se_alarm"),
      seMode: eid("entity_se_chip_mode"),
      seFwRv: eid("entity_se_fw_rv"),
      seFwSpect: eid("entity_se_fw_spect"),
      seSerial: eid("entity_se_serial"),
      notifMsg: eid("entity_notif_msg"),
      backlight: eid("entity_backlight"),
      updateInterval: eid("entity_display_interval"),
      refreshBtn: eid("entity_refresh_btn"),
      sleepBtn: eid("entity_sleep_btn"),
      rebootBtn: eid("entity_reboot_btn"),
    };

    // Values
    const battPct = this._num(cfg.battery);
    const battV = this._num(cfg.voltage);
    const battI = this._num(cfg.current);
    const chargeStr = this._str(cfg.chargeStatus) || "—";
    const usbStr = this._str(cfg.usbStatus) || "—";
    const runtimeH = this._num(cfg.runtime);
    const ipStr = this._str(cfg.ip) || "—";
    const ssidStr = this._str(cfg.ssid) || "—";
    const wifiDbm = this._num(cfg.wifiSig);
    const tempC = this._num(cfg.temp);
    const motionActive = this._on(cfg.motion);
    const uptimeS = this._num(cfg.uptime);
    const sao1On = this._on(cfg.sao1);
    const sao2On = this._on(cfg.sao2);
    const seAlarmOn = this._on(cfg.seAlarm);
    const seModeStr = this._str(cfg.seMode) || "—";
    const seFwRvStr = this._str(cfg.seFwRv);
    const seFwSpectStr = this._str(cfg.seFwSpect);
    const seSerialStr = this._str(cfg.seSerial);
    const versionStr = this._str(cfg.version) || "—";
    const notifMsgStr = this._str(cfg.notifMsg) || "";
    const backlightOn = this._on(cfg.backlight);
    const intervalVal = this._num(cfg.updateInterval) || 60;

    // Online status (HA connected)
    const online = $[c.entity_status]?.state === "on" || $[c.entity_status]?.state === "home";

    // Charge status short label + colour
    const chgInfo = (() => {
      switch (chargeStr) {
        case "Charging":
          return { label: "Charging", cls: "chg-active" };
        case "Charging Done":
          return { label: "Full", cls: "chg-done" };
        case "Discharging":
          return { label: "Discharge", cls: "chg-idle" };
        case "Not Charging":
          return { label: "No Power", cls: "chg-idle" };
        case "Pre-charging":
          return { label: "Pre-chg", cls: "chg-active" };
        default:
          return { label: chargeStr, cls: "chg-idle" };
      }
    })();

    // Abbreviate USB status
    const usbShort =
      usbStr.startsWith("USB-") ? usbStr.slice(4) : usbStr === "None" ? "—" : usbStr;

    // Battery bar
    const isCharging = chargeStr === "Charging" || chargeStr === "Pre-charging";

    return html`
      <ha-card>
        <div class="card-content">

          <!-- ═══ HEADER ═══ -->
          <div class="header">
            <div class="header-left">
              <span class="status-dot ${online ? "online" : "offline"}"></span>
              <span class="header-title">CDC Badge</span>
              <span class="header-version">v${versionStr}</span>
            </div>
          </div>

          <!-- ═══ BATTERY & POWER ═══ -->
          <div class="section">
            <div class="section-title"><ha-icon icon="mdi:power-plug"></ha-icon> Power</div>
            <div class="battery-row">
              <span class="battery-bar ${isCharging ? "charging" : ""}">
                ${_batteryBar(battPct, isCharging)}
              </span>
            </div>
            <div class="info-grid cols-4">
              <div class="metric">
                <span class="metric-label">Voltage</span>
                <span class="metric-value">${battV != null ? battV.toFixed(3) + "V" : "—"}</span>
              </div>
              <div class="metric">
                <span class="metric-label">Current</span>
                <span class="metric-value">${battI != null ? Math.round(battI) + "mA" : "—"}</span>
              </div>
              <div class="metric">
                <span class="metric-label">Status</span>
                <span class="metric-value ${chgInfo.cls}">${chgInfo.label}</span>
              </div>
              <div class="metric">
                <span class="metric-label">USB</span>
                <span class="metric-value">${usbShort}</span>
              </div>
            </div>
            ${runtimeH != null
              ? html`<div class="runtime-row">⏱ Runtime est. ${runtimeH.toFixed(1)}h</div>`
              : ""}
          </div>

          <!-- ═══ NETWORK ═══ -->
          <div class="section">
            <div class="section-title"><ha-icon icon="mdi:wifi"></ha-icon> Network</div>
            <div class="info-grid cols-3">
              <div class="metric wide">
                <span class="metric-label">IP</span>
                <span class="metric-value mono">${ipStr}</span>
              </div>
              <div class="metric">
                <span class="metric-label">SSID</span>
                <span class="metric-value">${ssidStr.length > 16 ? ssidStr.slice(0, 16) + "…" : ssidStr}</span>
              </div>
              <div class="metric">
                <span class="metric-label">Signal</span>
                <span class="metric-value">${_wifiBar(wifiDbm)} ${wifiDbm != null ? Math.round(wifiDbm) + "dBm" : ""}</span>
              </div>
            </div>
          </div>

          <!-- ═══ HARDWARE ═══ -->
          <div class="section">
            <div class="section-title"><ha-icon icon="mdi:chip"></ha-icon> Hardware</div>
            <div class="info-grid cols-4">
              <div class="metric">
                <span class="metric-label">Temp</span>
                <span class="metric-value">${tempC != null ? tempC.toFixed(1) + "°C" : "—"}</span>
              </div>
              <div class="metric">
                <span class="metric-label">Motion</span>
                <span class="metric-value ${motionActive ? "state-on" : ""}">
                  ${motionActive ? "Active" : "Idle"}
                </span>
              </div>
              <div class="metric">
                <span class="metric-label">Uptime</span>
                <span class="metric-value">${_fmtUptime(uptimeS)}</span>
              </div>
              <div class="metric">
                <span class="metric-label">Runs</span>
                <span class="metric-value">${this._num("entity_refresh_count") != null ? Math.round(this._num("entity_refresh_count")) : "—"}</span>
              </div>
            </div>
            ${cfg.sao1 || cfg.sao2
              ? html`
                  <div class="sao-row">
                    ${cfg.sao1
                      ? html`
                          <button
                            class="sao-toggle ${sao1On ? "on" : ""}"
                            @click=${() => this._toggleEntity(cfg.sao1)}
                          >SAO IO1 ${sao1On ? "ON" : "OFF"}</button>
                        `
                      : ""}
                    ${cfg.sao2
                      ? html`
                          <button
                            class="sao-toggle ${sao2On ? "on" : ""}"
                            @click=${() => this._toggleEntity(cfg.sao2)}
                          >SAO IO2 ${sao2On ? "ON" : "OFF"}</button>
                        `
                      : ""}
                  </div>
                `
              : ""}
          </div>

          <!-- ═══ SECURE ELEMENT ═══ -->
          <div class="section">
            <div class="section-title">
              <ha-icon icon="mdi:shield-lock"></ha-icon> Secure Element
              <span class="se-dot ${seAlarmOn ? "alarm" : "ok"}"></span>
              ${seAlarmOn ? html`<span class="se-alarm-label">TAMPER</span>` : ""}
            </div>
            <div class="info-grid cols-3">
              <div class="metric">
                <span class="metric-label">Mode</span>
                <span class="metric-value mono small">${seModeStr}</span>
              </div>
              <div class="metric">
                <span class="metric-label">Alarm</span>
                <span class="metric-value ${seAlarmOn ? "state-alarm" : "state-ok"}">
                  ${seAlarmOn ? "⚠️ DETECTED" : "✓ Clear"}
                </span>
              </div>
              <div class="metric">
                <span class="metric-label">Serial</span>
                <span class="metric-value mono small">${seSerialStr || "—"}</span>
              </div>
            </div>
            ${seFwRvStr || seFwSpectStr
              ? html`
                  <details class="se-details">
                    <summary>Firmware versions</summary>
                    <div class="info-grid cols-2">
                      ${seFwRvStr
                        ? html`<div class="metric"><span class="metric-label">RISC-V</span><span class="metric-value mono small">${seFwRvStr}</span></div>`
                        : ""}
                      ${seFwSpectStr
                        ? html`<div class="metric"><span class="metric-label">SPECT</span><span class="metric-value mono small">${seFwSpectStr}</span></div>`
                        : ""}
                    </div>
                  </details>
                `
              : ""}
          </div>

          <!-- ═══ KEYPAD ═══ -->
          ${c.key_0 || c.key_1 || c.key_yes ? this._renderKey() : ""}

          <!-- ═══ QUICK ACTIONS ═══ -->
          <div class="section">
            <div class="section-title"><ha-icon icon="mdi:lightning-bolt"></ha-icon> Actions</div>
            <div class="actions-row">
              ${cfg.refreshBtn
                ? html`<ha-button unelevated @click=${() => this._pressButton(cfg.refreshBtn)}>
                    <ha-icon icon="mdi:refresh" slot="icon"></ha-icon> Refresh
                  </ha-button>`
                : ""}
              ${cfg.sleepBtn
                ? html`<ha-button unelevated @click=${() => this._pressButton(cfg.sleepBtn)}>
                    <ha-icon icon="mdi:sleep" slot="icon"></ha-icon> Sleep
                  </ha-button>`
                : ""}
              ${cfg.rebootBtn
                ? html`<ha-button unelevated class="btn-danger" @click=${() => this._pressButton(cfg.rebootBtn)}>
                    <ha-icon icon="mdi:restart" slot="icon"></ha-icon> Restart
                  </ha-button>`
                : ""}
            </div>
            <div class="actions-sub-row">
              ${cfg.backlight
                ? html`
                    <label class="toggle-label">
                      <ha-switch
                        ?checked=${backlightOn}
                        @change=${(e) => this._setLight(cfg.backlight, e.target.checked)}
                      ></ha-switch>
                      Backlight
                    </label>
                  `
                : ""}
              ${cfg.updateInterval
                ? html`
                    <label class="select-label">
                      <ha-icon icon="mdi:timer-outline"></ha-icon>
                      <select
                        class="interval-select"
                        @change=${(e) => this._setNumber(cfg.updateInterval, Number(e.target.value))}
                      >
                        ${[15, 30, 60, 120, 300, 600].map(
                          (v) => html`
                            <option value=${v} ?selected=${intervalVal === v}>${v}s</option>
                          `
                        )}
                      </select>
                    </label>
                  `
                : ""}
            </div>
          </div>

          <!-- ═══ NOTIFY ═══ -->
          ${c.service_notify
            ? html`
                <div class="section">
                  <div class="section-title"><ha-icon icon="mdi:message-text-outline"></ha-icon> Send to Display</div>
                  <div class="notify-row">
                    <input
                      id="cdc-notify-input"
                      class="notify-input"
                      type="text"
                      placeholder="Type a message…"
                      @keydown=${this._handleNotifyKey}
                    />
                    <ha-button unelevated @click=${this._sendNotify}>
                      <ha-icon icon="mdi:send" slot="icon"></ha-icon>
                    </ha-button>
                  </div>
                  ${notifMsgStr
                    ? html`<div class="notif-current">
                        <ha-icon icon="mdi:message-text"></ha-icon>
                        <span>${notifMsgStr}</span>
                      </div>`
                    : ""}
                  ${this._notifHistory?.length
                    ? html`
                        <details class="notif-history">
                          <summary>History (${this._notifHistory.length})</summary>
                          ${this._notifHistory.map(
                            (h) => html`
                              <div class="notif-entry">
                                <span class="notif-msg">${h.msg}</span>
                                <span class="notif-ts">${h.ts}</span>
                              </div>
                            `
                          )}
                        </details>
                      `
                    : ""}
                </div>
              `
            : ""}

        </div>
      </ha-card>
    `;
  }

  // ── Styles ─────────────────────────────────────────────────

  static get styles() {
    return css`
      :host {
        --cdc-radius: 12px;
        --cdc-gap: 6px;
        --cdc-section-gap: 14px;
        --cdc-pad: 16px;
        --cdc-font-mono: "SF Mono", "Fira Code", "Cascadia Code", monospace;
      }

      ha-card {
        overflow: hidden;
        border-radius: var(--cdc-radius);
      }

      .card-content {
        padding: var(--cdc-pad);
        display: flex;
        flex-direction: column;
        gap: var(--cdc-section-gap);
      }

      /* ── Header ── */
      .header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        padding-bottom: 4px;
        border-bottom: 1px solid var(--divider-color, rgba(0, 0, 0, 0.08));
      }
      .header-left {
        display: flex;
        align-items: center;
        gap: 8px;
      }
      .header-title {
        font-size: 16px;
        font-weight: 600;
        color: var(--primary-text-color);
      }
      .header-version {
        font-size: 11px;
        color: var(--secondary-text-color);
        background: var(--secondary-background-color, #f0f0f0);
        padding: 1px 6px;
        border-radius: 4px;
      }
      .status-dot {
        width: 10px;
        height: 10px;
        border-radius: 50%;
        display: inline-block;
        flex-shrink: 0;
      }
      .status-dot.online {
        background: var(--state-active-color, #43a047);
        box-shadow: 0 0 4px var(--state-active-color, #43a047);
      }
      .status-dot.offline {
        background: var(--error-color, #e53935);
      }

      /* ── Section ── */
      .section {
        display: flex;
        flex-direction: column;
        gap: var(--cdc-gap);
      }
      .section-title {
        display: flex;
        align-items: center;
        gap: 6px;
        font-size: 12px;
        font-weight: 600;
        text-transform: uppercase;
        letter-spacing: 0.5px;
        color: var(--secondary-text-color);
      }
      .section-title ha-icon {
        --mdc-icon-size: 14px;
      }

      /* ── Battery ── */
      .battery-row {
        font-size: 15px;
        font-weight: 500;
        letter-spacing: 0.3px;
      }
      .battery-bar.charging {
        color: var(--state-active-color, #43a047);
      }
      .runtime-row {
        font-size: 11px;
        color: var(--secondary-text-color);
      }

      /* ── Info grid ── */
      .info-grid {
        display: grid;
        gap: 4px 12px;
      }
      .info-grid.cols-2 { grid-template-columns: 1fr 1fr; }
      .info-grid.cols-3 { grid-template-columns: 1fr 1fr 1fr; }
      .info-grid.cols-4 { grid-template-columns: 1fr 1fr 1fr 1fr; }
      .metric {
        display: flex;
        flex-direction: column;
        gap: 0px;
        min-width: 0;
      }
      .metric.wide { grid-column: span 2; }
      .metric-label {
        font-size: 10px;
        text-transform: uppercase;
        letter-spacing: 0.4px;
        color: var(--secondary-text-color);
      }
      .metric-value {
        font-size: 13px;
        font-weight: 500;
        color: var(--primary-text-color);
        white-space: nowrap;
        overflow: hidden;
        text-overflow: ellipsis;
      }
      .metric-value.mono { font-family: var(--cdc-font-mono); }
      .metric-value.small { font-size: 11px; }
      .metric-value.state-on { color: var(--state-active-color, #43a047); }
      .metric-value.state-alarm { color: var(--error-color, #e53935); font-weight: 600; }
      .metric-value.state-ok { color: var(--state-active-color, #43a047); }
      .metric-value.chg-active { color: var(--state-active-color, #43a047); }
      .metric-value.chg-done { color: var(--accent-color, #2196f3); }
      .metric-value.chg-idle { color: var(--secondary-text-color); }

      /* ── SAO toggles ── */
      .sao-row {
        display: flex;
        gap: 8px;
        margin-top: 2px;
      }
      .sao-toggle {
        flex: 1;
        padding: 6px 10px;
        border: 1px solid var(--divider-color, rgba(0, 0, 0, 0.12));
        border-radius: 6px;
        background: var(--card-background-color, #fff);
        color: var(--primary-text-color);
        font-size: 11px;
        font-weight: 500;
        cursor: pointer;
        transition: background 0.15s, border-color 0.15s;
      }
      .sao-toggle:hover {
        background: var(--secondary-background-color, #f0f0f0);
      }
      .sao-toggle.on {
        border-color: var(--state-active-color, #43a047);
        color: var(--state-active-color, #43a047);
      }

      /* ── Secure Element ── */
      .se-dot {
        width: 8px;
        height: 8px;
        border-radius: 50%;
        display: inline-block;
        margin-left: auto;
      }
      .se-dot.ok { background: var(--state-active-color, #43a047); }
      .se-dot.alarm {
        background: var(--error-color, #e53935);
        animation: pulse-alarm 1.2s infinite;
      }
      @keyframes pulse-alarm {
        0%, 100% { opacity: 1; }
        50% { opacity: 0.4; }
      }
      .se-alarm-label {
        font-size: 10px;
        font-weight: 700;
        color: var(--error-color, #e53935);
        background: color-mix(in srgb, var(--error-color, #e53935) 12%, transparent);
        padding: 1px 6px;
        border-radius: 4px;
      }
      .se-details {
        font-size: 12px;
        color: var(--secondary-text-color);
      }
      .se-details summary {
        cursor: pointer;
        padding: 2px 0;
        font-weight: 500;
      }
      .se-details[open] summary {
        margin-bottom: 4px;
      }

      /* ── Keypad ── */
      .keypad-grid {
        display: flex;
        flex-direction: column;
        gap: 4px;
      }
      .keypad-row {
        display: flex;
        gap: 4px;
        justify-content: center;
      }
      .key {
        display: inline-flex;
        align-items: center;
        justify-content: center;
        min-width: 44px;
        height: 36px;
        padding: 0 8px;
        border-radius: 6px;
        background: var(--secondary-background-color, #f0f0f0);
        color: var(--primary-text-color);
        font-size: 13px;
        font-weight: 600;
        font-family: var(--cdc-font-mono);
        transition: background 0.1s, transform 0.1s, box-shadow 0.1s;
        user-select: none;
      }
      .key.pressed {
        background: var(--state-active-color, #43a047);
        color: #fff;
        transform: scale(0.93);
        box-shadow: inset 0 2px 4px rgba(0,0,0,0.2);
      }
      .keypad-spacer {
        display: inline-block;
        min-width: 44px;
        height: 36px;
      }

      /* ── Actions ── */
      .actions-row {
        display: flex;
        flex-wrap: wrap;
        gap: 6px;
      }
      .actions-row ha-button {
        flex: 1;
        min-width: 80px;
      }
      .actions-row .btn-danger {
        --ha-button-background-color: var(--error-color, #e53935);
        --mdc-theme-primary: #fff;
      }
      .actions-sub-row {
        display: flex;
        flex-wrap: wrap;
        align-items: center;
        gap: 12px;
        margin-top: 2px;
      }
      .toggle-label {
        display: flex;
        align-items: center;
        gap: 6px;
        font-size: 12px;
        color: var(--primary-text-color);
      }
      .select-label {
        display: flex;
        align-items: center;
        gap: 4px;
        font-size: 12px;
        color: var(--secondary-text-color);
      }
      .select-label ha-icon {
        --mdc-icon-size: 14px;
      }
      .interval-select {
        background: var(--input-background-color, #f0f0f0);
        border: 1px solid var(--divider-color, rgba(0,0,0,0.12));
        border-radius: 4px;
        padding: 2px 6px;
        font-size: 12px;
        color: var(--primary-text-color);
      }

      /* ── Notify ── */
      .notify-row {
        display: flex;
        gap: 6px;
      }
      .notify-input {
        flex: 1;
        padding: 8px 10px;
        border: 1px solid var(--divider-color, rgba(0,0,0,0.15));
        border-radius: 6px;
        background: var(--input-background-color, #f5f5f5);
        color: var(--primary-text-color);
        font-size: 13px;
        outline: none;
        transition: border-color 0.15s;
      }
      .notify-input:focus {
        border-color: var(--primary-color);
      }
      .notify-input::placeholder {
        color: var(--secondary-text-color);
        opacity: 0.6;
      }
      .notif-current {
        display: flex;
        align-items: center;
        gap: 6px;
        font-size: 12px;
        color: var(--primary-text-color);
        background: var(--secondary-background-color, #f0f0f0);
        padding: 6px 10px;
        border-radius: 6px;
      }
      .notif-current ha-icon {
        --mdc-icon-size: 14px;
        color: var(--primary-color);
      }
      .notif-history {
        font-size: 12px;
        color: var(--secondary-text-color);
      }
      .notif-history summary {
        cursor: pointer;
        font-weight: 500;
        padding: 2px 0;
      }
      .notif-entry {
        display: flex;
        justify-content: space-between;
        align-items: center;
        padding: 2px 0 2px 12px;
        gap: 8px;
      }
      .notif-msg {
        flex: 1;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
      }
      .notif-ts {
        flex-shrink: 0;
        font-size: 10px;
        color: var(--secondary-text-color);
        opacity: 0.7;
      }
    `;
  }
}

// ── Editor ───────────────────────────────────────────────────

class CdcBadgeCardEditor extends LitElement {
  static get properties() {
    return { hass: {}, config: {} };
  }

  setConfig(config) {
    this.config = config || {};
  }

  _update(key, ev) {
    const val = ev.detail?.value ?? ev.target?.value;
    const newConfig = { ...this.config, [key]: val };
    // Remove empty strings so stub defaults apply
    Object.keys(newConfig).forEach((k) => {
      if (newConfig[k] === "" || newConfig[k] == null) delete newConfig[k];
    });
    const event = new Event("config-changed", { bubbles: true, composed: true });
    event.detail = { config: newConfig };
    this.dispatchEvent(event);
  }

  _entityPicker(key, label, required = false) {
    return html`
      <ha-entity-picker
        .hass=${this.hass}
        .value=${this.config[key] || ""}
        .label=${label}
        .required=${required}
        @value-changed=${(e) => this._update(key, e)}
        allow-custom-entity
      ></ha-entity-picker>
    `;
  }

  render() {
    if (!this.hass) return html``;

    return html`
      <div class="editor">
        <div class="editor-section">
          <div class="editor-section-title">🪪 Status</div>
          ${this._entityPicker("entity_status", "Connectivity (binary_sensor)", false)}
          ${this._entityPicker("entity_badge_version", "Firmware Version (text_sensor)", false)}
          ${this._entityPicker("entity_uptime", "Uptime (sensor)", false)}
          ${this._entityPicker("entity_refresh_count", "Refresh Count (sensor, diagnostic)", false)}
        </div>

        <div class="editor-section">
          <div class="editor-section-title">🔋 Power</div>
          ${this._entityPicker("entity_battery_level", "Battery Level (%)", false)}
          ${this._entityPicker("entity_battery_voltage", "Battery Voltage", false)}
          ${this._entityPicker("entity_charge_current", "Charge Current", false)}
          ${this._entityPicker("entity_charge_status", "Charge Status (text_sensor)", false)}
          ${this._entityPicker("entity_usb_bus_status", "USB Bus Status (text_sensor)", false)}
          ${this._entityPicker("entity_battery_runtime", "Battery Runtime (sensor)", false)}
        </div>

        <div class="editor-section">
          <div class="editor-section-title">🌐 Network</div>
          ${this._entityPicker("entity_ip", "IP Address (text_sensor)", false)}
          ${this._entityPicker("entity_ssid", "SSID (text_sensor)", false)}
          ${this._entityPicker("entity_wifi_signal", "WiFi Signal (sensor, dBm)", false)}
          ${this._entityPicker("entity_wifi_strength", "WiFi Strength (sensor, %)", false)}
        </div>

        <div class="editor-section">
          <div class="editor-section-title">⚙️ Hardware</div>
          ${this._entityPicker("entity_core_temp", "Core Temperature", false)}
          ${this._entityPicker("entity_motion", "Motion (binary_sensor)", false)}
          ${this._entityPicker("entity_sao_gpio1", "SAO IO1 (switch)", false)}
          ${this._entityPicker("entity_sao_gpio2", "SAO IO2 (switch)", false)}
        </div>

        <div class="editor-section">
          <div class="editor-section-title">🔒 Secure Element</div>
          ${this._entityPicker("entity_se_alarm", "Tamper Alarm (binary_sensor)", false)}
          ${this._entityPicker("entity_se_chip_mode", "Chip Mode (text_sensor)", false)}
          ${this._entityPicker("entity_se_fw_rv", "RISC-V Version (text_sensor)", false)}
          ${this._entityPicker("entity_se_fw_spect", "SPECT Version (text_sensor)", false)}
          ${this._entityPicker("entity_se_serial", "Serial (text_sensor)", false)}
        </div>

        <div class="editor-section">
          <div class="editor-section-title">🔢 Keypad</div>
          ${["key_0","key_1","key_2","key_3","key_4","key_5","key_6","key_7","key_8","key_9","key_yes","key_no"].map(
            (k) => this._entityPicker(k, `${k.replace("key_", "Key ")} (binary_sensor)`, false)
          )}
        </div>

        <div class="editor-section">
          <div class="editor-section-title">🎮 Controls</div>
          ${this._entityPicker("entity_refresh_btn", "Refresh Button", false)}
          ${this._entityPicker("entity_sleep_btn", "Deep Sleep Button", false)}
          ${this._entityPicker("entity_reboot_btn", "Restart Button", false)}
          ${this._entityPicker("entity_backlight", "Backlight (light)", false)}
          ${this._entityPicker("entity_display_interval", "Display Interval (number)", false)}
        </div>

        <div class="editor-section">
          <div class="editor-section-title">📨 Notify</div>
          <ha-textfield
            .label=${"Notify service (e.g. esphome.cdc_badge_notify)"}
            .value=${this.config.service_notify || ""}
            @input=${(e) => this._update("service_notify", e)}
          ></ha-textfield>
          ${this._entityPicker("entity_notif_msg", "Notification Msg (text_sensor)", false)}
        </div>
      </div>
    `;
  }

  static get styles() {
    return css`
      .editor {
        padding: 12px;
        display: flex;
        flex-direction: column;
        gap: 16px;
      }
      .editor-section {
        display: flex;
        flex-direction: column;
        gap: 6px;
      }
      .editor-section-title {
        font-size: 13px;
        font-weight: 600;
        color: var(--primary-text-color);
        padding-bottom: 2px;
        border-bottom: 1px solid var(--divider-color);
        margin-bottom: 2px;
      }
      ha-entity-picker,
      ha-textfield {
        width: 100%;
      }
    `;
  }
}

// ── Registration ────────────────────────────────────────────

customElements.define("cdc-badge-card", CdcBadgeCard);
customElements.define("cdc-badge-card-editor", CdcBadgeCardEditor);

window.customCards = window.customCards || [];
window.customCards.push({
  type: "cdc-badge-card",
  name: "CDC Badge",
  description: "Dashboard for the CDC Badge hardware — battery, keypad, secure element, display notify",
  preview: true,
  documentationURL: "https://github.com/gretel/cdc-badge-esphome",
  getEntitySuggestion: (hass, entityId) => {
    const domain = entityId.split(".")[0];
    // Only suggest for ESPHome entities
    if (domain !== "sensor" && domain !== "binary_sensor" && domain !== "switch" && domain !== "light" && domain !== "button" && domain !== "number") {
      return null;
    }
    // Only suggest if the entity name contains "badge" or the device is CDC Badge
    const state = hass.states[entityId];
    if (!state) return null;
    const devName = state.attributes?.friendly_name || "";
    if (!devName.toLowerCase().includes("badge") &&
        !devName.toLowerCase().includes("cdc") &&
        !state.entity_id.toLowerCase().includes("badge")) {
      return null;
    }
    return {
      config: { type: "custom:cdc-badge-card" },
    };
  },
});
