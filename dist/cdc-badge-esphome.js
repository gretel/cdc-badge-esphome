// CDC Badge — custom Lovelace card
// https://github.com/gretel/cdc-badge-esphome

import { LitElement, html, css } from "https://unpkg.com/lit@3.2.0/index.js?module";

// ── Helpers ──────────────────────────────────────────────────

function _state(hass, eid) {
  return eid ? hass.states[eid] : undefined;
}

function _fmtUptime(s) {
  if (s == null || isNaN(s)) return "—";
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  return h > 0 ? `${h}h ${m}m` : `${m}m`;
}

function clamp(v, lo, hi) {
  return Math.min(Math.max(v, lo), hi);
}

// ── Card ─────────────────────────────────────────────────────

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
    return 9;
  }

  getGridOptions() {
    return { rows: 9, columns: 6, min_rows: 4 };
  }

  // ── State -------------------------

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

  // ── Services ----------------------

  _callService(domain, service, data) {
    if (this.hass) this.hass.callService(domain, service, data);
  }
  _toggleEntity(eid) {
    if (eid) this._callService("homeassistant", "toggle", { entity_id: eid });
  }
  _pressButton(eid) {
    if (eid) this._callService("button", "press", { entity_id: eid });
  }
  _setNumber(eid, val) {
    if (eid) this._callService("number", "set_value", { entity_id: eid, value: val });
  }
  _setLight(eid, state) {
    if (eid) this._callService("light", state ? "turn_on" : "turn_off", { entity_id: eid });
  }

  _sendNotify() {
    const el = this.shadowRoot?.getElementById("cdc-notify-input");
    const msg = el?.value?.trim();
    if (!msg || !this.config.service_notify) return;
    this._callService("esphome", this.config.service_notify.replace(/^esphome\./, ""), { message: msg });
    const ts = new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    this._notifHistory = [{ msg, ts }, ...(this._notifHistory || [])].slice(0, 5);
    this.requestUpdate();
    if (el) el.value = "";
  }
  _handleNotifyKey(e) {
    if (e.key === "Enter") this._sendNotify();
  }

  // ── Render helpers ----------------

  _metric(label, value, opts = {}) {
    const cls = opts.cls || "";
    const unit = opts.unit || "";
    const val = value != null ? (opts.fixed != null ? Number(value).toFixed(opts.fixed) : value) : "—";
    return html`
      <div class="metric ${opts.wide ? "wide" : ""}">
        <span class="metric-label">${label}</span>
        <span class="metric-value ${cls}">${val}${unit}</span>
      </div>`;
  }

  _renderKey() {
    const rows = [
      ["key_0", "key_1", "key_2", "key_yes"],
      ["key_3", "key_4", "key_5"],
      ["key_6", "key_7", "key_8", "key_no"],
      [null, "key_9"],
    ];
    return html`
      <div class="sec">
        <div class="sec-title">Keypad</div>
        <div class="keypad">
          ${rows.map(
            (row) => html`
              <div class="kp-row">
                ${row.map(
                  (k) =>
                    k
                      ? html`<span class="kp-key ${this._on(k) ? "on" : ""}">${k.replace("key_", "")}</span>`
                      : html`<span class="kp-spacer"></span>`
                )}
              </div>`
          )}
        </div>
      </div>`;
  }

  _progressBar(value, max, color) {
    const pct = clamp((value / max) * 100, 0, 100);
    return html`<div class="bar-track"><div class="bar-fill" style="width:${pct}%;background:${color}"></div></div>`;
  }

  // ── Render ────────────────────────

  render() {
    if (!this.hass || !this.config) return html``;

    const eid = (k) => this.config[k];
    const n = (k) => this._num(eid(k));
    const s = (k) => this._str(eid(k));
    const on = (k) => this._on(eid(k));

    const battPct = n("entity_battery_level");
    const battV = n("entity_battery_voltage");
    const battI = n("entity_charge_current");
    const chargeStr = s("entity_charge_status") || "—";
    const usbStr = s("entity_usb_bus_status") || "—";
    const runtimeH = n("entity_battery_runtime");
    const ipStr = s("entity_ip") || "—";
    const ssidStr = s("entity_ssid") || "—";
    const wifiDbm = n("entity_wifi_signal");
    const tempC = n("entity_core_temp");
    const uptimeS = n("entity_uptime");
    const versionStr = s("entity_badge_version") || "—";
    const notifMsgStr = s("entity_notif_msg") || "";
    const seModeStr = s("entity_se_chip_mode") || "—";
    const seFwRvStr = s("entity_se_fw_rv");
    const seFwSpectStr = s("entity_se_fw_spect");
    const seSerialStr = s("entity_se_serial");
    const intervalVal = n("entity_display_interval") || 60;
    const online = this.hass.states[this.config.entity_status]?.state === "on"
      || this.hass.states[this.config.entity_status]?.state === "home";
    const isCharging = chargeStr === "Charging" || chargeStr === "Pre-charging";
    const motionActive = on("entity_motion");
    const seAlarmOn = on("entity_se_alarm");
    const sao1On = on("entity_sao_gpio1");
    const sao2On = on("entity_sao_gpio2");
    const backlightOn = on("entity_backlight");

    const chgCls = ({ "Charging": "green", "Charging Done": "blue", "Pre-charging": "green" })[chargeStr] || "dim";
    const usbShort = usbStr.startsWith("USB-") ? usbStr.slice(4) : usbStr === "None" ? "—" : usbStr;

    const battColor = isCharging ? "var(--state-active-color,#43a047)" : battPct != null && battPct < 20 ? "var(--error-color,#e53935)" : "var(--primary-color)";
    const wifiPct = wifiDbm != null ? clamp((wifiDbm + 100) * 2, 0, 100) : null;
    const wifiColor = wifiPct != null ? (wifiPct > 60 ? "var(--state-active-color,#43a047)" : wifiPct > 30 ? "#ff9800" : "var(--error-color,#e53935)") : "#999";

    return html`
      <ha-card>
        <div class="card">

          <!-- HEADER -->
          <div class="header">
            <div class="h-left">
              <span class="dot ${online ? "on" : "off"}"></span>
              <span class="h-title">CDC Badge</span>
              <span class="h-ver">v${versionStr}</span>
            </div>
          </div>

          <!-- POWER -->
          <div class="sec">
            <div class="sec-title">Power</div>
            <div class="batt-row">
              <span class="batt-pct">${battPct != null ? Math.round(battPct) + "%" : "—"}</span>
              <span class="batt-icon">${isCharging ? "⚡" : "🔋"}</span>
            </div>
            ${battPct != null ? this._progressBar(battPct, 100, battColor) : ""}
            <div class="grid-2">
              ${this._metric("Voltage", battV, { unit: "V", fixed: 3 })}
              ${this._metric("Current", battI, { unit: "mA" })}
              <div class="metric"><span class="metric-label">Charge</span><span class="metric-value ${chgCls}">${({ "Charging": "Active", "Charging Done": "Done", "Discharging": "Dischg", "Not Charging": "Off", "Pre-charging": "Pre" })[chargeStr] || chargeStr}</span></div>
              <div class="metric"><span class="metric-label">USB</span><span class="metric-value">${usbShort}</span></div>
            </div>
            ${runtimeH != null ? html`<div class="footnote">Runtime ≈${runtimeH.toFixed(1)}h</div>` : ""}
          </div>

          <!-- NETWORK -->
          <div class="sec">
            <div class="sec-title">Network</div>
            <div class="grid-2">
              <div class="metric wide"><span class="metric-label">IP</span><span class="metric-value mono">${ipStr}</span></div>
              <div class="metric"><span class="metric-label">SSID</span><span class="metric-value">${ssidStr.length > 14 ? ssidStr.slice(0, 14) + "…" : ssidStr || "—"}</span></div>
            </div>
            ${wifiDbm != null
              ? html`
                  <div class="wifi-row">
                    <span class="wifi-label">${Math.round(wifiDbm)} dBm</span>
                    ${this._progressBar(wifiPct, 100, wifiColor)}
                  </div>`
              : ""}
          </div>

          <!-- HARDWARE -->
          <div class="sec">
            <div class="sec-title">Hardware</div>
            <div class="grid-2">
              ${this._metric("Temp", tempC, { unit: "°C", fixed: 1 })}
              ${this._metric("Uptime", _fmtUptime(uptimeS))}
              <div class="metric"><span class="metric-label">Motion</span><span class="metric-value ${motionActive ? "green" : "dim"}">${motionActive ? "Active" : "Idle"}</span></div>
              <div class="metric"><span class="metric-label">Display</span><span class="metric-value">${n("entity_refresh_count") != null ? Math.round(n("entity_refresh_count")) + " runs" : "—"}</span></div>
            </div>
            ${eid("entity_sao_gpio1") || eid("entity_sao_gpio2")
              ? html`
                  <div class="sao-row">
                    ${eid("entity_sao_gpio1") ? html`<button class="sao ${sao1On ? "on" : ""}" @click=${() => this._toggleEntity(eid("entity_sao_gpio1"))}>IO1 ${sao1On ? "ON" : "OFF"}</button>` : ""}
                    ${eid("entity_sao_gpio2") ? html`<button class="sao ${sao2On ? "on" : ""}" @click=${() => this._toggleEntity(eid("entity_sao_gpio2"))}>IO2 ${sao2On ? "ON" : "OFF"}</button>` : ""}
                  </div>`
              : ""}
          </div>

          <!-- SECURE ELEMENT -->
          <div class="sec">
            <div class="sec-title">
              <span>Secure Element</span>
              <span class="se-dot ${seAlarmOn ? "alarm" : "ok"}"></span>
              ${seAlarmOn ? html`<span class="se-badge">TAMPER</span>` : ""}
            </div>
            <div class="grid-2">
              <div class="metric"><span class="metric-label">Mode</span><span class="metric-value mono sm">${seModeStr}</span></div>
              <div class="metric"><span class="metric-label">Tamper</span><span class="metric-value ${seAlarmOn ? "red" : "green"}">${seAlarmOn ? "⚠ DETECTED" : "✓ Clear"}</span></div>
              ${seSerialStr ? html`<div class="metric wide"><span class="metric-label">Serial</span><span class="metric-value mono sm">${seSerialStr}</span></div>` : ""}
            </div>
            ${seFwRvStr || seFwSpectStr
              ? html`
                  <details class="details">
                    <summary>Firmware</summary>
                    <div class="grid-2">
                      ${seFwRvStr ? html`<div class="metric"><span class="metric-label">RISC-V</span><span class="metric-value mono sm">${seFwRvStr}</span></div>` : ""}
                      ${seFwSpectStr ? html`<div class="metric"><span class="metric-label">SPECT</span><span class="metric-value mono sm">${seFwSpectStr}</span></div>` : ""}
                    </div>
                  </details>`
              : ""}
          </div>

          <!-- KEYPAD -->
          ${eid("key_0") || eid("key_1") || eid("key_yes") ? this._renderKey() : ""}

          <!-- ACTIONS -->
          <div class="sec">
            <div class="sec-title">Actions</div>
            <div class="act-row">
              ${eid("entity_refresh_btn") ? html`<ha-button unelevated @click=${() => this._pressButton(eid("entity_refresh_btn"))}><ha-icon icon="mdi:refresh" slot="icon"></ha-icon> Refresh</ha-button>` : ""}
              ${eid("entity_sleep_btn") ? html`<ha-button unelevated @click=${() => this._pressButton(eid("entity_sleep_btn"))}><ha-icon icon="mdi:sleep" slot="icon"></ha-icon> Sleep</ha-button>` : ""}
              ${eid("entity_reboot_btn") ? html`<ha-button unelevated class="red" @click=${() => this._pressButton(eid("entity_reboot_btn"))}><ha-icon icon="mdi:restart" slot="icon"></ha-icon> Restart</ha-button>` : ""}
            </div>
            <div class="opt-row">
              ${eid("entity_backlight") ? html`<label class="toggle"><ha-switch ?checked=${backlightOn} @change=${(e) => this._setLight(eid("entity_backlight"), e.target.checked)}></ha-switch> Backlight</label>` : ""}
              ${eid("entity_display_interval") ? html`<label class="select"><ha-icon icon="mdi:timer-outline"></ha-icon><select @change=${(e) => this._setNumber(eid("entity_display_interval"), +e.target.value)}>${[15, 30, 60, 120, 300, 600].map((v) => html`<option value=${v} ?selected=${intervalVal === v}>${v}s</option>`)}</select></label>` : ""}
            </div>
          </div>

          <!-- NOTIFY -->
          ${this.config.service_notify
            ? html`
                <div class="sec">
                  <div class="sec-title">Send to Display</div>
                  <div class="notify-row">
                    <input id="cdc-notify-input" class="notify-input" type="text" placeholder="Type a message…" @keydown=${this._handleNotifyKey} />
                    <ha-button unelevated @click=${this._sendNotify}><ha-icon icon="mdi:send" slot="icon"></ha-icon></ha-button>
                  </div>
                  ${notifMsgStr ? html`<div class="notif-bar"><ha-icon icon="mdi:message-text"></ha-icon>${notifMsgStr}</div>` : ""}
                  ${this._notifHistory?.length
                    ? html`
                        <details class="details">
                          <summary>History (${this._notifHistory.length})</summary>
                          ${this._notifHistory.map((h) => html`<div class="notif-entry"><span class="notif-msg">${h.msg}</span><span class="notif-ts">${h.ts}</span></div>`)}
                        </details>`
                    : ""}
                </div>`
            : ""}

        </div>
      </ha-card>`;
  }

  // ── Styles ─────────────────────────────────────────────────

  static get styles() {
    return css`
      /* ── Reset ── */
      :host { --cdc-mono: "SF Mono","Fira Code","Cascadia Code",monospace; }

      ha-card { border-radius: 12px; }
      .card { padding: 16px; display: flex; flex-direction: column; gap: 12px; }

      /* ── Header ── */
      .header { display: flex; align-items: center; border-bottom: 1px solid var(--divider-color,rgba(0,0,0,0.08)); padding-bottom: 6px; }
      .h-left { display: flex; align-items: center; gap: 8px; }
      .h-title { font-size: 15px; font-weight: 600; color: var(--primary-text-color); }
      .h-ver { font-size: 10px; color: var(--secondary-text-color); background: var(--secondary-background-color,#f0f0f0); padding: 1px 5px; border-radius: 3px; }
      .dot { width: 10px; height: 10px; border-radius: 50%; flex-shrink: 0; }
      .dot.on { background: var(--state-active-color,#43a047); box-shadow: 0 0 4px var(--state-active-color,#43a047); }
      .dot.off { background: var(--error-color,#e53935); }

      /* ── Sections ── */
      .sec { display: flex; flex-direction: column; gap: 6px; }
      .sec-title { display: flex; align-items: center; gap: 6px; font-size: 11px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.4px; color: var(--secondary-text-color); }
      .sec-title ha-icon { --mdc-icon-size: 13px; }

      /* ── Battery ── */
      .batt-row { display: flex; align-items: center; justify-content: space-between; }
      .batt-pct { font-size: 22px; font-weight: 700; color: var(--primary-text-color); line-height: 1; }
      .batt-icon { font-size: 20px; }
      .bar-track { height: 8px; background: var(--secondary-background-color,#e8e8e8); border-radius: 4px; overflow: hidden; }
      .bar-fill { height: 100%; border-radius: 4px; transition: width 0.3s; }

      /* ── Metric grid ── */
      .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 2px 10px; }
      .metric { display: flex; flex-direction: column; gap: 0; min-width: 0; }
      .metric.wide { grid-column: 1 / -1; }
      .metric-label { font-size: 9px; text-transform: uppercase; letter-spacing: 0.3px; color: var(--secondary-text-color); }
      .metric-value { font-size: 13px; font-weight: 500; color: var(--primary-text-color); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
      .metric-value.mono { font-family: var(--cdc-mono); }
      .metric-value.sm { font-size: 11px; }
      .metric-value.green { color: var(--state-active-color,#43a047); }
      .metric-value.red { color: var(--error-color,#e53935); font-weight: 600; }
      .metric-value.blue { color: var(--accent-color,#2196f3); }
      .metric-value.dim { color: var(--secondary-text-color); }

      /* ── WiFi ── */
      .wifi-row { display: flex; align-items: center; gap: 8px; }
      .wifi-label { font-size: 11px; font-family: var(--cdc-mono); color: var(--secondary-text-color); white-space: nowrap; min-width: 48px; }
      .wifi-row .bar-track { flex: 1; }

      /* ── Footnotes ── */
      .footnote { font-size: 10px; color: var(--secondary-text-color); }

      /* ── SAO toggles ── */
      .sao-row { display: flex; gap: 6px; margin-top: 2px; }
      .sao { flex: 1; padding: 5px 8px; border: 1px solid var(--divider-color,rgba(0,0,0,0.12)); border-radius: 5px; background: var(--card-background-color,#fff); color: var(--primary-text-color); font-size: 11px; font-weight: 500; cursor: pointer; transition: background 0.15s; }
      .sao:hover { background: var(--secondary-background-color,#f0f0f0); }
      .sao.on { border-color: var(--state-active-color,#43a047); color: var(--state-active-color,#43a047); }

      /* ── Secure Element ── */
      .se-dot { width: 8px; height: 8px; border-radius: 50%; flex-shrink: 0; }
      .se-dot.ok { background: var(--state-active-color,#43a047); }
      .se-dot.alarm { background: var(--error-color,#e53935); animation: pulse 1.2s infinite; }
      @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.4} }
      .se-badge { font-size: 9px; font-weight: 700; color: var(--error-color,#e53935); background: color-mix(in srgb,var(--error-color,#e53935) 12%,transparent); padding: 1px 5px; border-radius: 3px; }
      .details { font-size: 11px; color: var(--secondary-text-color); }
      .details summary { cursor: pointer; font-weight: 500; padding: 2px 0; }
      .details[open] summary { margin-bottom: 3px; }

      /* ── Keypad ── */
      .keypad { display: flex; flex-direction: column; gap: 3px; }
      .kp-row { display: flex; gap: 3px; justify-content: center; }
      .kp-key { display: flex; align-items: center; justify-content: center; min-width: 42px; height: 32px; padding: 0 6px; border-radius: 5px; background: var(--secondary-background-color,#f0f0f0); color: var(--primary-text-color); font-size: 12px; font-weight: 600; font-family: var(--cdc-mono); transition: background 0.1s, transform 0.1s; user-select: none; }
      .kp-key.on { background: var(--state-active-color,#43a047); color: #fff; transform: scale(0.92); }
      .kp-spacer { display: inline-block; min-width: 42px; height: 32px; }

      /* ── Actions ── */
      .act-row { display: flex; flex-wrap: wrap; gap: 5px; }
      .act-row ha-button { flex: 1; min-width: 70px; }
      .act-row .red { --ha-button-background-color: var(--error-color,#e53935); --mdc-theme-primary: #fff; }
      .opt-row { display: flex; flex-wrap: wrap; align-items: center; gap: 10px; margin-top: 2px; }
      .toggle { display: flex; align-items: center; gap: 4px; font-size: 11px; color: var(--primary-text-color); }
      .select { display: flex; align-items: center; gap: 3px; font-size: 11px; color: var(--secondary-text-color); }
      .select ha-icon { --mdc-icon-size: 14px; }
      .select select { background: var(--input-background-color,#f0f0f0); border: 1px solid var(--divider-color,rgba(0,0,0,0.12)); border-radius: 3px; padding: 2px 5px; font-size: 11px; color: var(--primary-text-color); }

      /* ── Notify ── */
      .notify-row { display: flex; gap: 5px; }
      .notify-input { flex: 1; padding: 7px 9px; border: 1px solid var(--divider-color,rgba(0,0,0,0.15)); border-radius: 5px; background: var(--input-background-color,#f5f5f5); color: var(--primary-text-color); font-size: 12px; outline: none; transition: border-color 0.15s; }
      .notify-input:focus { border-color: var(--primary-color); }
      .notify-input::placeholder { color: var(--secondary-text-color); opacity: 0.6; }
      .notif-bar { display: flex; align-items: center; gap: 5px; font-size: 11px; color: var(--primary-text-color); background: var(--secondary-background-color,#f0f0f0); padding: 5px 8px; border-radius: 5px; }
      .notif-bar ha-icon { --mdc-icon-size: 13px; color: var(--primary-color); }
      .notif-entry { display: flex; justify-content: space-between; padding: 2px 0 2px 10px; gap: 6px; }
      .notif-msg { flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
      .notif-ts { flex-shrink: 0; font-size: 9px; color: var(--secondary-text-color); opacity: 0.7; }
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
    Object.keys(newConfig).forEach((k) => { if (newConfig[k] === "" || newConfig[k] == null) delete newConfig[k]; });
    const event = new Event("config-changed", { bubbles: true, composed: true });
    event.detail = { config: newConfig };
    this.dispatchEvent(event);
  }

  _entityPicker(key, label, required = false) {
    return html`
      <ha-entity-picker .hass=${this.hass} .value=${this.config[key] || ""} .label=${label} .required=${required}
        @value-changed=${(e) => this._update(key, e)} allow-custom-entity>
      </ha-entity-picker>`;
  }

  render() {
    if (!this.hass) return html``;
    return html`
      <div class="editor">
        ${[
          ["🪪 Status", ["entity_status","entity_badge_version","entity_uptime","entity_refresh_count"]],
          ["🔋 Power", ["entity_battery_level","entity_battery_voltage","entity_charge_current","entity_charge_status","entity_usb_bus_status","entity_battery_runtime"]],
          ["🌐 Network", ["entity_ip","entity_ssid","entity_wifi_signal","entity_wifi_strength"]],
          ["⚙️ Hardware", ["entity_core_temp","entity_motion","entity_sao_gpio1","entity_sao_gpio2"]],
          ["🔒 Secure Element", ["entity_se_alarm","entity_se_chip_mode","entity_se_fw_rv","entity_se_fw_spect","entity_se_serial"]],
          ["🔢 Keypad", ["key_0","key_1","key_2","key_3","key_4","key_5","key_6","key_7","key_8","key_9","key_yes","key_no"]],
          ["🎮 Controls", ["entity_refresh_btn","entity_sleep_btn","entity_reboot_btn","entity_backlight","entity_display_interval"]],
          ["📨 Notify", ["service_notify","entity_notif_msg"]],
        ].map(([title, keys]) => html`
          <div class="sec-e">
            <div class="sec-e-title">${title}</div>
            ${keys.map((k) =>
              k === "service_notify"
                ? html`<ha-textfield .label="Notify service" .value=${this.config[k] || ""} @input=${(e) => this._update(k, e)}></ha-textfield>`
                : this._entityPicker(k, k)
            )}
          </div>`)}
      </div>`;
  }

  static get styles() {
    return css`
      .editor { padding: 12px; display: flex; flex-direction: column; gap: 14px; }
      .sec-e { display: flex; flex-direction: column; gap: 4px; }
      .sec-e-title { font-size: 12px; font-weight: 600; color: var(--primary-text-color); padding-bottom: 2px; border-bottom: 1px solid var(--divider-color); margin-bottom: 2px; }
      ha-entity-picker, ha-textfield { width: 100%; }
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
    if (domain !== "sensor" && domain !== "binary_sensor" && domain !== "switch" && domain !== "light" && domain !== "button" && domain !== "number") return null;
    const state = hass.states[entityId];
    if (!state) return null;
    const name = (state.attributes?.friendly_name || "").toLowerCase();
    if (!name.includes("badge") && !name.includes("cdc") && !entityId.toLowerCase().includes("badge")) return null;
    return { config: { type: "custom:cdc-badge-card" } };
  },
});
