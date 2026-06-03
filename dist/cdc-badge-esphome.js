// CDC Badge — custom Lovelace card
// https://github.com/gretel/cdc-badge-esphome

import { LitElement, html, css } from "https://unpkg.com/lit@3.2.0/index.js?module";

// ── Helpers ──────────────────────────────────────────────────

function fmtUptime(s) {
  if (s == null || isNaN(s)) return "—";
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  return h > 0 ? `${h}h ${m}m` : `${m}m`;
}

function fmtTemp(v) {
  return v != null ? Number(v).toFixed(1) + "°C" : "—";
}

function fmtVoltage(v) {
  return v != null ? Number(v).toFixed(3) + " V" : "—";
}

function fmtCurrent(v) {
  return v != null ? Math.round(v) + " mA" : "—";
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

  // ── Service helpers ───────────────────────────────────────

  _callService(domain, service, data) {
    if (this.hass) this.hass.callService(domain, service, data);
  }

  _toggle(eid) {
    if (eid) this._callService("homeassistant", "toggle", { entity_id: eid });
  }

  _press(eid) {
    if (eid) this._callService("button", "press", { entity_id: eid });
  }

  _setNumber(eid, v) {
    if (eid) this._callService("number", "set_value", { entity_id: eid, value: v });
  }

  _setLight(eid, on) {
    if (eid) this._callService("light", on ? "turn_on" : "turn_off", { entity_id: eid });
  }

  _sendNotify() {
    const el = this.shadowRoot?.querySelector("#cdc-notify-input");
    const msg = el?.value?.trim();
    if (!msg || !this.config.service_notify) return;
    this._callService("esphome", this.config.service_notify.replace(/^esphome\./, ""), { message: msg });
    const ts = new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    this._notifHistory = [{ msg, ts }, ...(this._notifHistory || [])].slice(0, 5);
    this.requestUpdate();
    if (el) el.value = "";
  }

  _notifyKey(e) {
    if (e.key === "Enter") this._sendNotify();
  }

  // ── State helpers ─────────────────────────────────────────

  _num(key) {
    const eid = this.config[key];
    const s = eid ? this.hass?.states[eid] : undefined;
    return s ? Number(s.state) : undefined;
  }

  _str(key) {
    const eid = this.config[key];
    const s = eid ? this.hass?.states[eid] : undefined;
    return s ? s.state : undefined;
  }

  _on(key) {
    const eid = this.config[key];
    const s = eid ? this.hass?.states[eid] : undefined;
    return s ? (s.state === "on" || s.state === "true") : false;
  }

  // ── Render helpers ────────────────────────────────────────

  // Render an entity-row-like line: icon + label + value
  _row(icon, label, value, valueClass = "") {
    return html`
      <div class="row">
        <ha-icon icon=${icon} class="row-icon"></ha-icon>
        <span class="row-label">${label}</span>
        <span class="row-value ${valueClass}">${value}</span>
      </div>`;
  }

  // Section with title and rows
  _section(icon, title, rows) {
    return html`
      <div class="section">
        <div class="section-title">
          <ha-icon icon=${icon}></ha-icon>
          ${title}
        </div>
        ${rows}
      </div>`;
  }

  // ── Render ────────────────────────────────────────────────

  render() {
    if (!this.hass || !this.config) return html``;

    const $ = this.hass.states;
    const on = (k) => this._on(k);
    const num = (k) => this._num(k);
    const str = (k) => this._str(k);

    // Entity config shorthand
    const eid = (k) => this.config[k];
    const connected = (k) => eid(k) && $[eid(k)];

    // Resolve values
    const battPct = num("entity_battery_level");
    const battV = num("entity_battery_voltage");
    const battI = num("entity_charge_current");
    const chargeStr = str("entity_charge_status") || "";
    const usbStr = str("entity_usb_bus_status") || "";
    const runtimeH = num("entity_battery_runtime");
    const ipStr = str("entity_ip") || "—";
    const ssidStr = str("entity_ssid") || "—";
    const wifiDbm = num("entity_wifi_signal");
    const tempC = num("entity_core_temp");
    const uptimeS = num("entity_uptime");
    const verStr = str("entity_badge_version") || "";
    const notifStr = str("entity_notif_msg") || "";
    const seMode = str("entity_se_chip_mode") || "—";
    const seSerial = str("entity_se_serial") || "";
    const seFwRv = str("entity_se_fw_rv");
    const seFwSp = str("entity_se_fw_spect");
    const intervalVal = num("entity_display_interval") || 60;

    const online = $[eid("entity_status")]?.state === "on" || $[eid("entity_status")]?.state === "home";
    const charging = chargeStr === "Charging" || chargeStr === "Pre-charging";
    const seAlarm = on("entity_se_alarm");
    const motionOn = on("entity_motion");
    const sao1On = on("entity_sao_gpio1");
    const sao2On = on("entity_sao_gpio2");
    const backlightOn = on("entity_backlight");
    const hasKeypad = connected("key_0") || connected("key_yes");

    // Charge status display
    const chgMap = { "Charging": "Charging", "Charging Done": "Full", "Discharging": "Discharging", "Not Charging": "Off", "Pre-charging": "Pre-charge" };
    const chgLabel = chgMap[chargeStr] || chargeStr || "—";
    const chgClass = chargeStr === "Charging" || chargeStr === "Pre-charging" ? "green" : chargeStr === "Charging Done" ? "blue" : "dim";

    const usbLabel = usbStr.startsWith("USB-") ? usbStr.slice(4) : usbStr === "None" ? "—" : usbStr || "—";

    // Battery gauge
    const battColor = charging ? "var(--state-active-color,#43a047)" : battPct != null && battPct < 20 ? "var(--error-color,#e53935)" : "var(--primary-color)";
    const battPctStr = battPct != null ? Math.round(battPct) + "%" : "—";
    const iconBatt = charging ? "mdi:lightning-bolt" : battPct != null && battPct < 20 ? "mdi:battery-outline" : "mdi:battery";

    // WiFi
    const wifiBar = wifiDbm != null ? ((wifiDbm + 100) * 2).toFixed(0) + "%" : "—";
    const wifiIcon = wifiDbm != null ? (wifiDbm > -60 ? "mdi:wifi-strength-4" : wifiDbm > -70 ? "mdi:wifi-strength-3" : wifiDbm > -80 ? "mdi:wifi-strength-2" : "mdi:wifi-strength-1") : "mdi:wifi-off";

    return html`
      <ha-card header="CDC Badge${verStr ? "  •  v" + verStr : ""}">

        <div class="card-content">

          <!-- POWER -->
          ${this._section("mdi:power-plug", "Power", html`
            <div class="batt-row">
              <ha-icon icon=${iconBatt} class="batt-icon" style="color:${battColor}"></ha-icon>
              <span class="batt-pct">${battPctStr}</span>
              <div class="batt-bar"><div class="batt-fill" style="width:${battPct != null ? Math.round(battPct) : 0}%;background:${battColor}"></div></div>
              ${runtimeH != null ? html`<span class="batt-runtime">≈${runtimeH.toFixed(1)}h</span>` : ""}
            </div>
            ${this._row("mdi:battery-charging-50", "Voltage", fmtVoltage(battV))}
            ${this._row("mdi:current-ac", "Current", fmtCurrent(battI))}
            ${this._row("mdi:power-cycle", "Charge", html`<span class="${chgClass}">${chgLabel}</span>`)}
            ${this._row("mdi:usb-port", "USB", usbLabel)}
          `)}

          <!-- NETWORK -->
          ${connected("entity_ip") || connected("entity_ssid") || connected("entity_wifi_signal")
            ? this._section("mdi:wifi", "Network", html`
                ${connected("entity_ip") ? this._row("mdi:ip-network", "IP", ipStr) : ""}
                ${connected("entity_ssid") ? this._row("mdi:access-point", "SSID", ssidStr || "—") : ""}
                ${connected("entity_wifi_signal") ? this._row(wifiIcon, "Signal", wifiDbm != null ? `${Math.round(wifiDbm)} dBm` : "—") : ""}
              `)
            : ""}

          <!-- HARDWARE -->
          ${connected("entity_core_temp") || connected("entity_motion") || connected("entity_uptime")
            ? this._section("mdi:chip", "Hardware", html`
                ${connected("entity_core_temp") ? this._row("mdi:thermometer", "Temperature", fmtTemp(tempC)) : ""}
                ${connected("entity_motion") ? this._row("mdi:motion-sensor", "Motion", html`<span class="${motionOn ? "green" : "dim"}">${motionOn ? "Active" : "Idle"}</span>`) : ""}
                ${connected("entity_uptime") ? this._row("mdi:clock-outline", "Uptime", fmtUptime(uptimeS)) : ""}
                ${connected("entity_sao_gpio1") || connected("entity_sao_gpio2")
                  ? html`
                      <div class="row">
                        <ha-icon icon="mdi:expansion-card" class="row-icon"></ha-icon>
                        <span class="row-label">SAO</span>
                        <span class="row-value sao-group">
                          ${connected("entity_sao_gpio1") ? html`<button class="saobtn ${sao1On ? "on" : ""}" @click=${() => this._toggle(eid("entity_sao_gpio1"))}>IO1 ${sao1On ? "ON" : "OFF"}</button>` : ""}
                          ${connected("entity_sao_gpio2") ? html`<button class="saobtn ${sao2On ? "on" : ""}" @click=${() => this._toggle(eid("entity_sao_gpio2"))}>IO2 ${sao2On ? "ON" : "OFF"}</button>` : ""}
                        </span>
                      </div>`
                  : ""}
              `)
            : ""}

          <!-- SECURE ELEMENT -->
          ${connected("entity_se_chip_mode") || connected("entity_se_alarm")
            ? this._section("mdi:shield-lock", "Secure Element", html`
                ${connected("entity_se_alarm")
                  ? this._row("mdi:alert-circle-check", "Tamper", html`
                      <span class="${seAlarm ? "red" : "green"}">${seAlarm ? "⚠ Detected" : "✓ Clear"}${seAlarm ? html` <span class="tamper-badge">TAMPER</span>` : ""}</span>`)
                  : ""}
                ${connected("entity_se_chip_mode") ? this._row("mdi:chip", "Mode", html`<span class="mono">${seMode}</span>`) : ""}
                ${connected("entity_se_serial") ? this._row("mdi:qrcode", "Serial", html`<span class="mono sm">${seSerial}</span>`) : ""}
                ${seFwRv || seFwSp
                  ? html`
                      <details class="fw-details">
                        <summary>Firmware</summary>
                        ${seFwRv ? this._row("mdi:code-tags", "RISC-V", html`<span class="mono sm">${seFwRv}</span>`) : ""}
                        ${seFwSp ? this._row("mdi:code-tags", "SPECT", html`<span class="mono sm">${seFwSp}</span>`) : ""}
                      </details>`
                  : ""}
              `)
            : ""}

          <!-- KEYPAD -->
          ${hasKeypad ? this._section("mdi:keyboard", "Keypad", this._renderKeys()) : ""}

          <!-- ACTIONS -->
          ${connected("entity_refresh_btn") || connected("entity_sleep_btn") || connected("entity_reboot_btn") || connected("entity_backlight") || connected("entity_display_interval")
            ? this._section("mdi:lightning-bolt", "Actions", html`
                <div class="actions">
                  ${connected("entity_refresh_btn") ? html`<ha-button unelevated @click=${() => this._press(eid("entity_refresh_btn"))}><ha-icon icon="mdi:refresh" slot="icon"></ha-icon> Refresh</ha-button>` : ""}
                  ${connected("entity_sleep_btn") ? html`<ha-button unelevated @click=${() => this._press(eid("entity_sleep_btn"))}><ha-icon icon="mdi:sleep" slot="icon"></ha-icon> Sleep</ha-button>` : ""}
                  ${connected("entity_reboot_btn") ? html`<ha-button unelevated class="red-btn" @click=${() => this._press(eid("entity_reboot_btn"))}><ha-icon icon="mdi:restart" slot="icon"></ha-icon> Restart</ha-button>` : ""}
                </div>
                <div class="action-options">
                  ${connected("entity_backlight") ? html`<label><ha-switch ?checked=${backlightOn} @change=${(e) => this._setLight(eid("entity_backlight"), e.target.checked)}></ha-switch> Backlight</label>` : ""}
                  ${connected("entity_display_interval") ? html`<label><ha-icon icon="mdi:timer-outline"></ha-icon> <select @change=${(e) => this._setNumber(eid("entity_display_interval"), +e.target.value)}>${[15,30,60,120,300,600].map((v) => html`<option value=${v} ?selected=${intervalVal===v}>${v}s</option>`)}</select></label>` : ""}
                </div>
              `)
            : ""}

          <!-- NOTIFY -->
          ${this.config.service_notify
            ? this._section("mdi:message-text-outline", "Send to Display", html`
                <div class="notify-row">
                  <input id="cdc-notify-input" class="notify-input" type="text" placeholder="Type a message…" @keydown=${this._notifyKey} />
                  <ha-button unelevated @click=${this._sendNotify}><ha-icon icon="mdi:send" slot="icon"></ha-icon></ha-button>
                </div>
                ${notifStr ? html`<div class="notif-msg"><ha-icon icon="mdi:message-text"></ha-icon> ${notifStr}</div>` : ""}
                ${this._notifHistory?.length
                  ? html`<details class="fw-details"><summary>History (${this._notifHistory.length})</summary>${this._notifHistory.map((h) => html`<div class="history-row"><span>${h.msg}</span><span class="ts">${h.ts}</span></div>`)}</details>`
                  : ""}
              `)
            : ""}

        </div>
      </ha-card>`;
  }

  // ── Keypad rendering ──────────────────────────────────────

  _renderKeys() {
    const keys = [["key_0","key_1","key_2","key_yes"], ["key_3","key_4","key_5"], ["key_6","key_7","key_8","key_no"], [null,"key_9"]];
    return html`
      <div class="keypad">
        ${keys.map((row) => html`
          <div class="kp-row">${row.map((k) =>
            k ? html`<span class="kp-key ${this._on(k) ? "on" : ""}">${k.replace("key_","")}</span>`
              : html`<span class="kp-spacer"></span>`
          )}</div>`)}
      </div>`;
  }

  // ── Styles ─────────────────────────────────────────────────

  static get styles() {
    return css`
      :host { --cdc-mono: "SF Mono","Fira Code","Cascadia Code",monospace; }

      .card-content { padding: 16px; display: flex; flex-direction: column; gap: 8px; }

      /* ── Section ── */
      .section { display: flex; flex-direction: column; gap: 2px; }
      .section-title {
        display: flex; align-items: center; gap: 6px;
        font-size: 13px; font-weight: 500; color: var(--primary-text-color);
        padding: 4px 0; margin-top: 4px;
        border-bottom: 1px solid var(--divider-color,rgba(0,0,0,0.08));
      }
      .section-title ha-icon { --mdc-icon-size: 16px; color: var(--primary-color); }

      /* ── Entity row ── */
      .row {
        display: flex; align-items: center; gap: 8px;
        min-height: 36px; padding: 0 4px;
        border-bottom: 1px solid var(--divider-color,rgba(0,0,0,0.04));
      }
      .row:last-child { border-bottom: none; }
      .row-icon { --mdc-icon-size: 16px; color: var(--primary-color); flex-shrink: 0; }
      .row-label {
        flex: 1; font-size: 13px; color: var(--primary-text-color);
        overflow: hidden; text-overflow: ellipsis; white-space: nowrap;
      }
      .row-value {
        font-size: 13px; font-weight: 500; color: var(--primary-text-color);
        text-align: right; white-space: nowrap; flex-shrink: 0;
      }

      /* ── Battery row ── */
      .batt-row { display: flex; align-items: center; gap: 10px; padding: 6px 4px; }
      .batt-icon { --mdc-icon-size: 28px; }
      .batt-pct { font-size: 24px; font-weight: 700; min-width: 52px; }
      .batt-bar { flex: 1; height: 10px; background: var(--secondary-background-color,#e8e8e8); border-radius: 5px; overflow: hidden; }
      .batt-fill { height: 100%; border-radius: 5px; transition: width 0.3s; }
      .batt-runtime { font-size: 11px; color: var(--secondary-text-color); white-space: nowrap; }

      /* ── Value modifiers ── */
      .green { color: var(--state-active-color,#43a047); }
      .red { color: var(--error-color,#e53935); font-weight: 600; }
      .blue { color: var(--accent-color,#2196f3); }
      .dim { color: var(--secondary-text-color); }
      .mono { font-family: var(--cdc-mono); }
      .sm { font-size: 11px; }
      .tamper-badge {
        font-size: 9px; font-weight: 700; color: var(--error-color,#e53935);
        background: color-mix(in srgb,var(--error-color,#e53935) 12%,transparent);
        padding: 1px 5px; border-radius: 3px; margin-left: 4px;
      }

      /* ── SAO ── */
      .sao-group { display: flex; gap: 4px; }
      .saobtn {
        padding: 3px 8px; border: 1px solid var(--divider-color,rgba(0,0,0,0.12)); border-radius: 4px;
        background: var(--card-background-color,#fff); color: var(--primary-text-color);
        font-size: 11px; font-weight: 500; cursor: pointer;
      }
      .saobtn.on { border-color: var(--state-active-color,#43a047); color: var(--state-active-color,#43a047); }

      /* ── Keypad ── */
      .keypad { display: flex; flex-direction: column; gap: 3px; padding: 4px 0; }
      .kp-row { display: flex; gap: 3px; justify-content: center; }
      .kp-key {
        display: flex; align-items: center; justify-content: center;
        min-width: 40px; height: 30px; border-radius: 4px;
        background: var(--secondary-background-color,#f0f0f0);
        color: var(--primary-text-color); font-size: 12px; font-weight: 600;
        font-family: var(--cdc-mono); transition: background 0.1s;
        user-select: none;
      }
      .kp-key.on { background: var(--state-active-color,#43a047); color: #fff; }
      .kp-spacer { display: inline-block; min-width: 40px; height: 30px; }

      /* ── Actions ── */
      .actions { display: flex; flex-wrap: wrap; gap: 4px; padding: 4px 0; }
      .actions ha-button { flex: 1; min-width: 70px; }
      .red-btn { --ha-button-background-color: var(--error-color,#e53935); --mdc-theme-primary: #fff; }
      .action-options { display: flex; flex-wrap: wrap; align-items: center; gap: 12px; padding: 2px 0; }
      .action-options label { display: flex; align-items: center; gap: 4px; font-size: 12px; color: var(--primary-text-color); cursor: pointer; }
      .action-options select { background: var(--input-background-color,#f0f0f0); border: 1px solid var(--divider-color); border-radius: 3px; padding: 2px 5px; font-size: 11px; color: var(--primary-text-color); }
      .action-options ha-icon { --mdc-icon-size: 14px; color: var(--secondary-text-color); }

      /* ── Notify ── */
      .notify-row { display: flex; gap: 4px; padding: 4px 0; }
      .notify-input { flex: 1; padding: 6px 8px; border: 1px solid var(--divider-color,rgba(0,0,0,0.15)); border-radius: 4px; background: var(--input-background-color,#f5f5f5); color: var(--primary-text-color); font-size: 13px; outline: none; }
      .notify-input:focus { border-color: var(--primary-color); }
      .notify-input::placeholder { color: var(--secondary-text-color); opacity: 0.6; }
      .notif-msg { display: flex; align-items: center; gap: 6px; font-size: 12px; color: var(--primary-text-color); background: var(--secondary-background-color); padding: 6px 8px; border-radius: 4px; }
      .notif-msg ha-icon { --mdc-icon-size: 14px; color: var(--primary-color); }
      .fw-details { font-size: 12px; color: var(--secondary-text-color); padding: 2px 0; }
      .fw-details summary { cursor: pointer; font-weight: 500; padding: 4px 0; }
      .history-row { display: flex; justify-content: space-between; padding: 2px 0 2px 8px; gap: 8px; }
      .history-row .ts { font-size: 10px; color: var(--secondary-text-color); opacity: 0.7; flex-shrink: 0; }
    `;
  }
}

