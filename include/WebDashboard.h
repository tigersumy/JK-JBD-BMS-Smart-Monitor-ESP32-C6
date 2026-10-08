#pragma once

#include <Arduino.h>

// Favicon SVG stored in PROGMEM (SVG lightning badge with gradient)
static const char FAVICON_SVG[] PROGMEM = 
  "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'>"
  "<defs><linearGradient id='g' x1='0%' y1='0%' x2='100%' y2='100%'>"
  "<stop offset='0%' stop-color='#1f6feb'/><stop offset='100%' stop-color='#238636'/>"
  "</linearGradient></defs>"
  "<rect width='100' height='100' rx='22' fill='url(#g)'/>"
  "<path d='M54 14 L24 54 L48 54 L44 86 L76 44 L52 44 Z' fill='#ffffff'/>"
  "</svg>";

// Web UI HTML Templates stored in PROGMEM

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Universal BMS Smart Monitor (ESP32-C6)</title>
  <link rel="icon" type="image/svg+xml" href="/favicon.svg">
  <link rel="alternate icon" href="/favicon.ico">
  <style>
    :root {
      --bg-color: #0d1117;
      --card-bg: rgba(22, 27, 34, 0.85);
      --card-border: rgba(56, 139, 253, 0.2);
      --text-main: #f0f6fc;
      --text-muted: #8b949e;
      --accent-blue: #58a6ff;
      --accent-green: #3fb950;
      --accent-orange: #d29922;
      --accent-red: #f85149;
      --accent-cyan: #39c5bb;
      --accent-purple: #a371f7;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }
    body { background: var(--bg-color); color: var(--text-main); min-height: 100vh; padding: 16px; }
    .container { max-width: 920px; margin: 0 auto; display: flex; flex-direction: column; gap: 16px; }
    
    /* Header */
    .header {
      display: flex; justify-content: space-between; align-items: center;
      background: var(--card-bg); border: 1px solid var(--card-border);
      border-radius: 14px; padding: 16px 20px; backdrop-filter: blur(10px);
      flex-wrap: wrap; gap: 12px;
    }
    .header-title { display: flex; align-items: center; gap: 12px; }
    .header-logo { width: 42px; height: 42px; border-radius: 10px; background: linear-gradient(135deg, #1f6feb, #238636); display: flex; align-items: center; justify-content: center; font-size: 22px; font-weight: bold; }
    .header-title h1 { font-size: 1.25rem; font-weight: 700; }
    .header-title p { font-size: 0.8rem; color: var(--text-muted); }
    .header-actions { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; }
    .status-badge { display: flex; align-items: center; gap: 8px; font-size: 0.85rem; padding: 6px 12px; border-radius: 20px; background: rgba(255,255,255,0.05); }
    .dot { width: 10px; height: 10px; border-radius: 50%; background: var(--accent-red); transition: all 0.3s; }
    .dot.online { background: var(--accent-green); box-shadow: 0 0 10px var(--accent-green); }
    .btn-settings { text-decoration: none; color: var(--text-main); background: rgba(56,139,253,0.15); border: 1px solid rgba(56,139,253,0.3); padding: 8px 14px; border-radius: 8px; font-size: 0.85rem; cursor: pointer; transition: 0.2s; }
    .btn-settings:hover { background: rgba(56,139,253,0.3); }
    .btn-lang { background: rgba(56,139,253,0.12); border: 1px solid rgba(56,139,253,0.3); color: var(--text-main); padding: 6px 12px; border-radius: 8px; font-size: 0.85rem; font-weight: 600; cursor: pointer; transition: 0.2s; }
    .btn-lang:hover { background: rgba(56,139,253,0.25); border-color: var(--accent-blue); }
    .bms-tag { font-size: 0.75rem; padding: 3px 8px; border-radius: 6px; font-weight: 600; background: rgba(163,113,247,0.2); color: var(--accent-purple); border: 1px solid rgba(163,113,247,0.4); }

    /* Quick KPI Grid */
    .kpi-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(190px, 1fr)); gap: 12px; }
    .kpi-card { background: var(--card-bg); border: 1px solid var(--card-border); border-radius: 12px; padding: 16px; display: flex; flex-direction: column; gap: 6px; }
    .kpi-label { font-size: 0.8rem; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.5px; }
    .kpi-val { font-size: 1.7rem; font-weight: 700; }
    .val-green { color: var(--accent-green); }
    .val-blue { color: var(--accent-blue); }
    .val-orange { color: var(--accent-orange); }
    .val-cyan { color: var(--accent-cyan); }
    .val-red { color: var(--accent-red); }

    /* Card Panels */
    .card { background: var(--card-bg); border: 1px solid var(--card-border); border-radius: 14px; padding: 20px; }
    .card-title { font-size: 1.1rem; font-weight: 600; margin-bottom: 16px; display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 8px; }
    
    /* Cells Grid */
    .cells-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(125px, 1fr)); gap: 10px; }
    .cell-box { background: rgba(13, 17, 23, 0.7); border: 1px solid rgba(255,255,255,0.08); border-radius: 10px; padding: 12px; text-align: center; position: relative; overflow: hidden; }
    .cell-box.min-cell { border-color: var(--accent-blue); box-shadow: inset 0 0 8px rgba(88,166,255,0.2); }
    .cell-box.max-cell { border-color: var(--accent-green); box-shadow: inset 0 0 8px rgba(63,185,80,0.2); }
    .cell-tag { position: absolute; top: 5px; right: 6px; font-size: 0.65rem; font-weight: 700; padding: 2px 5px; border-radius: 4px; text-transform: uppercase; }
    .tag-min { background: rgba(88,166,255,0.2); color: var(--accent-blue); }
    .tag-max { background: rgba(63,185,80,0.2); color: var(--accent-green); }
    .cell-num { font-size: 0.75rem; color: var(--text-muted); margin-bottom: 4px; }
    .cell-volts { font-size: 1.22rem; font-weight: 700; color: #fff; margin-bottom: 6px; }
    .cell-bar-bg { width: 100%; height: 6px; background: rgba(255,255,255,0.1); border-radius: 3px; overflow: hidden; }
    .cell-bar-fill { height: 100%; width: 50%; background: var(--accent-green); transition: width 0.4s ease; }

    /* Delta info */
    .delta-row { display: flex; justify-content: space-between; align-items: center; margin-top: 14px; padding-top: 12px; border-top: 1px solid rgba(255,255,255,0.08); font-size: 0.9rem; }
    .delta-badge { font-weight: 700; padding: 4px 10px; border-radius: 6px; }

    /* Parameters table */
    .param-list { display: flex; flex-direction: column; gap: 10px; }
    .param-row { display: flex; justify-content: space-between; align-items: center; font-size: 0.92rem; padding: 6px 0; border-bottom: 1px solid rgba(255,255,255,0.04); }
    .param-name { color: var(--text-muted); }
    .param-val { font-weight: 600; }

    /* Switches */
    .switches-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 12px; margin-top: 10px; }
    .switch-card { background: rgba(13, 17, 23, 0.7); border: 1px solid rgba(255,255,255,0.08); border-radius: 10px; padding: 14px; display: flex; justify-content: space-between; align-items: center; }
    .switch-info h4 { font-size: 0.95rem; font-weight: 600; }
    .switch-info p { font-size: 0.75rem; color: var(--text-muted); }
    
    /* Toggle switch CSS */
    .toggle { position: relative; display: inline-block; width: 48px; height: 26px; }
    .toggle input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #30363d; transition: .3s; border-radius: 26px; }
    .slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 3px; bottom: 3px; background-color: white; transition: .3s; border-radius: 50%; }
    input:checked + .slider { background-color: var(--accent-green); }
    input:checked + .slider:before { transform: translateX(22px); }
  </style>