// ── Editor ───────────────────────────────────────────────────

class CdcBadgeCardEditor extends LitElement {
  static get properties() { return { hass: {}, config: {} }; }

  setConfig(config) { this.config = config || {}; }

  _update(key, ev) {
    const val = ev.detail?.value ?? ev.target?.value;
    const newConfig = { ...this.config, [key]: val };
    Object.keys(newConfig).forEach((k) => { if (newConfig[k] === "" || newConfig[k] == null) delete newConfig[k]; });
    const event = new Event("config-changed", { bubbles: true, composed: true });
    event.detail = { config: newConfig };
    this.dispatchEvent(event);
  }

  _picker(key, label) {
    return html`<ha-entity-picker .hass=${this.hass} .value=${this.config[key]||""} .label=${label} @value-changed=${(e) => this._update(key,e)} allow-custom-entity></ha-entity-picker>`;
  }

  render() {
    if (!this.hass) return html``;
    const groups = [
      ["Status", ["entity_status","entity_badge_version","entity_uptime","entity_refresh_count"]],
      ["Power", ["entity_battery_level","entity_battery_voltage","entity_charge_current","entity_charge_status","entity_usb_bus_status","entity_battery_runtime"]],
      ["Network", ["entity_ip","entity_ssid","entity_wifi_signal","entity_wifi_strength"]],
      ["Hardware", ["entity_core_temp","entity_motion","entity_sao_gpio1","entity_sao_gpio2"]],
      ["Secure Element", ["entity_se_alarm","entity_se_chip_mode","entity_se_fw_rv","entity_se_fw_spect","entity_se_serial"]],
      ["Keypad", ["key_0","key_1","key_2","key_3","key_4","key_5","key_6","key_7","key_8","key_9","key_yes","key_no"]],
      ["Controls", ["entity_refresh_btn","entity_sleep_btn","entity_reboot_btn","entity_backlight","entity_display_interval"]],
      ["Notify", ["service_notify","entity_notif_msg"]],
    ];
    return html`<div class="editor">${groups.map(([title, keys]) => html`
      <div class="eg"><div class="eg-title">${title}</div>
      ${keys.map((k) => k === "service_notify"
        ? html`<ha-textfield .label="Notify service" .value=${this.config[k]||""} @input=${(e) => this._update(k,e)}></ha-textfield>`
        : this._picker(k, k)
      )}</div>`)}</div>`;
  }

  static get styles() {
    return css`
      .editor { padding: 12px; display: flex; flex-direction: column; gap: 12px; }
      .eg { display: flex; flex-direction: column; gap: 4px; }
      .eg-title { font-size: 12px; font-weight: 600; color: var(--primary-text-color); padding-bottom: 2px; border-bottom: 1px solid var(--divider-color); margin-bottom: 2px; }
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
    if (!["sensor","binary_sensor","switch","light","button","number"].includes(domain)) return null;
    const s = hass.states[entityId];
    if (!s) return null;
    const n = (s.attributes?.friendly_name || "").toLowerCase();
    if (!n.includes("badge") && !n.includes("cdc") && !entityId.toLowerCase().includes("badge")) return null;
    return { config: { type: "custom:cdc-badge-card" } };
  },
});