</head>
<body>
  <div class="container">
    <!-- Header -->
    <header class="header">
      <div class="header-title">
        <div class="header-logo">⚡</div>
        <div>
          <h1>Universal BMS Monitor</h1>
          <p id="mac-label" data-i18n="sub">ESP32-C6 BLE Gateway (JK / JBD)</p>
        </div>
      </div>
      <div class="header-actions">
        <span class="bms-tag" id="bms-type-badge">Auto / Unknown</span>
        <button type="button" class="btn-lang" id="btn-lang" onclick="toggleLang()">🌐 EN</button>
        <div style="display:flex; align-items:center; gap:6px;">
          <select id="cell-select" onchange="changeCells(this.value)" style="background:rgba(22,27,34,0.9); color:var(--accent-blue); border:1px solid var(--card-border); padding:6px 10px; border-radius:8px; font-size:0.85rem; font-weight:600; cursor:pointer; outline:none;">
            <option value="4">4S (12V)</option>
            <option value="8">8S (24V)</option>
            <option value="16">16S (48V)</option>
            <option value="24">24S (72V)</option>
          </select>
        </div>
        <div class="status-badge">
          <div class="dot" id="status-dot"></div>
          <span id="status-text">Connecting...</span>
        </div>
        <a href="/setup" class="btn-settings" data-i18n="options">⚙️ Setup</a>
      </div>
    </header>

    <!-- Quick KPIs -->
    <section class="kpi-grid">
      <div class="kpi-card">
        <span class="kpi-label" data-i18n="kpi_tot_v">🔋 Total Voltage</span>
        <span class="kpi-val val-green" id="kpi-total-v">--.- V</span>
      </div>
      <div class="kpi-card">
        <span class="kpi-label" data-i18n="kpi_curr">⚡ Battery Current</span>
        <span class="kpi-val val-blue" id="kpi-current">--.- A</span>
      </div>
      <div class="kpi-card">
        <span class="kpi-label" data-i18n="kpi_soc">📊 State of Charge (SOC)</span>
        <span class="kpi-val val-cyan" id="kpi-soc">-- %</span>
      </div>
      <div class="kpi-card">
        <span class="kpi-label" data-i18n="kpi_pwr">💡 Power</span>
        <span class="kpi-val val-orange" id="kpi-power">-- W</span>
      </div>
    </section>

    <!-- Battery Cells (Dynamic 4S/8S/16S/24S) -->
    <section class="card">
      <div class="card-title">
        <span id="cells-title">🔋 Cell Voltages</span>
        <span style="font-size:0.8rem; font-weight:normal; color:var(--text-muted);" id="balancer-status-label">Balancer: Idle</span>
      </div>
      <div class="cells-grid" id="cells-grid"></div>
      <div class="delta-row">
        <span data-i18n="delta_label">⚖️ Cell Voltage Delta (&Delta;V):</span>
        <span class="delta-badge" id="delta-val">-- mV</span>
      </div>
    </section>

    <!-- Switches Control -->
    <section class="card">
      <div class="card-title" data-i18n="sw_title">🛡️ Protection & Balancing Control</div>
      <div class="switches-grid">
        <div class="switch-card">
          <div class="switch-info">
            <h4 data-i18n="sw_charge_h">🔌 Charge (Charge MOS)</h4>
            <p data-i18n="sw_charge_p">Allow battery charging</p>
          </div>
          <label class="toggle">
            <input type="checkbox" id="sw-charge" onchange="toggleSw('charging', this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="switch-card">
          <div class="switch-info">
            <h4 data-i18n="sw_disch_h">💡 Discharge (Discharge MOS)</h4>
            <p data-i18n="sw_disch_p">Allow load power supply</p>
          </div>
          <label class="toggle">
            <input type="checkbox" id="sw-discharge" onchange="toggleSw('discharging', this.checked)">
            <span class="slider"></span>
          </label>
        </div>
        <div class="switch-card" id="card-balancer">
          <div class="switch-info">
            <h4 id="lbl-bal-title" data-i18n="sw_bal_h">⚖️ Active Balancer</h4>
            <p id="lbl-bal-desc" data-i18n="sw_bal_p">Cell voltage equalization</p>
          </div>
          <label class="toggle" id="toggle-bal-container">
            <input type="checkbox" id="sw-balancer" onchange="toggleSw('balancer', this.checked)">
            <span class="slider"></span>
          </label>
        </div>
      </div>
    </section>

    <!-- Detailed Stats -->
    <section class="card">
      <div class="card-title" data-i18n="stats_title">📈 Detailed Battery Statistics</div>
      <div class="param-list">
        <div class="param-row">
          <span class="param-name" data-i18n="stat_bms_model">🏷️ Connected Device</span>
          <span class="param-val" id="val-device-name">--</span>
        </div>
        <div class="param-row">
          <span class="param-name" data-i18n="stat_temp_mos">🌡️ Power MOSFET Temperature</span>
          <span class="param-val" id="val-temp-mos">-- °C</span>
        </div>
        <div class="param-row">
          <span class="param-name" data-i18n="stat_temp_1">🌡️ Battery Temp Sensor 1</span>
          <span class="param-val" id="val-temp-1">-- °C</span>
        </div>
        <div class="param-row">
          <span class="param-name" data-i18n="stat_temp_2">🌡️ Battery Temp Sensor 2</span>
          <span class="param-val" id="val-temp-2">-- °C</span>
        </div>
        <div class="param-row">
          <span class="param-name" data-i18n="stat_rem_ah">🔋 Remaining Capacity</span>
          <span class="param-val val-cyan" id="val-remain-ah">-- Ah</span>
        </div>
        <div class="param-row">
          <span class="param-name" data-i18n="stat_nom_ah">⚡ Nominal Pack Capacity</span>
          <span class="param-val" id="val-nom-ah">-- Ah</span>
        </div>
        <div class="param-row">
          <span class="param-name" data-i18n="stat_bal_curr">🔄 Balancer Current / State</span>
          <span class="param-val val-orange" id="val-bal-curr">--</span>
        </div>
        <div class="param-row">
          <span class="param-name" data-i18n="stat_cycles">🔄 Full Cycles Count</span>
          <span class="param-val" id="val-cycles">--</span>
        </div>
        <div class="param-row">
          <span class="param-name" data-i18n="stat_errors">⚠️ Status / Alarms</span>
          <span class="param-val val-green" id="val-errors">OK</span>
        </div>
        <div class="param-row">
          <span class="param-name" data-i18n="stat_heap">🧠 ESP32-C6 Free Heap</span>
          <span class="param-val val-blue" id="val-heap">-- KB</span>
        </div>
        <div class="param-row">
          <span class="param-name" data-i18n="stat_wifi_rssi">📶 Wi-Fi Signal</span>
          <span class="param-val" id="val-wifi-rssi">-- dBm</span>
        </div>
      </div>
    </section>
  </div>

  <script>
    const i18n = {
      en: {
        sub: "ESP32-C6 BLE Gateway (JK / JBD)",
        options: "⚙️ Setup",
        kpi_tot_v: "🔋 Total Voltage",
        kpi_curr: "⚡ Battery Current",
        kpi_soc: "📊 State of Charge (SOC)",
        kpi_pwr: "💡 Power",
        cells_title: "🔋 Cell Voltages",
        cell_prefix: "Cell",
        delta_label: "⚖️ Cell Voltage Delta (ΔV):",
        sw_title: "🛡️ Protection & Balancing Control",
        sw_charge_h: "🔌 Charge (Charge MOS)",
        sw_charge_p: "Allow battery charging",
        sw_disch_h: "💡 Discharge (Discharge MOS)",
        sw_disch_p: "Allow load power supply",
        sw_bal_h: "⚖️ Active Balancer",
        sw_bal_p: "Cell voltage equalization",
        stats_title: "📈 Detailed Battery Statistics",
        stat_bms_model: "🏷️ Connected Device",
        stat_temp_mos: "🌡️ Power MOSFET Temperature",
        stat_temp_1: "🌡️ Battery Temp Sensor 1",
        stat_temp_2: "🌡️ Battery Temp Sensor 2",
        stat_rem_ah: "🔋 Remaining Capacity",
        stat_nom_ah: "⚡ Nominal Pack Capacity",
        stat_bal_curr: "🔄 Balancer Current / State",
        stat_cycles: "🔄 Full Cycles Count",
        stat_errors: "⚠️ Status / Alarms",
        stat_heap: "🧠 ESP32-C6 Free Heap",
        stat_wifi_rssi: "📶 Wi-Fi Signal",
        st_connecting: "Connecting...",
        st_connected: "BLE Connected",
        st_searching: "Searching BMS...",
        st_server_err: "Server Unreachable",
        bal_idle: "Idle",
        bal_active: "Balancing Active",
        bal_prefix: "Balancer: "
      },
      uk: {
        sub: "ESP32-C6 BLE Шлюз (JK / JBD)",
        options: "⚙️ Налаштування",
        kpi_tot_v: "🔋 Загальна напруга",
        kpi_curr: "⚡ Струм АКБ",
        kpi_soc: "📊 Рівень заряду (SOC)",
        kpi_pwr: "💡 Потужність",
        cells_title: "🔋 Напруги осередків",
        cell_prefix: "Осередок",
        delta_label: "⚖️ Дельта напруг комірок (ΔV):",
        sw_title: "🛡️ Керування захистом та балансуванням",
        sw_charge_h: "🔌 Заряд (Charge MOS)",
        sw_charge_p: "Дозвіл зарядки АКБ",
        sw_disch_h: "💡 Розряд (Discharge MOS)",
        sw_disch_p: "Дозвіл живлення навантаження",
        sw_bal_h: "⚖️ Активний балансир",
        sw_bal_p: "Вирівнювання комірок",
        stats_title: "📈 Детальна статистика батареї",
        stat_bms_model: "🏷️ Підключений пристрій",
        stat_temp_mos: "🌡️ Температура MOSFET",
        stat_temp_1: "🌡️ Датчик температури АКБ 1",
        stat_temp_2: "🌡️ Датчик температури АКБ 2",
        stat_rem_ah: "🔋 Залишкова ємність",
        stat_nom_ah: "⚡ Номінальна ємність",
        stat_bal_curr: "🔄 Стан / Струм балансира",
        stat_cycles: "🔄 Кількість повних циклів",
        stat_errors: "⚠️ Статус / Помилки BMS",
        stat_heap: "🧠 Вільна пам'ять (Heap)",
        stat_wifi_rssi: "📶 Рівень сигналу Wi-Fi",
        st_connecting: "Підключення...",
        st_connected: "BLE Підключено",
        st_searching: "Пошук BMS...",
        st_server_err: "Сервер недоступний",
        bal_idle: "Очікування",
        bal_active: "Балансування активне",
        bal_prefix: "Балансир: "
      }
    };

    let currentLang = localStorage.getItem('lang') || 'uk';
    let updatingSw = false;
    let lastSwitchActionTime = {
      charging: 0,
      discharging: 0,
      balancer: 0
    };
    let userSelectedCells = null;
    let renderedCellCount = 0;
    let lastData = null;

    function t(key) {
      return (i18n[currentLang] && i18n[currentLang][key]) ? i18n[currentLang][key] : key;
    }

    function setLang(lang) {
      currentLang = lang;
      localStorage.setItem('lang', lang);
      document.querySelectorAll('[data-i18n]').forEach(el => {
        const k = el.getAttribute('data-i18n');
        if (i18n[lang] && i18n[lang][k]) el.textContent = i18n[lang][k];
      });
      const btn = document.getElementById('btn-lang');
      if (btn) btn.textContent = (lang === 'en') ? '🌐 EN' : '🌐 UA';
      if (lastData) {
        renderedCellCount = 0;
        updateUi(lastData);
      }
    }

    function toggleLang() {
      setLang(currentLang === 'en' ? 'uk' : 'en');
    }

    function renderCellBoxes(count) {
      const grid = document.getElementById('cells-grid');
      if (!grid) return;
      grid.innerHTML = '';
      for (let i = 1; i <= count; i++) {
        const div = document.createElement('div');
        div.className = 'cell-box';
        div.id = 'box-cell-' + i;
        div.innerHTML = `
          <span class="cell-tag" id="tag-cell-${i}"></span>
          <div class="cell-num">${t('cell_prefix')} ${i}</div>
          <div class="cell-volts" id="volts-cell-${i}">-.--- V</div>
          <div class="cell-bar-bg"><div class="cell-bar-fill" id="bar-cell-${i}"></div></div>
        `;
        grid.appendChild(div);
      }
      renderedCellCount = count;
    }

    function updateUi(data) {
      lastData = data;
      const dot = document.getElementById('status-dot');
      const stText = document.getElementById('status-text');
      const bmsTag = document.getElementById('bms-type-badge');
      
      if (data.connected) {
        dot.className = 'dot online';
        stText.textContent = t('st_connected');
        bmsTag.textContent = data.bms_type || 'Connected';
      } else {
        dot.className = 'dot';
        stText.textContent = t('st_searching');
        bmsTag.textContent = data.bms_type || 'Auto / Unknown';
      }

      document.getElementById('kpi-total-v').textContent = (data.total_voltage || 0.0).toFixed(2) + ' V';
      const curEl = document.getElementById('kpi-current');
      const cur = data.current || 0.0;
      curEl.textContent = (cur > 0 ? '+' : '') + cur.toFixed(2) + ' A';
      curEl.className = 'kpi-val ' + (cur > 0.1 ? 'val-green' : (cur < -0.1 ? 'val-orange' : 'val-blue'));

      document.getElementById('kpi-soc').textContent = Math.round(data.soc || 0) + ' %';
      document.getElementById('kpi-power').textContent = Math.round(data.power || 0) + ' W';

      // Dynamic Cells
      const cellCount = userSelectedCells || data.cell_count || (data.cells ? data.cells.length : 8);
      if (renderedCellCount !== cellCount) {
        renderCellBoxes(cellCount);
      }
      const titleEl = document.getElementById('cells-title');
      if (titleEl) titleEl.textContent = `${t('cells_title')} (${cellCount}S LiFePO4)`;
      const sel = document.getElementById('cell-select');
      if (sel && String(sel.value) !== String(cellCount)) sel.value = cellCount;

      const vMin = 2.8, vMax = 3.65;
      for (let i = 1; i <= cellCount; i++) {
        const v = (data.cells && data.cells[i - 1] !== undefined) ? data.cells[i - 1] : 0.0;
        const vEl = document.getElementById('volts-cell-' + i);
        const barEl = document.getElementById('bar-cell-' + i);
        const boxEl = document.getElementById('box-cell-' + i);
        const tagEl = document.getElementById('tag-cell-' + i);

        if (vEl) vEl.textContent = (v > 0.1) ? (v.toFixed(3) + ' V') : '-.--- V';
        if (barEl) {
          const pct = (v > 0.1) ? Math.max(0, Math.min(100, ((v - vMin) / (vMax - vMin)) * 100)) : 0;
          barEl.style.width = pct + '%';
        }

        if (boxEl && tagEl) {
          boxEl.className = 'cell-box';
          tagEl.className = 'cell-tag';
          tagEl.textContent = '';

          if (i === data.min_cell_idx && data.min_cell_idx > 0 && i <= cellCount) {
            boxEl.classList.add('min-cell');
            tagEl.classList.add('tag-min');
            tagEl.textContent = 'MIN';
          } else if (i === data.max_cell_idx && data.max_cell_idx > 0 && i <= cellCount) {
            boxEl.classList.add('max-cell');
            tagEl.classList.add('tag-max');
            tagEl.textContent = 'MAX';
          }
        }
      }

      // Delta
      const dMv = Math.round((data.delta_cell_v || 0.0) * 1000);
      const dEl = document.getElementById('delta-val');
      dEl.textContent = dMv + ' mV';
      if (dMv <= 15) {
        dEl.style.background = 'rgba(63,185,80,0.2)';
        dEl.style.color = 'var(--accent-green)';
      } else if (dMv <= 35) {
        dEl.style.background = 'rgba(210,153,34,0.2)';
        dEl.style.color = 'var(--accent-orange)';
      } else {
        dEl.style.background = 'rgba(248,81,73,0.2)';
        dEl.style.color = 'var(--accent-red)';
      }

      let balStr = t('bal_idle');
      if (data.balancing_active) {
        balStr = t('bal_active') + (data.balancing_current > 0 ? ` (${data.balancing_current.toFixed(2)} A)` : '');
      }
      document.getElementById('balancer-status-label').textContent = t('bal_prefix') + balStr;

      // Detailed Stats
      document.getElementById('val-device-name').textContent = (data.name || data.mac || 'N/A') + ` [${data.bms_type || 'Unknown'}]`;
      document.getElementById('val-temp-mos').textContent = (data.temp_mos || 0.0).toFixed(1) + ' °C';
      document.getElementById('val-temp-1').textContent = (data.temp_sensor1 || 0.0).toFixed(1) + ' °C';
      document.getElementById('val-temp-2').textContent = (data.temp_sensor2 || 0.0).toFixed(1) + ' °C';
      document.getElementById('val-remain-ah').textContent = (data.capacity_remain || 0.0).toFixed(1) + ' Ah';
      document.getElementById('val-nom-ah').textContent = (data.capacity_nominal || 0.0).toFixed(1) + ' Ah';
      document.getElementById('val-bal-curr').textContent = data.balancing_active ? (data.balancing_current > 0 ? (data.balancing_current.toFixed(2) + ' A') : 'Active') : 'Idle';
      document.getElementById('val-cycles').textContent = data.cycle_count || 0;
      
      const errEl = document.getElementById('val-errors');
      errEl.textContent = data.errors || 'OK';
      errEl.className = 'param-val ' + (data.errors && data.errors !== 'OK' ? 'val-orange' : 'val-green');

      if (data.heap_free !== undefined) {
        document.getElementById('val-heap').textContent = data.heap_free + ' KB';
      }
      if (data.wifi_rssi !== undefined) {
        document.getElementById('val-wifi-rssi').textContent = data.wifi_rssi + ' dBm';
      }

      // Update switches ONLY if user is not currently toggling
      const nowTs = Date.now();
      const swCharge = document.getElementById('sw-charge');
      const swDischarge = document.getElementById('sw-discharge');
      const swBalancer = document.getElementById('sw-balancer');

      if (!updatingSw) {
        if (swCharge && (nowTs - lastSwitchActionTime.charging > 2500)) {
          swCharge.checked = !!data.switch_charging;
        }
        if (swDischarge && (nowTs - lastSwitchActionTime.discharging > 2500)) {
          swDischarge.checked = !!data.switch_discharging;
        }
        if (swBalancer && (nowTs - lastSwitchActionTime.balancer > 2500)) {
          swBalancer.checked = !!data.switch_balancer;
        }
      }

      // Dynamic adaptation for JBD (passive auto) vs JK (active manual)
      const isJbd = (data.bms_type === 'JBD-BMS');
      const balTitle = document.getElementById('lbl-bal-title');
      const balDesc = document.getElementById('lbl-bal-desc');

      if (balTitle && balDesc && swBalancer) {
        if (isJbd) {
          balTitle.textContent = (currentLang === 'uk') ? '⚖️ Балансування (Пасивне)' : '⚖️ Balancing (Passive)';
          balDesc.textContent = data.balancing_active ? 
            ((currentLang === 'uk') ? '🟢 Балансування активне' : '🟢 Equalizing Active') :
            ((currentLang === 'uk') ? 'Автоматично за порогом V' : 'Automatic V threshold');
          swBalancer.disabled = true;
        } else {
          balTitle.textContent = (currentLang === 'uk') ? '⚖️ Активний балансир' : '⚖️ Active Balancer';
          balDesc.textContent = (currentLang === 'uk') ? 'Вирівнювання комірок' : 'Cell voltage equalization';
          swBalancer.disabled = false;
        }
      }
    }

    async function fetchData() {
      try {
        const res = await fetch('/api/data');
        if (res.ok) {
          const json = await res.json();
          updateUi(json);
        }
      } catch (e) {
        document.getElementById('status-dot').className = 'dot';
        document.getElementById('status-text').textContent = t('st_server_err');
      } finally {
        setTimeout(fetchData, 1200);
      }
    }

    async function toggleSw(swType, state) {
      if (swType === 'balancer' && lastData && lastData.bms_type === 'JBD-BMS') {
        return; // JBD BMS passive balancer is managed by BMS hardware thresholds automatically
      }
      updatingSw = true;
      lastSwitchActionTime[swType] = Date.now();
      try {
        const res = await fetch('/api/switch', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ switch: swType, state: state })
        });
        const respJson = await res.json();
        if (respJson && respJson.status === 'ok') {
          if (lastData) {
            if (swType === 'charging') lastData.switch_charging = state;
            if (swType === 'discharging') lastData.switch_discharging = state;
            if (swType === 'balancer') lastData.switch_balancer = state;
          }
        }
      } catch(e) {
        console.error(e);
      }
      setTimeout(() => { 
        updatingSw = false; 
      }, 1000);
    }

    async function changeCells(num) {
      const val = parseInt(num);
      userSelectedCells = val;
      renderCellBoxes(val);
      const titleEl = document.getElementById('cells-title');
      if (titleEl) titleEl.textContent = `${t('cells_title')} (${val}S LiFePO4)`;
      if (lastData) updateUi(lastData);
      try {
        await fetch('/api/set-cells', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ cells: val })
        });
      } catch (e) {
        console.error(e);
      }
    }

    setLang(currentLang);
    fetchData();
  </script>
</body>
</html>
)rawliteral";

static const char SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Universal BMS Gateway Setup</title>
  <link rel="icon" type="image/svg+xml" href="/favicon.svg">
  <link rel="alternate icon" href="/favicon.ico">
  <style>
    :root {
      --bg: #0d1117; --card-bg: #161b22; --border: rgba(56, 139, 253, 0.2);
      --text: #f0f6fc; --text-mut: #8b949e; --accent: #58a6ff; --green: #3fb950;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 20px; display: flex; justify-content: center; }
    .card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 14px; max-width: 540px; width: 100%; padding: 24px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }
    h2 { font-size: 1.3rem; margin-bottom: 8px; display: flex; align-items: center; gap: 10px; }
    p.sub { font-size: 0.85rem; color: var(--text-mut); margin-bottom: 20px; line-height: 1.4; }
    .field-group { margin-bottom: 18px; }
    label { display: block; font-size: 0.85rem; font-weight: 600; margin-bottom: 6px; color: var(--text); }
    .input-row { display: flex; gap: 8px; }
    input[type="text"], input[type="password"], select {
      width: 100%; background: #0d1117; border: 1px solid #30363d; border-radius: 8px;
      padding: 10px 14px; color: #fff; font-size: 0.95rem; outline: none; transition: 0.2s;
    }
    input:focus, select:focus { border-color: var(--accent); }
    .btn { background: #21262d; border: 1px solid #363b42; color: #fff; padding: 10px 14px; border-radius: 8px; cursor: pointer; font-size: 0.9rem; font-weight: 500; display: inline-flex; align-items: center; justify-content: center; gap: 6px; transition: 0.2s; white-space: nowrap; }
    .btn:hover { background: #30363d; }
    .btn-lang { background: rgba(56,139,253,0.12); border: 1px solid rgba(56,139,253,0.3); color: var(--text); padding: 6px 12px; border-radius: 8px; font-size: 0.85rem; font-weight: 600; cursor: pointer; transition: 0.2s; }
    .btn-lang:hover { background: rgba(56,139,253,0.25); border-color: var(--accent); }
    .btn-primary { background: #238636; border-color: rgba(240,246,252,0.1); width: 100%; padding: 12px; font-size: 1rem; margin-top: 10px; }
    .btn-primary:hover { background: #2ea043; }
    .device-list { margin-top: 8px; display: flex; flex-direction: column; gap: 6px; max-height: 180px; overflow-y: auto; }
    .device-item { background: #0d1117; border: 1px solid #30363d; padding: 8px 12px; border-radius: 6px; font-size: 0.85rem; display: flex; justify-content: space-between; align-items: center; cursor: pointer; }
    .device-item:hover { border-color: var(--accent); background: rgba(56,139,253,0.1); }
    .badge { font-size: 0.75rem; padding: 2px 6px; border-radius: 4px; background: rgba(88,166,255,0.15); color: var(--accent); }
    .alert { padding: 12px; border-radius: 8px; font-size: 0.85rem; margin-top: 16px; display: none; text-align: center; }
    .alert-success { background: rgba(63,185,80,0.15); border: 1px solid var(--green); color: var(--green); }
  </style>
</head>
<body>
  <div class="card">
    <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
      <h2 data-i18n="setup_title">⚡ Universal BMS Setup</h2>
      <button type="button" class="btn-lang" id="btn-lang" onclick="toggleLang()">🌐 EN</button>
    </div>
    <p class="sub" data-i18n="setup_sub">Configure Wi-Fi connection and select your Bluetooth BMS (JK-BMS or JBD-BMS).</p>

    <form id="setup-form">
      <!-- Wi-Fi SSID -->
      <div class="field-group">
        <label data-i18n="wifi_ssid_lbl">Wi-Fi Network Name (SSID):</label>
        <div class="input-row">
          <input type="text" id="wifi-ssid" placeholder="Enter or select from list" required>
          <button type="button" class="btn" id="btn-scan-wifi" onclick="scanWifi()" data-i18n="btn_scan_wifi">🔄 Scan</button>
        </div>
        <select id="wifi-select" style="display:none; margin-top:6px;" onchange="selectWifi(this.value)">
          <option value="" data-i18n="wifi_sel_default">-- Discovered Wi-Fi Networks --</option>
        </select>
      </div>

      <!-- Wi-Fi Password -->
      <div class="field-group">
        <label data-i18n="wifi_pass_lbl">Wi-Fi Password:</label>
        <input type="password" id="wifi-pass" placeholder="Enter Wi-Fi password">
      </div>

      <!-- Protocol Type -->
      <div class="field-group">
        <label data-i18n="bms_type_lbl">BMS Protocol / Type:</label>
        <select id="bms-type">
          <option value="0" selected data-i18n="opt_type_auto">Auto Detect (JK / JBD)</option>
          <option value="1">JK-BMS (0xFFE0 / TLV)</option>
          <option value="2">JBD / Xiaoxiang (0xFF00 / 0xA5)</option>
        </select>
      </div>

      <!-- BMS Bluetooth MAC -->
      <div class="field-group">
        <label data-i18n="bms_mac_lbl">BMS Bluetooth MAC Address:</label>
        <div class="input-row">
          <input type="text" id="bms-mac" placeholder="AA:BB:CC:DD:EE:FF" required pattern="^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$">
          <button type="button" class="btn" id="btn-scan-ble" onclick="scanBle()" data-i18n="btn_scan_ble">🔍 Find BMS</button>
        </div>
        <div class="device-list" id="ble-list"></div>
      </div>

      <!-- BMS PIN -->
      <div class="field-group">
        <label data-i18n="bms_pin_lbl">BMS Control PIN (for JK-BMS):</label>
        <input type="text" id="bms-pin" value="1234" placeholder="1234 or 123456">
      </div>

      <!-- Battery Cell Configuration -->
      <div class="field-group">
        <label data-i18n="cells_lbl">Battery Configuration (Cell count):</label>
        <select id="cell-count">
          <option value="4" data-i18n="opt_4s">4S (12V Pack - 4 cells)</option>
          <option value="8" selected data-i18n="opt_8s">8S (24V Pack - 8 cells)</option>
          <option value="16" data-i18n="opt_16s">16S (48V Pack - 16 cells)</option>
          <option value="24" data-i18n="opt_24s">24S (72V Pack - 24 cells)</option>
        </select>
      </div>

      <button type="submit" class="btn btn-primary" id="btn-submit" data-i18n="btn_save">💾 Save & Connect</button>
    </form>

    <div class="alert alert-success" id="alert-box" data-i18n="alert_saved">
      ✅ Configuration saved! ESP32-C6 is rebooting and connecting to your Wi-Fi. Access dashboard at <b>http://bms-monitor.local</b>
    </div>
  </div>

  <script>
    const i18n = {
      en: {
        setup_title: "⚡ Universal BMS Setup",
        setup_sub: "Configure Wi-Fi connection and select your Bluetooth BMS (JK-BMS or JBD-BMS).",
        wifi_ssid_lbl: "Wi-Fi Network Name (SSID):",
        btn_scan_wifi: "🔄 Scan",
        wifi_sel_default: "-- Discovered Wi-Fi Networks --",
        wifi_pass_lbl: "Wi-Fi Password:",
        bms_type_lbl: "BMS Protocol / Type:",
        opt_type_auto: "Auto Detect (JK / JBD)",
        bms_mac_lbl: "BMS Bluetooth MAC Address:",
        btn_scan_ble: "🔍 Find BMS",
        ble_scan_prog: "Scanning BLE radio (~5 sec)...",
        ble_not_found: "No supported BMS found nearby. Enter MAC manually.",
        ble_scan_err: "BLE scan error.",
        bms_pin_lbl: "BMS Control PIN (for JK-BMS):",
        cells_lbl: "Battery Configuration (Cell count):",
        opt_4s: "4S (12V Pack - 4 cells)",
        opt_8s: "8S (24V Pack - 8 cells)",
        opt_16s: "16S (48V Pack - 16 cells)",
        opt_24s: "24S (72V Pack - 24 cells)",
        btn_save: "💾 Save & Connect",
        btn_saving: "Saving...",
        btn_scanning: "⏳ Scanning...",
        alert_saved: "✅ Configuration saved! ESP32-C6 is applying settings and connecting to Wi-Fi. Dashboard: http://bms-monitor.local"
      },
      uk: {
        setup_title: "⚡ Налаштування Universal BMS",
        setup_sub: "Вкажіть параметри підключення до домашнього Wi-Fi та виберіть Bluetooth MAC-адресу вашої BMS (JK або JBD).",
        wifi_ssid_lbl: "Ім'я Wi-Fi мережі (SSID):",
        btn_scan_wifi: "🔄 Сканувати",
        wifi_sel_default: "-- Знайдені Wi-Fi мережі --",
        wifi_pass_lbl: "Пароль Wi-Fi мережі:",
        bms_type_lbl: "Тип / Протокол BMS:",
        opt_type_auto: "Автовизначення (JK / JBD)",
        bms_mac_lbl: "Bluetooth MAC-адреса BMS:",
        btn_scan_ble: "🔍 Пошук BMS",
        ble_scan_prog: "Сканування BLE ефіру (~5 сек)...",
        ble_not_found: "BMS не знайдено поруч. Введіть MAC вручну.",
        ble_scan_err: "Помилка сканування BLE.",
        bms_pin_lbl: "PIN-код керування (для JK-BMS):",
        cells_lbl: "Конфігурація батареї (кількість комірок):",
        opt_4s: "4S (12V АКБ - 4 осередки)",
        opt_8s: "8S (24V АКБ - 8 осередків)",
        opt_16s: "16S (48V АКБ - 16 осередків)",
        opt_24s: "24S (72V АКБ - 24 осередки)",
        btn_save: "💾 Зберегти та підключитися",
        btn_saving: "Збереження...",
        btn_scanning: "⏳ Пошук...",
        alert_saved: "✅ Налаштування збережено! Пристрій застосовує конфігурацію та підключається до Wi-Fi. Панель: http://bms-monitor.local"
      }
    };

    let currentLang = localStorage.getItem('lang') || 'uk';

    function t(key) {
      return (i18n[currentLang] && i18n[currentLang][key]) ? i18n[currentLang][key] : key;
    }

    function setLang(lang) {
      currentLang = lang;
      localStorage.setItem('lang', lang);
      document.querySelectorAll('[data-i18n]').forEach(el => {
        const k = el.getAttribute('data-i18n');
        if (i18n[lang] && i18n[lang][k]) el.textContent = i18n[lang][k];
      });
      const btn = document.getElementById('btn-lang');
      if (btn) btn.textContent = (lang === 'en') ? '🌐 EN' : '🌐 UA';
    }

    function toggleLang() {
      setLang(currentLang === 'en' ? 'uk' : 'en');
    }

    async function scanWifi() {
      const btn = document.getElementById('btn-scan-wifi');
      btn.textContent = t('btn_scanning');
      btn.disabled = true;
      try {
        const res = await fetch('/api/scan-wifi');
        const list = await res.json();
        const sel = document.getElementById('wifi-select');
        sel.innerHTML = `<option value="">${t('wifi_sel_default')}</option>`;
        list.forEach(net => {
          const opt = document.createElement('option');
          opt.value = net.ssid;
          opt.textContent = `${net.ssid} (${net.rssi} dBm)`;
          sel.appendChild(opt);
        });
        sel.style.display = 'block';
      } catch(e) {
        alert('Scan error: ' + e);
      }
      btn.textContent = t('btn_scan_wifi');
      btn.disabled = false;
    }

    function selectWifi(val) {
      if (val) document.getElementById('wifi-ssid').value = val;
    }

    async function scanBle() {
      const btn = document.getElementById('btn-scan-ble');
      const listEl = document.getElementById('ble-list');
      btn.textContent = t('btn_scanning');
      btn.disabled = true;
      listEl.innerHTML = `<p style="color:var(--text-mut); font-size:0.8rem;">${t('ble_scan_prog')}</p>`;
      try {
        const res = await fetch('/api/scan-ble');
        const list = await res.json();
        listEl.innerHTML = '';
        if (list.length === 0) {
          listEl.innerHTML = `<p style="color:var(--text-mut); font-size:0.8rem;">${t('ble_not_found')}</p>`;
        } else {
          list.forEach(item => {
            const div = document.createElement('div');
            div.className = 'device-item';
            div.innerHTML = `<span><b>${item.name || 'BMS Device'}</b> <span class="badge">${item.type}</span><br><small style="color:var(--text-mut);">${item.mac}</small></span><span>${item.rssi} dBm</span>`;
            div.onclick = () => { 
              document.getElementById('bms-mac').value = item.mac;
              if (item.type === 'JK-BMS') document.getElementById('bms-type').value = '1';
              else if (item.type === 'JBD-BMS') document.getElementById('bms-type').value = '2';
            };
            listEl.appendChild(div);
          });
        }
      } catch(e) {
        listEl.innerHTML = `<p style="color:var(--text-mut); font-size:0.8rem;">${t('ble_scan_err')}</p>`;
      }
      btn.textContent = t('btn_scan_ble');
      btn.disabled = false;
    }

    document.getElementById('setup-form').onsubmit = async (e) => {
      e.preventDefault();
      const payload = {
        ssid: document.getElementById('wifi-ssid').value.trim(),
        pass: document.getElementById('wifi-pass').value.trim(),
        mac: document.getElementById('bms-mac').value.trim(),
        type: parseInt(document.getElementById('bms-type').value),
        pin: document.getElementById('bms-pin').value.trim(),
        cells: parseInt(document.getElementById('cell-count').value)
      };

      const btnSub = document.getElementById('btn-submit');
      btnSub.disabled = true;
      btnSub.textContent = t('btn_saving');

      try {
        const res = await fetch('/api/save-config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });
        if (res.ok) {
          document.getElementById('setup-form').style.display = 'none';
          document.getElementById('alert-box').style.display = 'block';
          setTimeout(() => { window.location.href = '/'; }, 3500);
        }
      } catch(err) {
        alert('Save error: ' + err);
        btnSub.disabled = false;
        btnSub.textContent = t('btn_save');
      }
    };

    async function loadCurrentConfig() {
      try {
        const res = await fetch('/api/config');
        if (res.ok) {
          const cfg = await res.json();
          if (cfg.ssid) document.getElementById('wifi-ssid').value = cfg.ssid;
          if (cfg.mac) document.getElementById('bms-mac').value = cfg.mac;
          if (cfg.type !== undefined) document.getElementById('bms-type').value = cfg.type;
          if (cfg.pin) document.getElementById('bms-pin').value = cfg.pin;
          if (cfg.cells) document.getElementById('cell-count').value = cfg.cells;
        }
      } catch(e) {}
    }

    setLang(currentLang);
    loadCurrentConfig();
  </script>
</body>
</html>
)rawliteral";
