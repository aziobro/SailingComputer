#pragma once

// Returns the web UI HTML as a C string (stored in flash / rodata).
inline const char* getWebUI() {
    static const char ui[] = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Sailing Computer</title>
<style>
  :root {
    color-scheme: dark;
    --bg: #071426;
    --panel: #0d2244;
    --panel-deep: #081a34;
    --line: #24508d;
    --muted: #8ca8c1;
    --blue: #67c5ff;
    --green: #4fd39a;
    --amber: #ffbb45;
    --red: #ff6b6b;
    --nav-height: 68px;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  html { -webkit-text-size-adjust: 100%; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif;
    background: var(--bg);
    color: #edf5fc;
    min-height: 100vh;
    padding-bottom: calc(var(--nav-height) + env(safe-area-inset-bottom));
  }
  button, input, select { font: inherit; touch-action: manipulation; }
  button { -webkit-tap-highlight-color: transparent; }
  header {
    position: sticky;
    top: 0;
    z-index: 30;
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    min-height: 58px;
    padding: calc(8px + env(safe-area-inset-top)) 14px 8px;
    background: rgba(9, 28, 54, 0.97);
    border-bottom: 1px solid var(--line);
    box-shadow: 0 4px 14px rgba(0, 0, 0, 0.25);
  }
  .app-name { color: #7d9bb7; font-size: 0.68rem; font-weight: 700; letter-spacing: 0.08em; text-transform: uppercase; }
  header h1 { color: #edf5fc; font-size: 1.25rem; line-height: 1.15; }
  .header-status { display: flex; gap: 6px; align-items: center; }
  .status-pill {
    display: inline-flex;
    align-items: center;
    min-height: 30px;
    padding: 4px 8px;
    border: 1px solid #254568;
    border-radius: 999px;
    background: #08182c;
    color: #9bb3c9;
    font-size: 0.7rem;
    font-weight: 800;
    white-space: nowrap;
  }
  .status-pill.ok { border-color: #287d60; color: #65e0aa; }
  .status-pill.rtk { border-color: #2f79a8; color: #74ceff; }
  .status-pill.warn { border-color: #8a6424; color: #ffd078; }
  .status-pill.err { border-color: #7a3535; color: #ff8585; }

  nav {
    position: fixed;
    left: 0;
    right: 0;
    bottom: 0;
    z-index: 50;
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    min-height: var(--nav-height);
    padding-bottom: env(safe-area-inset-bottom);
    background: rgba(8, 25, 47, 0.98);
    border-top: 1px solid var(--line);
    box-shadow: 0 -6px 18px rgba(0, 0, 0, 0.35);
  }
  nav button {
    min-width: 0;
    min-height: var(--nav-height);
    padding: 8px 4px;
    background: none;
    border: none;
    border-top: 3px solid transparent;
    color: #8da6bd;
    cursor: pointer;
    font-size: 0.76rem;
    font-weight: 800;
    letter-spacing: 0.04em;
    text-transform: uppercase;
  }
  nav button.active { color: var(--blue); background: #0e294d; border-top-color: var(--blue); }
  .more-backdrop {
    display: none;
    position: fixed;
    inset: 0;
    z-index: 55;
    background: rgba(0, 0, 0, 0.55);
  }
  .more-backdrop.open { display: block; }
  .more-menu {
    position: fixed;
    left: 10px;
    right: 10px;
    bottom: calc(var(--nav-height) + env(safe-area-inset-bottom) + 8px);
    z-index: 60;
    display: none;
    padding: 10px;
    border: 1px solid #315a8b;
    border-radius: 14px;
    background: #0c2343;
    box-shadow: 0 14px 40px rgba(0, 0, 0, 0.5);
  }
  .more-menu.open { display: block; }
  .more-menu button {
    width: 100%;
    min-height: 50px;
    padding: 10px 12px;
    border: 0;
    border-bottom: 1px solid #1b3b61;
    background: transparent;
    color: #e5f0f8;
    text-align: left;
    font-weight: 700;
  }
  .more-menu button:last-of-type { border-bottom: 0; }
  .more-menu .danger-link { color: #ff8585; }
  .device-meta { padding: 8px 12px 4px; color: #7895af; font-size: 0.74rem; }

  .page { display: none; padding: 12px 12px 28px; max-width: 700px; margin: 0 auto; }
  .page.active { display: block; }

  .card { background: var(--panel); border: 1px solid #234b80; border-radius: 12px; padding: 14px; margin-bottom: 12px; }
  .card h2 { font-size: 0.78rem; text-transform: uppercase; letter-spacing: 1px; color: var(--blue); margin-bottom: 12px; }
  .card-heading-row { display: flex; justify-content: space-between; align-items: center; gap: 10px; margin-bottom: 12px; }
  .card-heading-row h2 { margin: 0; }
  details.card { padding: 0; }
  details.card > summary {
    min-height: 54px;
    padding: 16px 14px;
    color: var(--blue);
    cursor: pointer;
    font-size: 0.82rem;
    font-weight: 800;
    letter-spacing: 0.08em;
    list-style-position: inside;
    text-transform: uppercase;
  }
  details.card[open] > summary { border-bottom: 1px solid #234b80; }
  .details-body { padding: 14px; }

  .stat-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 10px; }
  .stat { min-width: 0; background: var(--panel-deep); border: 1px solid #173a66; border-radius: 8px; padding: 12px 8px; text-align: center; }
  .stat-label { font-size: 0.68rem; color: #7f9db8; text-transform: uppercase; letter-spacing: 0.5px; }
  .stat-value { font-size: 1.4rem; font-weight: bold; color: #e0e8f0; margin-top: 4px; font-variant-numeric: tabular-nums; }
  .primary-stats .stat-value { font-size: clamp(1.8rem, 9vw, 2.5rem); }
  .stat-value.ok  { color: var(--green); }
  .stat-value.rtk { color: var(--blue); }
  .stat-value.warn { color: var(--amber); }
  .stat-value.err { color: var(--red); }
  .fix-inline { display: flex; align-items: center; gap: 8px; color: #8ca8c1; font-size: 0.75rem; white-space: nowrap; }
  .fix-inline .stat-value { margin: 0; font-size: 0.85rem; }

  label { display: block; margin-bottom: 6px; font-size: 0.84rem; color: #9ab2c7; }
  input, select {
    width: 100%;
    min-height: 48px;
    padding: 10px 12px;
    background: #07182f;
    border: 1px solid #315a8b;
    border-radius: 8px;
    color: #edf5fc;
    font-size: 16px;
    margin-bottom: 14px;
  }
  input:focus, select:focus { outline: 2px solid #4eb7f4; outline-offset: 1px; border-color: #4eb7f4; }
  input[type=range] { min-height: 44px; padding: 0; border: 0; background: transparent; }
  .row { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }

  button.btn, #tracks button:not(.btn) {
    min-height: 48px;
    padding: 11px 16px;
    border: 1px solid #365d88;
    border-radius: 8px;
    background: #173a61;
    color: #edf5fc;
    cursor: pointer;
    font-size: 0.95rem;
    font-weight: 800;
    transition: opacity 0.2s;
  }
  button.btn:hover { opacity: 0.85; }
  button.btn-primary { background: #1676c7; border-color: #2e94e4; color: #fff; }
  button.btn-danger  { background: #8b2929; border-color: #b24747; color: #fff; }
  button:disabled { opacity: 0.45; cursor: default; }

  .toast { display: none; position: fixed; bottom: calc(var(--nav-height) + env(safe-area-inset-bottom) + 14px);
           left: 12px; right: 12px; padding: 12px 16px; border-radius: 8px; font-size: 0.95rem;
           font-weight: 700; text-align: center; z-index: 100; }
  .section-title { font-size: 0.8rem; font-weight: bold; text-transform: uppercase;
                   letter-spacing: 0.5px; color: var(--blue); margin: 16px 0 8px;
                   border-bottom: 1px solid #1e4080; padding-bottom: 4px; }
  .toggle-row { display: flex; align-items: center; min-height: 48px; margin-bottom: 12px; }
  .toggle-row label { margin: 0 0 0 8px; font-size: 0.9rem; color: #e0e8f0; }
  input[type=checkbox] { width: 24px; height: 24px; min-height: 24px; margin: 0; accent-color: #5ab4e8; cursor: pointer; }

  /* Instrument panel */
  .instruments { display: flex; gap: 12px; flex-wrap: wrap; justify-content: center; align-items: flex-start; }
  .instrument  { display: flex; flex-direction: column; align-items: center; gap: 6px; }
  .instrument-label { font-size: 0.7rem; color: #5a7a9a; text-transform: uppercase; letter-spacing: 0.5px; }
  .instr-legend { font-size: 0.72rem; color: #8899aa; margin-top: 2px; }
  .instrument svg { max-width: 100%; height: auto; }

  #start.page.active { display: flex; flex-direction: column; }
  #start .race-sequence-card { order: 1; }
  #start .race-line-card { order: 2; }
  #start .race-course-card { order: 3; }
  #start .crew-control-card { order: 4; }
  .race-actions { display: flex; gap: 10px; }
  .race-actions .btn { min-height: 56px; }
  .crew-control-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 0 12px; }
  .crew-priority-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 0 12px; }
  .crew-control-actions { display: flex; gap: 10px; align-items: stretch; }
  .crew-control-actions .btn { flex: 1; min-height: 54px; }
  #raceClock { font-size: clamp(4.2rem, 20vw, 6rem) !important; }
  #compassCanvas { width: min(220px, 62vw); height: auto; }
  #raceLapSelector .btn { flex: 1; min-width: 48px; padding-left: 8px; padding-right: 8px; }
  #fileList .btn, #markList .btn { min-width: 44px; min-height: 44px; }
  #configForm > .btn { width: 100%; min-height: 58px; }
  #marks .card > button.btn-primary { width: 100%; }
  .firmware-action { width: 100%; min-height: 58px !important; }
  .firmware-version { font-size: 1.05rem; overflow-wrap: anywhere; }
  .progress-track { height: 10px; overflow: hidden; border-radius: 999px; background: #07182f; }
  .progress-fill { height: 100%; width: 0; border-radius: inherit; background: var(--blue); transition: width 0.25s; }
  .race-line-card h2, #tracks .card h2[onclick] { min-height: 44px; padding: 12px 0; }
  #raceCourseSelect, #lineMarkSel0, #lineMarkSel1, #gpxFile { margin-bottom: 0; }
  .mark-row {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    gap: 10px;
    align-items: center;
    min-height: 64px;
    padding: 10px 0;
    border-bottom: 1px solid #1e4080;
  }
  .mark-row:last-child { border-bottom: 0; }
  .mark-name { color: #edf5fc; font-size: 1rem; font-weight: 800; }
  .mark-coords { margin-top: 3px; color: #89a8c1; font-size: 0.76rem; font-variant-numeric: tabular-nums; }
  .course-editor { margin-bottom: 12px; padding: 12px; border: 1px solid #315a8b; border-radius: 10px; background: #081a34; }
  .course-mark-edit { padding: 10px 0; border-bottom: 1px solid #1b3b61; }
  .course-mark-edit:last-child { border-bottom: 0; }
  .course-mark-line { display: grid; grid-template-columns: minmax(0, 1fr) auto; gap: 8px; align-items: center; }
  .course-mark-line select { margin: 0; }
  .course-mark-controls { display: flex; gap: 6px; margin-top: 7px; }
  .course-mark-controls .btn { min-width: 48px; min-height: 44px; padding: 8px 10px; }
  .course-rounding { min-width: 66px !important; }
  .course-list-item { padding: 12px 0; border-bottom: 1px solid #1e4080; }
  .course-list-item:last-child { border-bottom: 0; }
  .course-list-actions { display: flex; gap: 8px; margin-top: 9px; }
  .course-list-actions .btn { flex: 1; }

  @media (max-width: 520px) {
    .row { grid-template-columns: 1fr; gap: 0; }
    .crew-control-grid, .crew-priority-grid { grid-template-columns: 1fr; gap: 0; }
    .crew-control-actions { flex-direction: column; }
    .card { padding: 12px; }
    .page { padding-left: 10px; padding-right: 10px; }
    .instruments { flex-direction: column; align-items: stretch; }
    .instrument { width: 100%; }
    input, select { font-size: 16px !important; }
  }

  @media (min-width: 760px) {
    body { padding-bottom: 0; }
    nav {
      position: sticky;
      top: 58px;
      bottom: auto;
      display: flex;
      justify-content: center;
      min-height: 54px;
      padding-bottom: 0;
      border-top: 0;
      border-bottom: 1px solid var(--line);
      box-shadow: none;
    }
    nav button { flex: 0 1 150px; min-height: 54px; border-top: 0; border-bottom: 3px solid transparent; }
    nav button.active { border-top-color: transparent; border-bottom-color: var(--blue); }
    .more-menu { left: auto; right: calc(50% - 350px); bottom: auto; top: 118px; width: 330px; }
    .page { padding-top: 20px; }
    .stat-grid { grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); }
  }
</style>
</head>
<body>
<header>
  <div>
    <div class="app-name">Sailing Computer</div>
    <h1 id="pageTitle">Race</h1>
  </div>
  <div class="header-status">
    <span id="headerFix" class="status-pill err">GPS --</span>
    <span id="headerNtrip" class="status-pill warn">NTRIP --</span>
  </div>
</header>
<nav aria-label="Primary">
  <button class="active" data-page="start" onclick="showPage('start')">Race</button>
  <button data-page="status" onclick="showPage('status')">Data</button>
  <button data-page="marks" onclick="showPage('marks')">Marks</button>
  <button id="moreNavButton" aria-expanded="false" onclick="toggleMoreMenu()">More</button>
</nav>
<div id="moreBackdrop" class="more-backdrop" onclick="closeMoreMenu()"></div>
<div id="moreMenu" class="more-menu" role="menu">
  <button role="menuitem" onclick="showPage('tracks')">Track recording</button>
  <button role="menuitem" onclick="showPage('files')">Files and storage</button>
  <button role="menuitem" onclick="showPage('config')">Device settings</button>
  <button role="menuitem" onclick="showPage('system')">System and updates</button>
  <button role="menuitem" class="danger-link" onclick="doLogout()">Log out</button>
  <div class="device-meta">Device <span id="deviceIP">--</span> <span id="fwVersion"></span></div>
</div>

<!-- STATUS PAGE -->
<div id="status" class="page">
  <div class="card">
    <div class="card-heading-row">
      <h2>On the Water</h2>
      <div class="fix-inline"><span class="stat-value" id="s-fix">--</span><span>SAT <strong id="s-sats">--</strong></span></div>
    </div>
    <div class="stat-grid primary-stats">
      <div class="stat"><div class="stat-label">Speed (kts)</div><div class="stat-value" id="s-sog">--</div></div>
      <div class="stat"><div class="stat-label">True Heading</div><div class="stat-value" id="s-hdt">--&deg;</div></div>
      <div class="stat"><div class="stat-label">COG</div><div class="stat-value" id="s-cog">--&deg;</div></div>
      <div class="stat"><div class="stat-label">Heel</div><div class="stat-value" id="s-roll">--&deg;</div></div>
    </div>
  </div>
  <div class="card">
    <h2>Navigation Instruments</h2>
    <div class="instruments">

      <!-- Compass rose: heading (blue) + COG (amber) + leeway arc -->
      <div class="instrument">
        <svg id="compass-svg" viewBox="-115 -115 230 230" width="220" height="220">
          <!-- Outer ring -->
          <circle r="112" fill="#091a36" stroke="#1e4080" stroke-width="1.5"/>
          <!-- Tick marks and cardinal labels inserted by JS -->
          <g id="c-ticks"></g>
          <g id="c-labels"></g>
          <!-- Inner reference ring -->
          <circle r="78" fill="none" stroke="#1e3a5f" stroke-width="0.5" stroke-dasharray="2,6"/>
          <!-- Leeway arc -->
          <path id="c-arc" fill="none" stroke-width="4" stroke-linecap="round" opacity="0.75"/>
          <!-- COG pointer (amber arrow) -->
          <g id="c-cog">
            <line x1="0" y1="18" x2="0" y2="-88" stroke="#f59e0b" stroke-width="2"/>
            <polygon points="0,-100 -7,-82 7,-82" fill="#f59e0b"/>
            <line x1="-6" y1="18" x2="6" y2="18" stroke="#f59e0b" stroke-width="1.5"/>
          </g>
          <!-- Heading pointer (boat silhouette, blue) -->
          <g id="c-hdg">
            <polygon points="0,-94 -9,-54 -4,-54 -4,16 4,16 4,-54 9,-54" fill="#4a9eff"/>
            <polygon points="-9,-54 9,-54 12,-38 -12,-38" fill="#2a7fd4" opacity="0.7"/>
          </g>
          <!-- Hub -->
          <circle r="20" fill="#091a36" stroke="#1e4080" stroke-width="1.5"/>
          <text id="c-hdg-num" y="-3" text-anchor="middle" fill="#4a9eff" font-size="9" font-weight="bold">---</text>
          <text id="c-cog-num" y="9"  text-anchor="middle" fill="#f59e0b" font-size="9">---</text>
        </svg>
        <div class="instr-legend">
          <span style="color:#4a9eff">&#9650; Heading</span> &nbsp;
          <span style="color:#f59e0b">&#9650; COG</span>
        </div>
      </div>

      <!-- Heel / roll inclinometer -->
      <div class="instrument">
        <svg id="heel-svg" viewBox="-110 -75 220 150" width="220" height="150">
          <!-- Background -->
          <rect x="-108" y="-73" width="216" height="146" rx="8" fill="#091a36" stroke="#1e4080" stroke-width="1.5"/>
          <!-- Scale arcs and tick marks inserted by JS -->
          <g id="h-scale"></g>
          <!-- Water fill (tilts with heel) -->
          <g id="h-water">
            <clipPath id="hclip">
              <rect x="-108" y="-73" width="216" height="146" rx="8"/>
            </clipPath>
            <rect id="h-water-rect" x="-200" y="10" width="400" height="80"
                  fill="#1e3a5f" opacity="0.45" clip-path="url(#hclip)"/>
          </g>
          <!-- Boat hull cross-section (rotates) -->
          <g id="h-hull">
            <!-- Hull -->
            <polygon points="0,-52 -46,14 -32,26 0,32 32,26 46,14"
                     fill="#1a3a5a" stroke="#4a9eff" stroke-width="1.5"/>
            <!-- Deck line -->
            <line x1="-46" y1="14" x2="46" y2="14" stroke="#5ab4e8" stroke-width="1"/>
            <!-- Keel -->
            <polygon points="-6,32 6,32 4,56 -4,56" fill="#2a5a8f"/>
            <!-- Mast -->
            <line x1="0" y1="-52" x2="0" y2="-66" stroke="#7ab4d8" stroke-width="2"/>
          </g>
          <!-- Center reference line -->
          <line x1="0" y1="-70" x2="0" y2="70" stroke="#1e3060" stroke-width="1" stroke-dasharray="4,6"/>
          <!-- Horizon reference -->
          <line x1="-108" y1="14" x2="108" y2="14" stroke="#4a9eff" stroke-width="0.5" opacity="0.25"/>
          <!-- Roll value -->
          <text id="h-num" y="62" text-anchor="middle" font-size="13" font-weight="bold" fill="#e0e8f0">0.0°</text>
        </svg>
        <div class="instr-legend">Roll / Heel &nbsp; <span style="color:#f59e0b">■</span> &gt;20°</div>
      </div>

    </div>
  </div>

  <div class="card">
    <h2>Sailing Performance</h2>
    <div class="stat-grid">
      <div class="stat">
        <div class="stat-label">Likely Tack</div>
        <div class="stat-value" id="s-tack">--</div>
        <div style="font-size:0.7rem;color:#8899aa;margin-top:2px" id="s-tack-sub">from heel angle</div>
      </div>
      <div class="stat">
        <div class="stat-label">Leeway</div>
        <div class="stat-value" id="s-leeway">--</div>
        <div style="font-size:0.7rem;color:#8899aa;margin-top:2px" id="s-leeway-dir">heading vs track</div>
      </div>
      <div class="stat">
        <div class="stat-label">Lateral Drift</div>
        <div class="stat-value" id="s-lateral">--</div>
        <div style="font-size:0.7rem;color:#8899aa;margin-top:2px" id="s-lateral-dir">sideways speed</div>
      </div>
      <div class="stat">
        <div class="stat-label">Forward Speed</div>
        <div class="stat-value" id="s-drive">--</div>
        <div style="font-size:0.7rem;color:#8899aa;margin-top:2px">fwd speed (kts)</div>
      </div>
    </div>
    <div id="s-favor-row" style="display:none;margin-top:0.6rem;padding:0.5rem 0.7rem;border-radius:6px;background:#0d1f33;font-size:0.8rem">
      <span style="color:#8899aa">Estimated favor: </span>
      <span id="s-favor-val" style="font-weight:bold"></span>
      <span id="s-favor-desc" style="color:#8899aa;margin-left:0.4rem"></span>
    </div>
    <p style="font-size:0.75rem;color:#556677;margin:0.5rem 0 0">
      Leeway = COG &minus; Heading and includes both keel slip and current.
      Tack is estimated from heel (&gt;3&deg;). Favor is estimated from cross-track motion.
    </p>
  </div>
  <details class="card">
    <summary>Position and GNSS details</summary>
    <div class="details-body">
      <div class="stat-grid">
        <div class="stat"><div class="stat-label">Latitude</div><div class="stat-value" id="s-lat" style="font-size:1rem">--</div></div>
        <div class="stat"><div class="stat-label">Longitude</div><div class="stat-value" id="s-lon" style="font-size:1rem">--</div></div>
        <div class="stat"><div class="stat-label">Heading Source</div><div class="stat-value" id="s-hdtsrc" style="font-size:0.9rem">--</div></div>
        <div class="stat"><div class="stat-label">HDOP</div><div class="stat-value" id="s-hdop">--</div></div>
        <div class="stat"><div class="stat-label">Altitude</div><div class="stat-value" id="s-alt">--</div></div>
      </div>
    </div>
  </details>
  <details class="card">
    <summary>Connections and diagnostics</summary>
    <div class="details-body">
      <div class="stat-grid">
        <div class="stat"><div class="stat-label">WiFi Mode</div><div class="stat-value" id="s-wifimode">--</div></div>
        <div class="stat"><div class="stat-label">IP Address</div><div class="stat-value" id="s-ip" style="font-size:0.9rem">--</div></div>
        <div class="stat"><div class="stat-label">NTRIP</div><div class="stat-value" id="s-ntrip">--</div></div>
        <div class="stat"><div class="stat-label">NTRIP Source</div><div class="stat-value" id="s-ntrip-src" style="font-size:0.85rem">--</div></div>
        <div class="stat"><div class="stat-label">RTCM In / UART</div><div class="stat-value" id="s-rtcm">0</div></div>
        <div class="stat"><div class="stat-label">RTCM Frames / Type</div><div class="stat-value" id="s-rtcm-frames">0</div></div>
        <div class="stat"><div class="stat-label">UM982 AUX UART</div><div class="stat-value" id="s-rtcm-uart">--</div></div>
        <div class="stat"><div class="stat-label">NMEA Bytes</div><div class="stat-value" id="s-nmea-bytes">0</div><div style="font-size:0.7rem;color:#8899aa;margin-top:2px" id="s-nmea-lines">-- sentences</div></div>
        <div class="stat" id="s-ble-tile" style="display:none"><div class="stat-label">Bluetooth GPS</div><div class="stat-value" id="s-ble">--</div></div>
      </div>
    </div>
  </details>
</div>

<!-- CONFIG PAGE -->
<div id="config" class="page">
  <div id="configAuthWall" style="display:none" class="card">
    <p style="text-align:center;color:#e85a5a;font-size:1rem;margin:1rem 0">&#128274; Login required</p>
    <p style="text-align:center;color:#7a9ab8;font-size:0.85rem">Click <strong>Log Out</strong> then reload the page, or <button class="btn" style="display:inline;padding:4px 12px" onclick="retryAuth('config')">Try Again</button></p>
  </div>
  <form id="configForm">
    <div class="card">
      <h2>WiFi</h2>
      <div class="toggle-row">
        <input type="checkbox" id="apMode" name="apMode">
        <label for="apMode">Access Point mode (create own WiFi hotspot)</label>
      </div>
      <div id="apFields">
        <p class="section-title">AP Credentials</p>
        <label>AP Network Name (SSID)</label>
        <input type="text" name="apSSID" id="apSSID" maxlength="63">
        <label>AP Password</label>
        <input type="password" name="apPassword" id="apPassword" maxlength="63" placeholder="(unchanged)">
      </div>
      <div id="wifiFields">
        <p class="section-title">Join Existing Network</p>
        <label>Network SSID</label>
        <input type="text" name="wifiSSID" id="wifiSSID" maxlength="63">
        <label>Password</label>
        <input type="password" name="wifiPassword" id="wifiPassword" maxlength="63" placeholder="(unchanged)">
      </div>
      <p class="section-title">Web Admin Password</p>
      <label>Admin Password (username: admin)</label>
      <input type="password" name="adminPassword" id="adminPassword" maxlength="63" placeholder="(unchanged — leave blank to keep current)">
      <label>Confirm Admin Password</label>
      <input type="password" name="adminPassword2" id="adminPassword2" maxlength="63" placeholder="(unchanged — leave blank to keep current)">
    </div>
    <div class="card">
      <h2>Antenna</h2>
      <label>Heading Offset (&deg;)</label>
      <input type="number" name="headingOffset" id="headingOffset" value="90" min="-360" max="360" step="0.1">
      <p style="font-size:0.8rem;color:#8899aa;margin:0.25rem 0 0.75rem">Degrees added to UM982 heading. Default 90&deg; for port/starboard aft-rail mounting (ANT1=stbd, ANT2=port). Set 0 for fore/aft mounting.</p>
      <label>COG Minimum SOG (knots)</label>
      <input type="number" name="cogMinSog" id="cogMinSog" value="0.1" min="0" max="5" step="0.1">
      <p style="font-size:0.8rem;color:#8899aa;margin:0.25rem 0 0.75rem">COG is frozen below this speed to suppress GPS position noise. Default 0.1 kts. Increase if COG is still unstable at rest.</p>
      <label>GPS Update Rate</label>
      <select name="gpsUpdateRate" id="gpsUpdateRate">
        <option value="1">1 Hz</option>
        <option value="2">2 Hz</option>
        <option value="5">5 Hz</option>
        <option value="10">10 Hz</option>
        <option value="20">20 Hz</option>
      </select>
      <p style="font-size:0.8rem;color:#8899aa;margin:0.25rem 0 0">NMEA sentence output rate. Higher rates improve responsiveness but increase CPU load. 10 Hz is recommended for most use.</p>
    </div>
    <div class="card">
      <h2>NTRIP Corrections</h2>
      <p style="font-size:0.8rem;color:#8899aa;margin:0 0 0.75rem">Up to 3 sources with automatic failover. Sources are tried in order — if one fails 3 times it switches to the next enabled source.</p>
      <div id="ntripSourcesContainer"></div>
    </div>
    <button type="submit" class="btn btn-primary">Save &amp; Restart</button>
  </form>
</div>

<!-- SYSTEM PAGE -->
<div id="system" class="page">
  <div id="systemAuthWall" style="display:none" class="card">
    <p style="text-align:center;color:#e85a5a;font-size:1rem;margin:1rem 0">&#128274; Login required</p>
    <p style="text-align:center;color:#7a9ab8;font-size:0.85rem">Click <strong>Log Out</strong> then reload the page, or <button class="btn" style="display:inline;padding:4px 12px" onclick="retryAuth('system')">Try Again</button></p>
  </div>
  <div id="systemContent">
  <div class="card">
    <h2>System</h2>
    <p style="font-size:0.85rem;color:#7a9ab8;margin-bottom:1rem;">
      NMEA sentences are broadcast on TCP port <strong style="color:#5ab4e8">10110</strong>.<br>
      Connect SignalK, OpenCPN, or any chart plotter to <span id="sysIP">--</span>:10110.
    </p>
    <button class="btn btn-danger" onclick="doRestart()">Restart Device</button>
    <button class="btn btn-danger" onclick="doUM982Reset()" style="margin-top:0.5rem">UM982 Factory Reset</button>
    <p style="font-size:0.8rem;color:#8899aa;margin-top:0.25rem">Only use if heading/pitch stops working. Wipes UM982 config and reconfigures signal groups. Takes ~15 seconds.</p>
  </div>
  <div class="card">
    <h2>Bluetooth GPS</h2>
    <p style="font-size:0.85rem;color:#7a9ab8;margin-bottom:1rem">
      Broadcasts NMEA sentences over BLE so iOS/Android navigation apps
      (SW Maps, iNavX, etc.) can use this device as an external GPS receiver.
      Uses the <strong>Nordic UART Service</strong> profile.<br>
      <span style="color:#8899aa;font-size:0.8rem">Note: a device restart is required when toggling BLE on or off.</span>
    </p>
    <div class="toggle-row">
      <input type="checkbox" id="bleNmea" name="bleNmea" onchange="saveBleNmea(this)">
      <label for="bleNmea">Enable Bluetooth GPS (BLE NMEA)</label>
    </div>
    <p id="ble-status-text" style="font-size:0.85rem;margin-top:0.5rem;color:#8899aa">--</p>
  </div>
  <div class="card">
    <h2>WiFi Coprocessor Firmware</h2>
    <p style="font-size:0.85rem;color:#8ca8c1;margin-bottom:0.85rem">
      Updates the onboard ESP32-C6 that provides WiFi and Bluetooth. The tested
      firmware is bundled into this Sailing Computer build.
    </p>
    <div class="stat-grid" style="margin-bottom:0.75rem">
      <div class="stat">
        <div class="stat-label">C6 Running</div>
        <div class="stat-value firmware-version" id="c6RunningVersion">--</div>
      </div>
      <div class="stat">
        <div class="stat-label">C6 Bundled</div>
        <div class="stat-value firmware-version" id="c6AvailableVersion">--</div>
      </div>
    </div>
    <p id="c6ImageDetails" style="font-size:0.78rem;color:#7895af;margin-bottom:0.75rem">Checking bundled image...</p>
    <p style="font-size:0.75rem;color:#7895af;margin:-0.35rem 0 0.75rem">
      Running reports the C6 protocol version; Bundled reports this image's
      build version. Use the bundled build date to identify a custom image.
    </p>
    <div id="c6Progress" style="display:none;margin-bottom:0.85rem">
      <div class="progress-track">
        <div id="c6Bar" class="progress-fill"></div>
      </div>
      <p id="c6Status" style="font-size:0.85rem;color:#a9bfd2;margin:0.4rem 0 0">Ready</p>
    </div>
    <button id="c6UpdateButton" type="button" class="btn btn-primary firmware-action"
            onclick="startC6Update()" disabled>Update ESP32-C6</button>
    <p style="font-size:0.8rem;color:#ffcf75;margin:0.65rem 0 0">
      Do this ashore, not during a race. WiFi will disconnect briefly and the
      complete Sailing Computer will restart when the update finishes.
    </p>
  </div>
  <div class="card">
    <h2>Main Firmware (ESP32-P4)</h2>
    <p style="font-size:0.85rem;color:#7a9ab8;margin-bottom:1rem;">
      Build with PlatformIO, then upload
      <code style="background:#0a1628;padding:2px 6px;border-radius:4px">.pio/build/esp32p4/firmware.bin</code>.
      The device will restart automatically after a successful flash.
    </p>
    <!-- enctype here is ignored — startOTA() uses XHR with Content-Type: application/octet-stream (raw binary) -->
    <form id="otaForm" method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="firmware" id="otaFile" accept=".bin" required
        style="display:none" onchange="document.getElementById('otaFileName').textContent=this.files[0].name">
      <div style="display:flex;gap:0.5rem;align-items:center;flex-wrap:wrap">
        <button type="button" class="btn btn-primary" onclick="document.getElementById('otaFile').click()">
          Choose .bin file
        </button>
        <span id="otaFileName" style="color:#8899aa;font-size:0.85rem">No file chosen</span>
      </div>
      <div id="otaProgress" style="display:none;margin-top:0.75rem">
        <div style="background:#0a1628;border-radius:4px;height:8px;overflow:hidden">
          <div id="otaBar" style="background:#4a9eff;height:100%;width:0%;transition:width 0.3s"></div>
        </div>
        <p id="otaStatus" style="font-size:0.85rem;color:#8899aa;margin:0.25rem 0 0">Uploading...</p>
      </div>
      <button type="submit" class="btn btn-danger" style="margin-top:0.75rem" onclick="startOTA(event)">
        Upload &amp; Flash
      </button>
    </form>
  </div>
  </div><!-- systemContent -->
</div>

<!-- RACING PAGE -->
<!-- START / RACE SEQUENCE PAGE -->
<div id="start" class="page active">

  <!-- Race Clock -->
  <div class="card race-sequence-card">
    <h2>Race Sequence</h2>

    <!-- IDLE: duration picker + arm button -->
    <div id="raceSetupView">
      <div style="display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:0.75rem">
        <div class="stat" style="padding:9px 8px">
          <div class="stat-label">Course</div>
          <div id="raceSetupCourse" style="margin-top:4px;color:#edf5fc;font-size:0.88rem;font-weight:800">None</div>
        </div>
        <div class="stat" style="padding:9px 8px">
          <div class="stat-label">Start Line</div>
          <div id="raceSetupLine" style="margin-top:4px;color:#ffbb45;font-size:0.88rem;font-weight:800">Not set</div>
        </div>
      </div>
      <div style="text-align:center;color:#9ab2c7;font-size:0.82rem;margin-top:0.25rem">Start countdown</div>
      <div style="display:flex;align-items:center;justify-content:center;gap:16px;margin:1rem 0">
        <button class="btn" onclick="adjustDuration(-60)" style="font-size:1.1rem;padding:8px 14px">&#8722;1</button>
        <div id="raceDurationDisplay" style="font-size:3.2rem;font-weight:bold;font-variant-numeric:tabular-nums;color:#e0e8f0;min-width:90px;text-align:center">5:00</div>
        <button class="btn" onclick="adjustDuration(60)"  style="font-size:1.1rem;padding:8px 14px">+1</button>
      </div>
      <div style="display:flex;gap:8px;justify-content:center;margin-bottom:1rem">
        <button class="btn" id="durBtn5"  onclick="setDuration(300)"> 5 min</button>
        <button class="btn" id="durBtn10" onclick="setDuration(600)">10 min</button>
        <button class="btn" id="durBtn15" onclick="setDuration(900)">15 min</button>
      </div>
      <button id="armRaceBtn" class="btn btn-primary" style="width:100%;min-height:58px" onclick="armRace()">ARM 5:00 START</button>
    </div>

    <!-- COUNTDOWN / RACING: live clock -->
    <div id="raceActiveView" style="display:none">
      <div id="racePhaseLabel" style="font-size:0.75rem;text-transform:uppercase;letter-spacing:1px;color:#7a9ab8;text-align:center;margin-bottom:4px">STARTING IN</div>
      <div id="raceClock"
           onclick="syncRace()" title="Tap to sync to nearest minute"
           style="font-size:5rem;font-weight:bold;text-align:center;font-variant-numeric:tabular-nums;cursor:pointer;padding:0.4rem 0 0;border-radius:8px;line-height:1">0:00</div>
      <div style="text-align:center;font-size:0.68rem;color:#2a5a7a;margin-bottom:0.75rem">tap to sync</div>

      <!-- Compass -->
      <div id="compassSection" style="display:flex;flex-direction:column;align-items:center;margin-bottom:0.85rem">
        <canvas id="compassCanvas" width="220" height="220" style="display:block"></canvas>
        <div style="display:flex;justify-content:center;gap:12px;flex-wrap:wrap;font-size:0.74rem;margin-top:5px">
          <span style="color:#67c5ff">Boat <strong id="raceCompassHeading">--&deg;</strong></span>
          <span style="color:#ffbb45">CMG <strong id="raceCompassCmg">--&deg;</strong></span>
          <span style="color:#4fd39a">Mark <strong id="raceCompassMark">--&deg;</strong></span>
          <span style="color:#c49cff">Heel <strong id="raceCompassHeel">--&deg;</strong></span>
        </div>
        <button id="compassEnableBtn" class="btn" onclick="requestCompassPermission()"
          style="margin-top:6px;display:none">
          Enable Compass
        </button>
        <div id="compassUnavail" style="display:none;font-size:0.75rem;color:#3a6a8a;margin-top:4px">Compass not available</div>
      </div>

      <!-- Pre-start: time to line -->
      <div id="racePreInfo" style="background:#060f1e;border:1px solid #1a3050;border-radius:6px;padding:0.65rem 0.9rem;margin-bottom:0.75rem">
        <div style="display:flex;justify-content:space-between;align-items:baseline">
          <span style="color:#9ab2c7;font-size:0.82rem">Est. time to line</span>
          <span id="raceTTL" style="font-variant-numeric:tabular-nums;color:#e0e8f0;font-size:1.1rem;font-weight:bold">--:--</span>
        </div>
        <div style="display:flex;justify-content:space-between;align-items:baseline;margin-top:4px">
          <span style="color:#9ab2c7;font-size:0.82rem">At current SOG</span>
          <span id="raceTTLSog" style="font-variant-numeric:tabular-nums;color:#aac8e0;font-size:0.9rem">-- kts</span>
        </div>
      </div>

      <!-- Racing: next mark info -->
      <div id="raceMarkInfo" style="display:none;background:#060f1e;border:1px solid #1a3050;border-radius:6px;padding:0.65rem 0.9rem;margin-bottom:0.75rem">
        <div id="raceNextMarkName" style="font-weight:bold;color:#5ab4e8;margin-bottom:6px;font-size:0.95rem">Next Mark</div>
        <div class="stat-grid">
          <div class="stat">
            <div class="stat-label">Distance</div>
            <div id="raceMarkDist" class="stat-value" style="font-size:1.25rem">-- nm</div>
          </div>
          <div class="stat">
            <div class="stat-label">Time To Mark</div>
            <div id="raceMarkTTM" class="stat-value" style="font-size:1.25rem">--:--</div>
          </div>
          <div class="stat">
            <div class="stat-label">Bearing</div>
            <div id="raceMarkBrg" class="stat-value" style="font-size:1.05rem">--&deg;</div>
          </div>
          <div class="stat">
            <div class="stat-label">CMG</div>
            <div id="raceMarkCmg" class="stat-value" style="font-size:1.05rem">--&deg;</div>
          </div>
          <div class="stat">
            <div class="stat-label">VMG</div>
            <div id="raceMarkVmg" class="stat-value" style="font-size:1.05rem">-- kt</div>
          </div>
          <div class="stat">
            <div class="stat-label">SMG</div>
            <div id="raceMarkSmg" class="stat-value" style="font-size:1.05rem">-- kt</div>
          </div>
        </div>
        <p style="font-size:0.7rem;color:#617f99;margin:0.55rem 0 0">
          VMG is the current velocity component toward the mark. SMG is measured closing speed; time-to-mark uses positive SMG.
        </p>
      </div>

      <div class="race-actions">
        <button id="racePrevLegBtn" class="btn" style="flex:1;display:none" onclick="prevLeg()">&#8592; Prev Mark</button>
        <button id="raceNextLegBtn" class="btn btn-primary" style="flex:1;display:none" onclick="nextLeg()">Next Mark &#8594;</button>
        <button id="raceCancelBtn" class="btn btn-danger" style="display:none" onclick="resetRace()">Cancel Start</button>
        <button id="raceEndBtn" class="btn btn-danger" style="display:none" onclick="endRace()">Finish Race</button>
      </div>
    </div>

    <!-- COMPLETE / STATS -->
    <div id="raceCompleteView" style="display:none">
      <div style="text-align:center;margin-bottom:1rem">
        <div style="font-size:0.75rem;text-transform:uppercase;letter-spacing:2px;color:#4caf82;margin-bottom:4px">Race Complete</div>
        <div id="raceElapsedDisplay" style="font-size:4rem;font-weight:bold;font-variant-numeric:tabular-nums;color:#4caf82;line-height:1">0:00:00</div>
      </div>
      <div id="raceStatsBody" style="background:#060f1e;border:1px solid #1a3050;border-radius:6px;padding:0.7rem 0.9rem;margin-bottom:1rem">
        <div id="raceStatsCourse" style="font-size:0.82rem;color:#7a9ab8;margin-bottom:0.5rem"></div>
        <div id="raceStatsLegs"></div>
      </div>
      <button class="btn btn-primary" style="width:100%;padding:12px" onclick="resetRace()">New Race</button>
    </div>
  </div>

  <!-- Crew display controls -->
  <div class="card crew-control-card">
    <div class="card-heading-row">
      <h2>Crew Display</h2>
      <a href="/crew" target="_blank" rel="noopener"
         style="color:#67c5ff;font-size:0.78rem;font-weight:800;text-decoration:none">OPEN DISPLAY</a>
    </div>
    <div class="crew-control-grid">
      <div>
        <label for="crewPhase">Race phase</label>
        <select id="crewPhase">
          <option value="prestart">Pre-start</option>
          <option value="starting">Starting sequence</option>
          <option value="upwind">Upwind leg</option>
          <option value="downwind">Downwind leg</option>
          <option value="reaching">Reaching leg</option>
          <option value="rounding">Mark rounding</option>
          <option value="finish">Finish</option>
          <option value="custom">Free / custom</option>
        </select>
      </div>
      <div>
        <label for="crewMode">Shared display mode</label>
        <select id="crewMode">
          <option value="auto">Automatic by phase</option>
          <option value="start">Start</option>
          <option value="navigation">Navigation</option>
          <option value="performance">Performance</option>
          <option value="rounding">Mark rounding</option>
          <option value="custom">Custom</option>
        </select>
      </div>
      <div>
        <label for="crewActiveMark">Active mark</label>
        <select id="crewActiveMark">
          <option value="">Automatic from course</option>
        </select>
      </div>
      <div>
        <label for="crewNextMark">Next mark</label>
        <select id="crewNextMark">
          <option value="">Automatic from course</option>
        </select>
      </div>
      <div>
        <label for="crewStartTarget">Start target</label>
        <select id="crewStartTarget">
          <option value="line">Start line</option>
          <option value="pin">Pin end</option>
          <option value="boat">Boat end</option>
        </select>
      </div>
      <div>
        <label for="crewTargetHeading">Target heading (optional)</label>
        <input id="crewTargetHeading" type="number" min="0" max="359"
               inputmode="numeric" placeholder="0-359">
      </div>
    </div>

    <div class="section-title">Priority tiles</div>
    <div class="crew-priority-grid">
      <div><label for="crewPriority0">Priority 1</label><select id="crewPriority0"></select></div>
      <div><label for="crewPriority1">Priority 2</label><select id="crewPriority1"></select></div>
      <div><label for="crewPriority2">Priority 3</label><select id="crewPriority2"></select></div>
      <div><label for="crewPriority3">Priority 4</label><select id="crewPriority3"></select></div>
    </div>

    <label for="crewManeuver">Maneuver / preparation message</label>
    <input id="crewManeuver" type="text" maxlength="63"
           placeholder="e.g. Bear away / set">
    <label for="crewStatus">Status message</label>
    <input id="crewStatus" type="text" maxlength="63"
           placeholder="e.g. Lift +7 degrees">

    <div class="toggle-row">
      <input id="crewLocked" type="checkbox">
      <label for="crewLocked">Lock crew displays to the shared view</label>
    </div>
    <div class="crew-control-actions">
      <button class="btn btn-primary" onclick="pushCrewDisplay()">Push To Crew Displays</button>
      <button class="btn" onclick="loadCrewControlState()">Reload Controls</button>
    </div>
    <div id="crewControlStatus" style="margin-top:9px;color:#6f91ad;font-size:0.78rem">
      Loading crew display state...
    </div>
  </div>

  <!-- Start Line (collapsible) -->
  <div class="card race-line-card">
    <h2 onclick="toggleStartLine()" style="cursor:pointer;display:flex;justify-content:space-between;align-items:center;margin:0">
      Start Line
      <span id="startLineChevron" style="font-size:0.9rem;color:#5a7a9a;transition:transform 0.2s">&#9660;</span>
    </h2>
    <div id="startLineBody" style="margin-top:1rem">
      <div style="margin-bottom:1.1rem">
        <div class="section-title">Port End (Pin)</div>
        <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:6px">
          <button class="btn" style="white-space:nowrap" onclick="useGpsForLine(0)">Set Here</button>
          <select id="lineMarkSel0" onchange="useMarkForLine(0)"
            style="flex:1;min-width:0;background:#091a36;border:1px solid #1e4080;color:#e0e8f0;padding:6px 8px;border-radius:5px;font-size:0.85rem">
            <option value="">&#8212; saved mark &#8212;</option>
          </select>
        </div>
        <div id="lineStatus0" style="font-size:0.8rem;color:#5a7a9a">Not set</div>
      </div>
      <div>
        <div class="section-title">Starboard End (Boat)</div>
        <div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:6px">
          <button class="btn" style="white-space:nowrap" onclick="useGpsForLine(1)">Set Here</button>
          <select id="lineMarkSel1" onchange="useMarkForLine(1)"
            style="flex:1;min-width:0;background:#091a36;border:1px solid #1e4080;color:#e0e8f0;padding:6px 8px;border-radius:5px;font-size:0.85rem">
            <option value="">&#8212; saved mark &#8212;</option>
          </select>
        </div>
        <div id="lineStatus1" style="font-size:0.8rem;color:#5a7a9a">Not set</div>
      </div>
    </div>
  </div>

  <!-- Course -->
  <div class="card race-course-card">
    <h2>Course</h2>
    <div style="display:flex;gap:8px;align-items:center;margin-bottom:0.5rem">
      <select id="raceCourseSelect"
        style="flex:1;background:#091a36;border:1px solid #1e4080;color:#e0e8f0;padding:8px;border-radius:5px;font-size:0.85rem">
        <option value="">&#8212; no course &#8212;</option>
      </select>
      <button class="btn btn-primary" onclick="setRaceCourse()">Use</button>
    </div>
    <!-- Selected course detail (expanded with laps) -->
    <div id="raceCourseDetail" style="display:none;margin:0.75rem 0 0.25rem">
      <div style="font-size:0.72rem;text-transform:uppercase;letter-spacing:0.5px;color:#7a9ab8;margin-bottom:6px">Course Sequence</div>
      <div id="raceCourseMarks" style="line-height:2.1"></div>
    </div>
    <!-- Lap selector -->
    <div id="raceLapSelector" style="display:none;margin-top:0.9rem">
      <div style="font-size:0.72rem;text-transform:uppercase;letter-spacing:0.5px;color:#7a9ab8;margin-bottom:6px">Laps</div>
      <div style="display:flex;gap:6px">
        <button class="btn" id="lapBtn1" onclick="setLaps(1)">1</button>
        <button class="btn" id="lapBtn2" onclick="setLaps(2)">2</button>
        <button class="btn" id="lapBtn3" onclick="setLaps(3)">3</button>
        <button class="btn" id="lapBtn4" onclick="setLaps(4)">4</button>
        <button class="btn" id="lapBtn5" onclick="setLaps(5)">5</button>
      </div>
    </div>
    <div id="raceCourseStatus" style="font-size:0.8rem;color:#5a7a9a;margin-top:8px"></div>
  </div>

</div>

<!-- MARKS / ROUTES PAGE -->
<div id="marks" class="page">
  <!-- Mark Manager -->
  <div class="card">
    <h2>Mark Manager</h2>
    <div id="markList" style="margin-bottom:1rem">
      <p style="color:#5a7a9a;font-size:0.85rem">Loading marks&hellip;</p>
    </div>
    <div class="section-title">Add Mark</div>
    <div class="row">
      <div>
        <label>Name</label>
        <input id="mkName" type="text" placeholder="e.g. Pin End" maxlength="31">
      </div>
      <div>
        <button class="btn btn-primary" style="margin-top:22px;width:100%" onclick="useGpsForMark()">Use Current Position</button>
      </div>
    </div>
    <div class="row">
      <div>
        <label>Latitude</label>
        <input id="mkLat" type="number" step="0.0000001" placeholder="41.2345678">
      </div>
      <div>
        <label>Longitude</label>
        <input id="mkLon" type="number" step="0.0000001" placeholder="-70.1234567">
      </div>
    </div>
    <button class="btn btn-primary" onclick="addMark()">Add Mark</button>
  </div>

  <!-- Courses -->
  <div class="card">
    <div class="card-heading-row">
      <h2>Courses</h2>
      <button type="button" class="btn btn-primary" onclick="newCourse()">New Course</button>
    </div>
    <div id="courseEditor" class="course-editor" style="display:none">
      <div id="courseEditorTitle" class="section-title" style="margin-top:0">New Course</div>
      <label for="courseEditName">Course Name</label>
      <input id="courseEditName" type="text" maxlength="31" placeholder="e.g. Wednesday Windward/Leeward">
      <div class="section-title">Mark Sequence</div>
      <div id="courseEditorMarks"></div>
      <button type="button" class="btn" style="width:100%;margin-top:9px" onclick="addCourseMark()">Add Mark</button>
      <p style="font-size:0.76rem;color:#7895af;margin:0.65rem 0">
        Put marks in rounding order. P means leave the mark to port; S means leave it to starboard.
      </p>
      <div style="display:flex;gap:8px">
        <button type="button" class="btn" style="flex:1" onclick="cancelCourseEdit()">Cancel</button>
        <button type="button" class="btn btn-primary" style="flex:2" onclick="saveCourse()">Save Course</button>
      </div>
    </div>
    <div id="courseList">
      <p style="color:#5a7a9a;font-size:0.85rem">Loading courses&hellip;</p>
    </div>
  </div>

  <!-- GPX Import -->
  <div class="card">
    <h2>Import GPX File</h2>
    <p style="font-size:0.82rem;color:#7a9ab8;margin-bottom:12px">
      Import waypoints as marks and routes as courses from a standard GPX file.
      Duplicate mark names are skipped. Route points are matched to marks by name.
    </p>
    <div style="display:flex;gap:10px;align-items:center;flex-wrap:wrap">
      <input id="gpxFile" type="file" accept=".gpx,.xml"
             style="flex:1;min-width:0;background:#091a36;border:1px dashed #1e4080;padding:8px;border-radius:5px;color:#7a9ab8;font-size:0.85rem">
      <button class="btn btn-primary" style="white-space:nowrap" onclick="importGpx()">&#8593; Import</button>
    </div>
    <div id="gpxResult" style="margin-top:10px;font-size:0.85rem;display:none"></div>
  </div>

</div>

<!-- TRACKS PAGE -->
<div id="tracks" class="page">

  <div class="card">
    <h2>Track Recording</h2>
    <div id="trackSdWarning" style="display:none;padding:.5rem .75rem;border-radius:6px;
         background:#3a1010;color:#e85a5a;font-size:.88rem;margin-bottom:.75rem">
    </div>
    <div id="trackStatusLine" style="margin-bottom:1rem;font-size:.9rem;color:#aaa">
      Initializing…
    </div>

    <div style="margin-bottom:1rem">
      <button id="trackLoopBtn" onclick="toggleTrackLoop()" style="min-width:200px">Enable Loop Recording</button>
    </div>

    <!-- Export segment — shown once loop has data -->
    <div id="trackExportSection" style="display:none">
      <hr style="border-color:#1a2a3a;margin-bottom:1rem">
      <div style="font-size:.88rem;color:#8ab0cc;margin-bottom:.75rem;font-weight:600">Export Segment</div>
      <div style="margin-bottom:.75rem">
        <div style="display:flex;justify-content:space-between;margin-bottom:.25rem">
          <label style="font-size:.83rem;color:#aaa">Start</label>
          <span id="trackT0Label" style="font-size:.8rem;font-family:monospace;color:#4caf82">--</span>
        </div>
        <input type="range" id="trackT0" style="width:100%;accent-color:#4caf82" oninput="onTrackT0()">
      </div>
      <div style="margin-bottom:.75rem">
        <div style="display:flex;justify-content:space-between;margin-bottom:.25rem">
          <label style="font-size:.83rem;color:#aaa">End</label>
          <span id="trackT1Label" style="font-size:.8rem;font-family:monospace;color:#4caf82">--</span>
        </div>
        <input type="range" id="trackT1" style="width:100%;accent-color:#4caf82" oninput="onTrackT1()">
      </div>
      <div id="trackExportDur" style="font-size:.8rem;color:#888;margin-bottom:.75rem">Duration: --</div>
      <button id="trackExportBtn" onclick="exportTrackSegment()">Export GPX</button>
      <div id="trackFileStatus" style="display:none;padding:.5rem .75rem;border-radius:6px;
           background:#1a3a1a;color:#4caf82;font-size:.9rem;margin-top:.75rem">
        ✓ File written: <span id="trackFileName"></span>
      </div>
    </div>
  </div>

  <div class="card">
    <h2 onclick="toggleTrackSettings()" style="cursor:pointer">
      Recording Settings <span id="trackSettingsChevron" style="float:right;transition:transform .2s">▼</span>
    </h2>
    <div id="trackSettingsBody">
      <label style="display:block;margin-bottom:.3rem">Recording Interval</label>
      <select id="trackIntervalSel" style="width:100%;margin-bottom:1rem">
        <option value="1">1 second</option>
        <option value="5">5 seconds</option>
        <option value="10">10 seconds</option>
        <option value="30">30 seconds</option>
        <option value="60">1 minute</option>
      </select>
      <label style="display:block;margin-bottom:.3rem">Loop Buffer Duration</label>
      <select id="trackLoopHrsSel" style="width:100%;margin-bottom:1rem">
        <option value="1">1 hour</option>
        <option value="2">2 hours</option>
        <option value="3">3 hours</option>
        <option value="6">6 hours</option>
        <option value="12">12 hours</option>
        <option value="24">24 hours</option>
      </select>
      <p style="font-size:.8rem;color:#888;margin-bottom:1rem">
        Changing these settings will erase the current loop buffer and create a new one.
        Use the Files tab to download saved track files.
      </p>
      <button onclick="saveTrackSettings()">Save Settings</button>
    </div>
  </div>

</div>

<!-- FILES PAGE -->
<div id="files" class="page">

  <!-- Storage summary -->
  <div class="card">
    <h2>Storage <button class="btn" style="padding:3px 10px;font-size:0.75rem;margin-left:8px;background:#1a3a5c" onclick="loadStorageInfo()">&#8635; Refresh</button></h2>
    <div id="storageInfo" style="font-size:0.85rem;color:#7a9ab8">Loading&hellip;</div>
    <div style="margin-top:10px">
      <div style="background:#091a36;border-radius:4px;height:10px;overflow:hidden">
        <div id="storageBar" style="height:100%;background:#5ab4e8;width:0%;transition:width 0.4s"></div>
      </div>
    </div>
  </div>

  <!-- File browser -->
  <div class="card">
    <h2>File Browser</h2>
    <div id="fileBreadcrumb" style="font-size:0.82rem;color:#7a9ab8;margin-bottom:12px;word-break:break-all;line-height:1.8">
      /
    </div>
    <div style="overflow-x:auto">
      <table style="width:100%;border-collapse:collapse;font-size:0.85rem">
        <thead>
          <tr style="font-size:0.72rem;color:#5a7a9a;text-transform:uppercase">
            <th style="text-align:left;padding:4px 8px">Name</th>
            <th style="text-align:right;padding:4px 8px;white-space:nowrap">Size</th>
            <th style="text-align:right;padding:4px 6px">Actions</th>
          </tr>
        </thead>
        <tbody id="fileList">
          <tr><td colspan="3" style="padding:10px 8px;color:#5a7a9a">Loading&hellip;</td></tr>
        </tbody>
      </table>
    </div>
  </div>

  <!-- Danger zone -->
  <div class="card">
    <h2 style="color:#e85a5a">Danger Zone</h2>
    <p style="font-size:0.82rem;color:#7a9ab8;margin-bottom:12px">
      Formatting erases <strong>all files</strong> on the SD card, then recreates the empty marks and courses files.
      This cannot be undone.
    </p>
    <button class="btn btn-danger" onclick="formatSd()">&#9888; Format SD Card</button>
  </div>

</div>

<div class="toast" id="toast"></div>

<script>
function showAuthWall(page) {
  document.getElementById(page + 'AuthWall').style.display = '';
  var content = document.getElementById(page + 'Content');
  if (content) content.style.display = 'none';
  var form = document.getElementById(page + 'Form');
  if (form) form.style.display = 'none';
}
function hideAuthWall(page) {
  document.getElementById(page + 'AuthWall').style.display = 'none';
  var content = document.getElementById(page + 'Content');
  if (content) content.style.display = '';
  var form = document.getElementById(page + 'Form');
  if (form) form.style.display = '';
}
function retryAuth(page) {
  // Re-attempt auth by fetching /config — browser will re-prompt for credentials
  fetch('/config').then(function(r) {
    if (r.ok) {
      hideAuthWall(page);
      if (page === 'config') loadConfig();
      if (page === 'system') { loadC6Status(); startC6Polling(); }
    }
    else showAuthWall(page);
  }).catch(function() { showAuthWall(page); });
}

var PAGE_TITLES = {
  start: 'Race',
  status: 'Sailing Data',
  marks: 'Marks',
  tracks: 'Track Recording',
  files: 'Files and Storage',
  config: 'Device Settings',
  system: 'System'
};

function toggleMoreMenu() {
  var menu = document.getElementById('moreMenu');
  var backdrop = document.getElementById('moreBackdrop');
  var btn = document.getElementById('moreNavButton');
  var open = !menu.classList.contains('open');
  menu.classList.toggle('open', open);
  backdrop.classList.toggle('open', open);
  btn.setAttribute('aria-expanded', open ? 'true' : 'false');
}

function closeMoreMenu() {
  document.getElementById('moreMenu').classList.remove('open');
  document.getElementById('moreBackdrop').classList.remove('open');
  document.getElementById('moreNavButton').setAttribute('aria-expanded', 'false');
}

function showPage(id) {
  closeMoreMenu();
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('nav button').forEach(b => b.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  var directBtn = document.querySelector('nav button[data-page="' + id + '"]');
  (directBtn || document.getElementById('moreNavButton')).classList.add('active');
  document.getElementById('pageTitle').textContent = PAGE_TITLES[id] || 'Sailing Computer';
  window.scrollTo(0, 0);
  if (id === 'config') loadConfig();
  if (id === 'marks')  { loadMarks(); loadCourses(); }
  if (id === 'start')  { loadRaceState(); loadRaceMarksAndCourses(); loadCrewControlState(); startRacePolling(); initRaceCompass(); }
  if (id !== 'start')  { stopRaceCompass(); }
  if (id === 'tracks') { startTrackPolling(); }
  if (id !== 'tracks') { stopTrackPolling(); }
  if (id !== 'system') { stopC6Polling(); }
  if (id === 'files')  { initFilesPage(); }
  if (id === 'system') {
    fetch('/config').then(function(r) {
      if (r.ok) {
        hideAuthWall('system');
        loadC6Status();
        startC6Polling();
      }
      else showAuthWall('system');
    }).catch(function() { showAuthWall('system'); });
  }
}

document.addEventListener('keydown', function(e) {
  if (e.key === 'Escape') closeMoreMenu();
});

function toast(msg, ok=true) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.style.display = 'block';
  el.style.background = ok ? '#1a4a30' : '#3a1010';
  el.style.color = ok ? '#4caf82' : '#e85a5a';
  setTimeout(() => el.style.display='none', 3000);
}

function setText(id, val) {
  const el = document.getElementById(id);
  if (el) el.textContent = val;
}

function formatBytes(b) {
  if (b < 1024) return b + ' B';
  if (b < 1048576) return (b/1024).toFixed(1) + ' KB';
  return (b/1048576).toFixed(1) + ' MB';
}

function updateStatus() {
  fetch('/status').then(function(r) { return r.json(); }).then(function(d) {
    raceStatusCache = d;
    var fixColors = {0:'err',1:'warn',2:'ok',4:'rtk',5:'ok'};
    var el = document.getElementById('s-fix');
    el.textContent = d.fixLabel || '--';
    el.className = 'stat-value ' + (fixColors[d.fix] || 'err');
    setText('s-sats', d.sats != null ? d.sats : '--');
    // Decimal places scale with fix quality:
    // RTK Fixed(4)/Float(5) → 7 dp (~1 cm), DGPS(2) → 6 dp (~11 cm), GPS(1) → 5 dp (~1 m)
    var coordDp = (d.fix === 4 || d.fix === 5) ? 7 : (d.fix === 2 ? 6 : 5);
    setText('s-lat', d.lat != null ? Number(d.lat).toFixed(coordDp) + '°' : '--');
    setText('s-lon', d.lon != null ? Number(d.lon).toFixed(coordDp) + '°' : '--');
    setText('s-hdt',   d.hdtValid   ? Number(d.heading).toFixed(1) + '\u00b0' : '--\u00b0');
    setText('s-roll',  d.rollValid  ? Number(d.roll).toFixed(1)    + '\u00b0' : '--\u00b0');
    var hdtEl = document.getElementById('s-hdtsrc');
    hdtEl.textContent = d.hdtValid ? 'Dual Antenna' : 'No ANT2 Lock';
    hdtEl.className = 'stat-value ' + (d.hdtValid ? 'ok' : 'warn');
    setText('s-sog', d.sog != null ? Number(d.sog).toFixed(1) : '--');
    var cogEl = document.getElementById('s-cog');
    if (d.cog != null) {
      cogEl.textContent = Number(d.cog).toFixed(1) + '\u00b0';
      cogEl.style.opacity = d.cogValid ? '1' : '0.4';
      cogEl.title = d.cogValid ? '' : 'Frozen \u2014 speed below ' + (d.cogMinSog || 0.1) + ' kts';
    }
    setText('s-hdop', d.hdop     != null ? Number(d.hdop).toFixed(2) : '--');
    setText('s-alt',  d.altitude != null ? Number(d.altitude).toFixed(1) + ' m' : '--');

    // Sailing performance metrics
    var roll = d.rollValid ? Number(d.roll) : 0;
    var tack = (d.rollValid && roll >  3) ? 'port'
             : (d.rollValid && roll < -3) ? 'starboard'
             : 'unknown';
    var tackEl  = document.getElementById('s-tack');
    var tackSub = document.getElementById('s-tack-sub');
    if (tack === 'port') {
      tackEl.textContent  = 'Port';
      tackEl.style.color  = '#f87171'; // red
      tackSub.textContent = roll.toFixed(1) + '° stbd heel';
    } else if (tack === 'starboard') {
      tackEl.textContent  = 'Starboard';
      tackEl.style.color  = '#60a5fa'; // blue
      tackSub.textContent = Math.abs(roll).toFixed(1) + '° port heel';
    } else {
      tackEl.textContent  = '--';
      tackEl.style.color  = '#8899aa';
      tackSub.textContent = 'heel < 3°';
    }

    if (d.sailingValid) {
      var leeway = Number(d.leeway);
      var lateral = Number(d.lateralDrift);
      setText('s-leeway',  leeway.toFixed(1) + '°');
      setText('s-lateral', Math.abs(lateral).toFixed(2) + ' kts');
      setText('s-drive',   Number(d.driveSpeed).toFixed(2) + ' kts');
      // Direction labels
      var leewayEl = document.getElementById('s-leeway');
      leewayEl.style.color = Math.abs(leeway) > 5 ? '#f59e0b' : '#4ade80';
      document.getElementById('s-leeway-dir').textContent =
        leeway > 0.5 ? '▶ starboard slip' : leeway < -0.5 ? '◀ port slip' : 'on track';
      document.getElementById('s-lateral-dir').textContent =
        lateral > 0 ? '▶ starboard' : lateral < 0 ? '◀ port' : 'centered';

      // Current favor: lateral drift component toward windward on current tack.
      // Port tack: windward = port → favorable when lateral < 0 → favor = -lateral
      // Starboard tack: windward = starboard → favorable when lateral > 0 → favor = +lateral
    var favorRow = document.getElementById('s-favor-row');
      if (tack !== 'unknown') {
        var favor = tack === 'port' ? -lateral : lateral;
        favorRow.style.display = '';
        var favorVal  = document.getElementById('s-favor-val');
        var favorDesc = document.getElementById('s-favor-desc');
        favorVal.textContent = (favor >= 0 ? '+' : '') + favor.toFixed(2) + ' kts';
        if (favor > 0.1) {
          favorVal.style.color  = '#4ade80';
          favorDesc.textContent = 'cross-track drift favors ' + tack + ' tack';
        } else if (favor < -0.1) {
          favorVal.style.color  = '#f87171';
          favorDesc.textContent = 'cross-track drift opposes ' + tack + ' tack';
        } else {
          favorVal.style.color  = '#8899aa';
          favorDesc.textContent = 'cross-track drift is neutral';
        }
      } else {
        favorRow.style.display = 'none';
      }
    } else {
      setText('s-leeway',  '--');
      setText('s-lateral', '--');
      setText('s-drive',   '--');
      document.getElementById('s-leeway-dir').textContent  = 'needs heading + speed';
      document.getElementById('s-lateral-dir').textContent = 'sideways speed';
      document.getElementById('s-favor-row').style.display = 'none';
    }
    setText('s-wifimode', d.wifiMode || '--');
    var ip = d.wifiMode === 'AP' ? d.apIP : d.ip;
    setText('s-ip', ip || '--');
    setText('s-rtcm', formatBytes(d.ntripBytesIn || 0) + ' / ' +
      formatBytes(d.ntripBytesToUart || 0));
    setText('s-rtcm-frames', (d.rtcmFramesIn || 0) + ' / ' +
      (d.rtcmLastType || '--'));
    var rtcmUart = document.getElementById('s-rtcm-uart');
    rtcmUart.textContent = d.rtcmUartProbeOk ? 'Responding' : 'No response';
    rtcmUart.className = 'stat-value ' + (d.rtcmUartProbeOk ? 'ok' : 'err');
    setText('s-nmea-bytes', formatBytes(d.nmeaBytesRx || 0));
    var nmeaLines = d.nmeaLinesRx || 0;
    var nmea = document.getElementById('s-nmea-bytes');
    nmea.style.color = nmeaLines > 0 ? '#4ade80' : '#f87171';
    setText('s-nmea-lines', nmeaLines + ' sentences');
    setText('sysIP', ip || '--');
    setText('deviceIP', ip || '--');
    if (d.version) {
      var vEl = document.getElementById('fwVersion');
      if (vEl && vEl.textContent !== 'v' + d.version) vEl.textContent = 'v' + d.version;
    }
    var ntrip = document.getElementById('s-ntrip');
    ntrip.textContent = d.ntripConnected ? 'Connected' : 'Off';
    ntrip.className = 'stat-value ' + (d.ntripConnected ? 'ok' : 'err');
    setText('s-ntrip-src', d.ntripConnected ? 'Source ' + (d.ntripActiveIdx||0) : '--');
    var headerFix = document.getElementById('headerFix');
    headerFix.textContent = 'GPS ' + (d.fixLabel || '--');
    headerFix.className = 'status-pill ' + (fixColors[d.fix] || 'err');
    var headerNtrip = document.getElementById('headerNtrip');
    headerNtrip.textContent = d.ntripConnected ? 'NTRIP ON' : 'NTRIP OFF';
    headerNtrip.className = 'status-pill ' + (d.ntripConnected ? 'ok' : 'warn');

    // BLE status tile — only show if BLE is enabled
    var bleTile = document.getElementById('s-ble-tile');
    var bleEl   = document.getElementById('s-ble');
    if (d.bleEnabled) {
      bleTile.style.display = '';
      bleEl.textContent  = d.bleConnected ? 'Connected' : 'Advertising';
      bleEl.className    = 'stat-value ' + (d.bleConnected ? 'ok' : 'warn');
    } else {
      bleTile.style.display = 'none';
    }

    // Update BLE toggle and status text on System tab
    var bleCb = document.getElementById('bleNmea');
    if (bleCb) bleCb.checked = d.bleEnabled;
    var bleStatusText = document.getElementById('ble-status-text');
    if (bleStatusText) {
      if (d.bleEnabled) {
        bleStatusText.textContent = d.bleConnected ? 'Client connected' : 'Advertising as "SailingComputer" — open your GPS app and scan for Bluetooth devices';
        bleStatusText.style.color = d.bleConnected ? '#4ade80' : '#8899aa';
      } else {
        bleStatusText.textContent = 'Disabled';
        bleStatusText.style.color = '#8899aa';
      }
    }
    // Live instruments
    updateCompass(
      d.hdtValid  ? Number(d.heading) : 0,
      d.cogValid  ? Number(d.cog)     : 0,
      d.sailingValid ? Number(d.leeway) : 0,
      d.sailingValid || false,
      d.hdtValid  || false,
      d.cogValid  || false
    );
    updateHeel(d.rollValid ? Number(d.roll) : 0, d.rollValid || false);

  }).catch(function(e) { console.error('Status fetch error:', e); });
}
setInterval(updateStatus, 500);
updateStatus();

// ── Instrument initialisation ─────────────────────────────────────────────────

function initNavigationInstruments() {
  var ticks = document.getElementById('c-ticks');
  var lbls  = document.getElementById('c-labels');
  if (!ticks) return;
  var cardinals = {0:'N',45:'NE',90:'E',135:'SE',180:'S',225:'SW',270:'W',315:'NW'};
  var mainCard  = {0:1, 90:1, 180:1, 270:1};
  var tickHtml = '', lblHtml = '';
  for (var i = 0; i < 360; i += 5) {
    var rad = (i - 90) * Math.PI / 180;
    var isMain = i % 90 === 0, isTen = i % 10 === 0;
    var r1 = 112, r2 = r1 - (isMain ? 12 : isTen ? 8 : 5);
    var sw = isMain ? 2 : isTen ? 1.2 : 0.7;
    var col = isMain ? '#5ab4e8' : '#2a4a7f';
    var x1 = r1*Math.cos(rad), y1 = r1*Math.sin(rad);
    var x2 = r2*Math.cos(rad), y2 = r2*Math.sin(rad);
    tickHtml += '<line x1="'+x1.toFixed(1)+'" y1="'+y1.toFixed(1)+'" x2="'+x2.toFixed(1)+'" y2="'+y2.toFixed(1)+'" stroke="'+col+'" stroke-width="'+sw+'"/>';
  }
  Object.keys(cardinals).forEach(function(deg) {
    var rad = (deg - 90) * Math.PI / 180;
    var r = 94, isMain = mainCard[parseInt(deg)];
    var x = r*Math.cos(rad), y = r*Math.sin(rad);
    lblHtml += '<text x="'+x.toFixed(1)+'" y="'+y.toFixed(1)+'" dy="4" text-anchor="middle"'+
               ' fill="'+(isMain?'#c8daf0':'#5a7a9a')+'" font-size="'+(isMain?10:8)+'"'+
               ' font-weight="'+(isMain?'bold':'normal')+'">'+cardinals[deg]+'</text>';
  });
  ticks.innerHTML = tickHtml;
  lbls.innerHTML  = lblHtml;
}

function initHeelScale() {
  var scale = document.getElementById('h-scale');
  if (!scale) return;
  var html = '';
  for (var deg = -45; deg <= 45; deg += 15) {
    var rad = deg * Math.PI / 180;
    var x = 100 * Math.sin(rad), y = -100 * Math.cos(rad) + 14;
    var tickX2 = 92 * Math.sin(rad), tickY2 = -92 * Math.cos(rad) + 14;
    var col = Math.abs(deg) >= 30 ? '#f59e0b' : '#2a4a7f';
    html += '<line x1="'+x.toFixed(1)+'" y1="'+y.toFixed(1)+'" x2="'+tickX2.toFixed(1)+'" y2="'+tickY2.toFixed(1)+'" stroke="'+col+'" stroke-width="1.5"/>';
    if (deg !== 0) {
      var lx = 80*Math.sin(rad), ly = -80*Math.cos(rad)+14;
      html += '<text x="'+lx.toFixed(1)+'" y="'+ly.toFixed(1)+'" dy="3" text-anchor="middle" fill="'+col+'" font-size="8">'+Math.abs(deg)+'</text>';
    }
  }
  scale.innerHTML = html;
}

function updateCompass(hdg, cogDeg, leeway, sailValid, hdtV, cogV) {
  var hdgPtr = document.getElementById('c-hdg');
  var cogPtr = document.getElementById('c-cog');
  var arc    = document.getElementById('c-arc');
  var hdgNum = document.getElementById('c-hdg-num');
  var cogNum = document.getElementById('c-cog-num');
  if (!hdgPtr) return;

  if (hdtV) {
    hdgPtr.setAttribute('transform', 'rotate('+hdg.toFixed(1)+')');
    hdgNum.textContent = Math.round(hdg)+'°';
  }
  if (cogV) {
    cogPtr.setAttribute('transform', 'rotate('+cogDeg.toFixed(1)+')');
    cogNum.textContent = Math.round(cogDeg)+'°';
  }

  // Leeway arc between heading and COG
  if (hdtV && cogV && sailValid) {
    var r = 68;
    var sRad = (hdg  - 90) * Math.PI / 180;
    var eRad = (cogDeg - 90) * Math.PI / 180;
    var sx = r*Math.cos(sRad), sy = r*Math.sin(sRad);
    var ex = r*Math.cos(eRad), ey = r*Math.sin(eRad);
    var large = Math.abs(leeway) > 180 ? 1 : 0;
    var sweep = leeway > 0 ? 1 : 0;
    arc.setAttribute('d','M '+sx.toFixed(1)+' '+sy.toFixed(1)+
                        ' A '+r+' '+r+' 0 '+large+' '+sweep+' '+ex.toFixed(1)+' '+ey.toFixed(1));
    arc.setAttribute('stroke', Math.abs(leeway) > 5 ? '#f59e0b' : '#4ade80');
  } else {
    arc.setAttribute('d','');
  }
}

function updateHeel(roll, valid) {
  var hull   = document.getElementById('h-hull');
  var water  = document.getElementById('h-water-rect');
  var num    = document.getElementById('h-num');
  if (!hull) return;
  var r = valid ? roll : 0;
  hull.setAttribute('transform',  'rotate('+r+',0,14)');
  water.setAttribute('transform', 'rotate('+r+',0,14)');
  num.textContent = (valid ? r.toFixed(1) : '--') + '°';
  num.setAttribute('fill', Math.abs(r)>30 ? '#e85a5a' : Math.abs(r)>20 ? '#f59e0b' : '#e0e8f0');
}

// Initialise on load
initNavigationInstruments();
initHeelScale();

const SOURCE_LABELS = ['Source 1 (Primary)', 'Source 2 (Failover)', 'Source 3 (Failover)'];

function buildNtripSources(sources) {
  const container = document.getElementById('ntripSourcesContainer');
  container.innerHTML = '';
  sources.forEach(function(src, i) {
    var div = document.createElement('div');
    div.style.cssText = 'border:1px solid #1e3a5f;border-radius:6px;padding:0.75rem;margin-bottom:0.75rem';
    div.innerHTML =
      '<div class="toggle-row" style="margin-bottom:0.5rem">' +
        '<input type="checkbox" id="n'+i+'enabled" name="n'+i+'enabled"' + (src.enabled ? ' checked' : '') + '>' +
        '<label for="n'+i+'enabled" style="font-weight:600">' + SOURCE_LABELS[i] + '</label>' +
      '</div>' +
      '<div id="n'+i+'fields" style="display:' + (src.enabled ? '' : 'none') + '">' +
        '<div class="row">' +
          '<div><label>Caster Host</label>' +
            '<input type="text" name="n'+i+'host" id="n'+i+'host" value="' + (src.host||'') + '" placeholder="192.168.8.195 (no http://)" maxlength="127">' +
            '<div style="font-size:0.7rem;color:#5a7a9a;margin-top:3px">Hostname or IP only; URL prefixes are removed automatically.</div></div>' +
          '<div><label>Port</label>' +
            '<input type="number" name="n'+i+'port" id="n'+i+'port" value="' + (src.port||2101) + '" min="1" max="65535"></div>' +
        '</div>' +
        '<label>Mountpoint</label>' +
        '<input type="text" name="n'+i+'mount" id="n'+i+'mount" value="' + (src.mount||'') + '" placeholder="MOUNTPOINT" maxlength="63">' +
        '<div class="row">' +
          '<div><label>Username</label>' +
            '<input type="text" name="n'+i+'user" id="n'+i+'user" value="' + (src.user||'') + '" maxlength="63"></div>' +
          '<div><label>Password</label>' +
            '<input type="password" name="n'+i+'pass" id="n'+i+'pass" maxlength="63" placeholder="(unchanged)"></div>' +
        '</div>' +
      '</div>';
    container.appendChild(div);
    document.getElementById('n'+i+'enabled').addEventListener('change', function() {
      document.getElementById('n'+i+'fields').style.display = this.checked ? '' : 'none';
    });
  });
}

function loadConfig() {
  fetch('/config').then(function(r) {
    if (r.status === 401) { showAuthWall('config'); return null; }
    hideAuthWall('config');
    return r.json();
  }).then(d => {
    if (!d) return;
    document.getElementById('apMode').checked = d.apMode;
    document.getElementById('wifiSSID').value  = d.wifiSSID || '';
    document.getElementById('headingOffset').value = d.headingOffset != null ? d.headingOffset : 90;
    document.getElementById('cogMinSog').value = d.cogMinSog != null ? d.cogMinSog : 0.1;
    document.getElementById('gpsUpdateRate').value = d.gpsUpdateRate != null ? String(d.gpsUpdateRate) : '1';
    var bleCb = document.getElementById('bleNmea');
    if (bleCb) bleCb.checked = d.bleNmea || false;
    document.getElementById('apSSID').value    = d.apSSID || '';
    buildNtripSources(d.ntrip || [{},{},{}]);
    toggleAPFields();
  });
}

document.getElementById('apMode').addEventListener('change', toggleAPFields);

function toggleAPFields() {
  const ap = document.getElementById('apMode').checked;
  document.getElementById('apFields').style.display   = ap ? '' : 'none';
  document.getElementById('wifiFields').style.display = ap ? 'none' : '';
}

document.getElementById('configForm').addEventListener('submit', function(e) {
  e.preventDefault();
  const pw1 = document.getElementById('adminPassword').value;
  const pw2 = document.getElementById('adminPassword2').value;
  if (pw1 !== pw2) { toast('Passwords do not match', false); return; }
  const fd = new FormData(this);
  const data = {};
  for (const [k, v] of fd.entries()) data[k] = v;
  // Remove confirm field — only send adminPassword if non-empty
  delete data['adminPassword2'];
  if (!pw1) delete data['adminPassword'];
  data.apMode = document.getElementById('apMode').checked ? 'true' : 'false';
  // Ensure unchecked source checkboxes are sent as false
  for (let i = 0; i < 3; i++) {
    const cb = document.getElementById('n' + i + 'enabled');
    if (cb) data['n' + i + 'enabled'] = cb.checked ? 'true' : 'false';
  }
  fetch('/config/save', {
    method: 'POST',
    headers: {'Content-Type':'application/x-www-form-urlencoded'},
    body: new URLSearchParams(data).toString()
  }).then(r=>r.json()).then(d => {
    if (d.ok) toast('Saved \u2014 restarting...');
    else toast('Error saving', false);
  }).catch(() => toast('Error saving', false));
});

function saveBleNmea(cb) {
  var data = new URLSearchParams();
  data.append('bleNmea', cb.checked ? 'true' : 'false');
  fetch('/ble/toggle', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body: data.toString()})
    .then(function(r) { return r.json(); })
    .then(function(d) {
      if (d.ok) toast('BLE setting saved — restarting...');
      else toast('Error saving BLE setting', false);
    }).catch(function() { toast('Error saving BLE setting', false); });
}
function doLogout() {
  // Bust the browser's Basic Auth cache by sending a request with bogus credentials.
  // The server always returns 401 for /logout, which causes the browser to discard
  // the cached credentials so the next page visit requires re-entering the password.
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/logout', true, '__logged_out__', '__logged_out__');
  xhr.onloadend = function() { window.location.href = '/'; };
  xhr.send();
}

// Auto-logout when the tab or browser is closed
window.addEventListener('beforeunload', function() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/logout', false, '__logged_out__', '__logged_out__'); // sync so it fires before unload
  try { xhr.send(); } catch(e) {}
});

function doRestart() {
  if (!confirm('Restart the device?')) return;
  fetch('/restart', {method:'POST'}).then(() => toast('Restarting...')).catch(()=>{});
}
function doUM982Reset() {
  if (!confirm('Factory reset the UM982? This takes ~15 seconds and will temporarily lose fix.')) return;
  toast('UM982 reset in progress (~15s)...');
  fetch('/um982reset', {method:'POST'}).catch(()=>{});
}

var c6PollTimer = null;
var c6ReloadScheduled = false;

function startC6Polling() {
  if (!c6PollTimer) c6PollTimer = setInterval(loadC6Status, 750);
}

function stopC6Polling() {
  if (c6PollTimer) clearInterval(c6PollTimer);
  c6PollTimer = null;
}

function loadC6Status() {
  fetch('/c6/status', {cache:'no-store'}).then(function(r) {
    if (r.status === 401) {
      stopC6Polling();
      showAuthWall('system');
      throw new Error('Login required');
    }
    if (!r.ok) throw new Error('Status request failed (' + r.status + ')');
    return r.json();
  }).then(function(d) {
    setText('c6RunningVersion', d.runningVersion || 'Unknown');
    setText('c6AvailableVersion', d.availableVersion || 'Invalid');
    setText('c6ImageDetails',
      (d.availableDate ? 'Built ' + d.availableDate + ' · ' : '') +
      formatBytes(d.imageBytes || 0) +
      (d.activateSupported ? ' · Remote activation supported' : ' · Legacy activation'));

    var button = document.getElementById('c6UpdateButton');
    var progress = document.getElementById('c6Progress');
    var bar = document.getElementById('c6Bar');
    var status = document.getElementById('c6Status');
    var pct = Math.max(0, Math.min(100, Number(d.progress) || 0));

    button.disabled = !!d.updating || !d.imageValid;
    button.textContent = d.updating ? 'Updating ESP32-C6…' : 'Update ESP32-C6';
    progress.style.display = (d.updating || d.error) ? '' : 'none';
    bar.style.width = pct + '%';
    bar.style.background = d.error ? '#ff6b6b' : '#67c5ff';
    status.textContent = d.error ? d.error : (d.phase + (d.updating && pct < 100 ? ' · ' + pct + '%' : ''));
    status.style.color = d.error ? '#ff8585' : '#a9bfd2';

    if (d.phase === 'Restarting device' && !c6ReloadScheduled) {
      c6ReloadScheduled = true;
      status.textContent = 'Update complete. WiFi is restarting; reconnecting shortly…';
      setTimeout(function() { window.location.reload(); }, 10000);
    }
  }).catch(function(e) {
    if (c6ReloadScheduled) {
      setText('c6Status', 'WiFi is restarting; reconnecting shortly…');
    } else {
      console.error('C6 status error:', e);
    }
  });
}

function startC6Update() {
  var running = document.getElementById('c6RunningVersion').textContent;
  var available = document.getElementById('c6AvailableVersion').textContent;
  if (!confirm('Update the ESP32-C6 from ' + running + ' to bundled ' + available +
      '?\n\nWiFi will disconnect and the Sailing Computer will restart.')) return;

  var button = document.getElementById('c6UpdateButton');
  button.disabled = true;
  document.getElementById('c6Progress').style.display = '';
  setText('c6Status', 'Starting update…');

  fetch('/c6/update', {method:'POST'}).then(function(r) {
    if (!r.ok) return r.text().then(function(t) { throw new Error(t || 'Update request failed'); });
    startC6Polling();
    loadC6Status();
  }).catch(function(e) {
    button.disabled = false;
    var status = document.getElementById('c6Status');
    status.textContent = e.message || 'Could not start update';
    status.style.color = '#ff8585';
  });
}

function startOTA(e) {
  e.preventDefault();
  var file = document.getElementById('otaFile').files[0];
  if (!file) { alert('Please choose a firmware.bin file first.'); return; }
  if (!confirm('Flash ' + file.name + ' (' + (file.size/1024).toFixed(1) + ' KB)?\nThe device will restart after flashing.')) return;

  var form = document.getElementById('otaForm');
  var progress = document.getElementById('otaProgress');
  var bar = document.getElementById('otaBar');
  var status = document.getElementById('otaStatus');
  progress.style.display = 'block';

  var xhr = new XMLHttpRequest();
  xhr.open('POST', '/update');
  xhr.setRequestHeader('Content-Type', 'application/octet-stream');
  xhr.upload.onprogress = function(e) {
    if (e.lengthComputable) {
      var pct = Math.round(e.loaded / e.total * 100);
      bar.style.width = pct + '%';
      status.textContent = 'Uploading... ' + pct + '%';
    }
  };
  xhr.onload = function() {
    if (xhr.status === 200) {
      bar.style.width = '100%';
      bar.style.background = '#4ade80';
      status.textContent = 'Flash successful! Device restarting...';
      document.open(); document.write(xhr.responseText); document.close();
    } else {
      bar.style.background = '#ff6b6b';
      status.textContent = 'Flash failed! (' + xhr.status + ')';
      document.open(); document.write(xhr.responseText); document.close();
    }
  };
  xhr.onerror = function() {
    bar.style.background = '#ff6b6b';
    status.textContent = 'Upload error — check connection.';
  };
  xhr.send(file);
}

// ── Start / Race Sequence ─────────────────────────────────────────────────────

var raceStatusCache  = null;   // latest /status payload — updated by updateStatus()
var racePollTimer    = null;
var raceStateCache   = null;   // latest /race/state response
var raceCourseCache  = [];     // full course objects from /courses
var raceMarkMapCache = {};     // mark id → name map from /marks
var crewControlState = null;
var markClosingKey = '';
var markClosingSamples = [];
var markSmgFiltered = null;

// ── Compass ───────────────────────────────────────────────────────────────────
var compassHeading   = null;   // degrees, 0=North, from deviceorientation
var compassRafId     = null;

function onDeviceOrientation(e) {
  if (e.webkitCompassHeading != null) {
    compassHeading = e.webkitCompassHeading;           // iOS: true bearing 0-360
  } else if (e.alpha != null) {
    compassHeading = (360 - e.alpha + 360) % 360;     // Android approximation
  }
}

function startCompassListener() {
  window.addEventListener('deviceorientation', onDeviceOrientation, true);
  document.getElementById('compassEnableBtn').style.display = 'none';
  if (!compassRafId) drawCompassLoop();
}

function requestCompassPermission() {
  if (typeof DeviceOrientationEvent.requestPermission === 'function') {
    DeviceOrientationEvent.requestPermission().then(function(s) {
      if (s === 'granted') startCompassListener();
    }).catch(function() {});
  } else {
    startCompassListener();
  }
}

function initRaceCompass() {
  if (!compassRafId) drawCompassLoop();
  if (typeof DeviceOrientationEvent === 'undefined') {
    document.getElementById('compassUnavail').style.display = '';
    return;
  }
  if (typeof DeviceOrientationEvent.requestPermission === 'function') {
    // iOS 13+: must be triggered by a user gesture — show button
    document.getElementById('compassEnableBtn').style.display = '';
  } else {
    startCompassListener();
  }
}

function stopRaceCompass() {
  if (compassRafId) cancelAnimationFrame(compassRafId);
  compassRafId = null;
}

function drawCompassLoop() {
  drawCompass();
  compassRafId = requestAnimationFrame(drawCompassLoop);
}

function drawRaceCompassVector(ctx, deg, length, color, width, dashed) {
  var a = deg * Math.PI / 180;
  var tx = Math.sin(a) * length;
  var ty = -Math.cos(a) * length;
  ctx.save();
  ctx.setLineDash(dashed ? [7, 5] : []);
  ctx.beginPath();
  ctx.moveTo(0, 0);
  ctx.lineTo(tx, ty);
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.stroke();
  ctx.setLineDash([]);

  var px = Math.cos(a), py = Math.sin(a);
  ctx.beginPath();
  ctx.moveTo(tx, ty);
  ctx.lineTo(tx - Math.sin(a) * 13 + px * 6, ty + Math.cos(a) * 13 + py * 6);
  ctx.lineTo(tx - Math.sin(a) * 13 - px * 6, ty + Math.cos(a) * 13 - py * 6);
  ctx.closePath();
  ctx.fillStyle = color;
  ctx.fill();
  ctx.restore();
}

function drawCompass() {
  var canvas = document.getElementById('compassCanvas');
  if (!canvas) return;
  var ctx = canvas.getContext('2d');
  var w = canvas.width, h = canvas.height;
  var cx = w / 2, cy = h / 2, r = cx - 8;
  ctx.clearRect(0, 0, w, h);

  var phoneHdg = compassHeading != null ? compassHeading : 0;
  var state  = raceStateCache;
  var status = raceStatusCache;
  var boatHdg = status && status.hdtValid ? Number(status.heading) : null;
  var heel = status && status.rollValid ? Number(status.roll) : null;
  var cmg = status && status.cogValid ? Number(status.cog) : null;
  var markBrg = null;
  if (state && status && state.state === 'racing' && state.nextMark &&
      state.nextMark.lat != null && state.nextMark.lon != null &&
      status.lat != null && status.lon != null) {
    markBrg = bearingDeg(status.lat, status.lon, state.nextMark.lat, state.nextMark.lon);
  }
  setText('raceCompassHeading', boatHdg != null ? Math.round(boatHdg) + '\u00b0' : '--\u00b0');
  setText('raceCompassCmg', cmg != null ? Math.round(cmg) + '\u00b0' : '--\u00b0');
  setText('raceCompassMark', markBrg != null ? Math.round(markBrg) + '\u00b0' : '--\u00b0');
  setText('raceCompassHeel', heel != null ?
    Math.abs(heel).toFixed(1) + '\u00b0 ' +
      (Math.abs(heel) < 0.5 ? 'LEVEL' : heel > 0 ? 'STBD' : 'PORT') :
    '--\u00b0');

  // Outer ring
  ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.fillStyle = '#060f1e'; ctx.fill();
  ctx.strokeStyle = '#1e4080'; ctx.lineWidth = 2; ctx.stroke();

  // Rotating rose
  ctx.save();
  ctx.translate(cx, cy);
  ctx.rotate(-phoneHdg * Math.PI / 180);

  // Tick marks
  for (var i = 0; i < 360; i += 5) {
    var a = i * Math.PI / 180;
    var big = (i % 30 === 0);
    var len = big ? 11 : (i % 10 === 0 ? 7 : 4);
    ctx.beginPath();
    ctx.moveTo(Math.sin(a) * (r - len), -Math.cos(a) * (r - len));
    ctx.lineTo(Math.sin(a) * (r - 1),  -Math.cos(a) * (r - 1));
    ctx.strokeStyle = big ? '#2e6ab0' : '#1a3a60';
    ctx.lineWidth = big ? 1.5 : 1;
    ctx.stroke();
  }

  // Cardinal labels
  [['N',0,'#e85a5a'],['E',90,'#c0d8f0'],['S',180,'#c0d8f0'],['W',270,'#c0d8f0']].forEach(function(c) {
    var a = c[1] * Math.PI / 180;
    ctx.font = 'bold ' + (c[0]==='N'?'15':'12') + 'px sans-serif';
    ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.fillStyle = c[2];
    ctx.fillText(c[0], Math.sin(a) * (r - 22), -Math.cos(a) * (r - 22));
  });

  // Absolute vectors rotate with the phone-up rose.
  if (cmg != null) drawRaceCompassVector(ctx, cmg, r - 42, '#ffbb45', 3, true);
  if (boatHdg != null) drawRaceCompassVector(ctx, boatHdg, r - 34, '#67c5ff', 4, false);
  if (markBrg != null) drawRaceCompassVector(ctx, markBrg, r - 25, '#4fd39a', 3, false);

  ctx.restore();

  // Fixed top indicator is the phone direction; boat heading is the blue vector.
  ctx.beginPath();
  ctx.moveTo(cx,     cy - r + 2);
  ctx.lineTo(cx - 8, cy - r + 16);
  ctx.lineTo(cx + 8, cy - r + 16);
  ctx.closePath(); ctx.fillStyle = '#d7e8f6'; ctx.fill();

  // Center dot
  ctx.beginPath(); ctx.arc(cx, cy, 3, 0, Math.PI * 2);
  ctx.fillStyle = '#e0e8f0'; ctx.fill();

  // Phone orientation readout; without permission the display remains north-up.
  ctx.font = '11px monospace'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
  ctx.fillStyle = compassHeading != null ? '#7a9ab8' : '#2a4a6a';
  ctx.fillText(compassHeading != null ? 'phone ' + Math.round(phoneHdg) + '°' : 'north up', cx, cy + r - 12);
}

function startRacePolling() {
  if (racePollTimer) return;
  racePollTimer = setInterval(function() {
    var activePage = document.querySelector('.page.active');
    if (!activePage || activePage.id !== 'start') {
      clearInterval(racePollTimer);
      racePollTimer = null;
      return;
    }
    loadRaceState();
  }, 500);
}

function loadRaceState() {
  fetch('/race/state').then(function(r) { return r.json(); }).then(function(d) {
    raceStateCache = d;
    renderRacePage(d);
  }).catch(function() {});
}

function fmtMS(ms) {
  var s = Math.floor(Math.abs(ms) / 1000);
  var m = Math.floor(s / 60); s = s % 60;
  return m + ':' + (s < 10 ? '0' : '') + s;
}

function fmtHMS(totalSec) {
  var h = Math.floor(totalSec / 3600);
  var m = Math.floor((totalSec % 3600) / 60);
  var s = totalSec % 60;
  return (h > 0 ? h + ':' + (m < 10 ? '0' : '') : '') + m + ':' + (s < 10 ? '0' : '') + s;
}

function renderRacePage(d) {
  var setupView    = document.getElementById('raceSetupView');
  var activeView   = document.getElementById('raceActiveView');
  var completeView = document.getElementById('raceCompleteView');
  if (!setupView) return;

  var state = d.state || 'idle';
  var now   = d.server_now_ms;
  var t0    = d.t0_ms;
  document.getElementById('start').classList.toggle('race-live', state !== 'idle');
  if (state !== 'racing') resetMarkClosingSpeed();

  setupView.style.display    = (state === 'idle')    ? '' : 'none';
  activeView.style.display   = (state === 'countdown' || state === 'racing') ? '' : 'none';
  completeView.style.display = (state === 'complete') ? '' : 'none';

  if (state === 'idle') {
    var durSec = d.duration_s || 300;
    var durMin = Math.floor(durSec / 60);
    document.getElementById('raceDurationDisplay').textContent = durMin + ':00';
    document.getElementById('armRaceBtn').textContent = 'ARM ' + durMin + ':00 START';
    ['5','10','15'].forEach(function(m) {
      var btn = document.getElementById('durBtn' + m);
      if (btn) btn.style.background = (durSec === parseInt(m) * 60) ? '#1e5080' : '';
    });
  }

  if (state === 'countdown' || state === 'racing') {
    var remainingMs = t0 - now;
    var clock      = document.getElementById('raceClock');
    var phaseLabel = document.getElementById('racePhaseLabel');
    var preInfo    = document.getElementById('racePreInfo');
    var markInfo   = document.getElementById('raceMarkInfo');
    var nextLegBtn = document.getElementById('raceNextLegBtn');
    var prevLegBtn = document.getElementById('racePrevLegBtn');
    var cancelBtn  = document.getElementById('raceCancelBtn');
    var endBtn     = document.getElementById('raceEndBtn');

    if (remainingMs > 0) {
      var secs = Math.ceil(remainingMs / 1000);
      clock.textContent      = fmtMS(remainingMs);
      phaseLabel.textContent = 'STARTING IN';
      clock.style.color      = secs <= 60  ? '#e85a5a' :
                               secs <= 240 ? '#f59e0b' : '#e0e8f0';
      preInfo.style.display  = '';
      markInfo.style.display = 'none';
      nextLegBtn.style.display = 'none';
      prevLegBtn.style.display = 'none';
      cancelBtn.style.display = '';
      endBtn.style.display = 'none';
      updateTTL(d);
    } else {
      var elapsed = -remainingMs;
      clock.textContent      = '+' + fmtHMS(Math.floor(elapsed / 1000));
      clock.style.color      = '#4caf82';
      phaseLabel.textContent = 'RACING';
      preInfo.style.display  = 'none';
      cancelBtn.style.display = 'none';
      endBtn.style.display = '';
      if (d.nextMark) {
        markInfo.style.display   = '';
        nextLegBtn.style.display = '';
        prevLegBtn.style.display = (d.legIdx > 0) ? '' : 'none';
        document.getElementById('raceNextMarkName').textContent = d.nextMark.name;
        updateMarkInfo(d.nextMark);
      } else {
        markInfo.style.display   = 'none';
        nextLegBtn.style.display = 'none';
        prevLegBtn.style.display = (d.legIdx > 0) ? '' : 'none';
        resetMarkClosingSpeed();
      }
    }
  }

  if (state === 'complete') {
    var elapsedSec = Math.max(0, Math.floor((d.end_ms - d.t0_ms) / 1000));
    document.getElementById('raceElapsedDisplay').textContent = fmtHMS(elapsedSec);

    // Course summary line
    var courseEl = document.getElementById('raceStatsCourse');
    if (courseEl) {
      var rounded = (d.legs && d.legs.length) || 0;
      var total   = d.courseTotalMarks || 0;
      var cname   = d.courseName ? escHtml(d.courseName) + ' &mdash; ' : '';
      courseEl.innerHTML = cname + rounded + (total ? ' of ' + total : '') + ' mark' + (total !== 1 ? 's' : '') + ' rounded';
    }

    // Leg splits table
    var legsEl = document.getElementById('raceStatsLegs');
    if (legsEl) {
      if (d.legs && d.legs.length) {
        var rows = d.legs.map(function(leg, i) {
          return '<div style="display:flex;justify-content:space-between;align-items:baseline;' +
                 (i > 0 ? 'border-top:1px solid #0d2244;' : '') + 'padding:5px 0">' +
            '<span style="color:#aac8e0;font-size:0.82rem">' + escHtml(leg.mark) + '</span>' +
            '<span style="font-variant-numeric:tabular-nums;font-size:0.9rem;color:#e0e8f0">' + fmtHMS(leg.elapsed_s) +
            ' <span style="color:#5a7a9a;font-size:0.75rem">(+' + fmtHMS(leg.split_s) + ')</span></span>' +
          '</div>';
        }).join('');
        legsEl.innerHTML = '<div style="font-size:0.7rem;text-transform:uppercase;letter-spacing:0.5px;color:#3a6a8a;margin-bottom:4px">Elapsed &nbsp;/&nbsp; Split</div>' + rows;
      } else {
        legsEl.innerHTML = '<div style="color:#5a7a9a;font-size:0.82rem">No marks rounded</div>';
      }
    }
  }

  // Start line status
  if (d.line) {
    var setEnds = 0;
    [0, 1].forEach(function(i) {
      var el = document.getElementById('lineStatus' + i);
      if (!el) return;
      var end = d.line[i];
      if (end && end.set) {
        setEnds++;
        el.style.color = '#4caf82';
        el.textContent = end.name + ' - set';
      } else {
        el.style.color = '#5a7a9a';
        el.textContent = 'Not set';
      }
    });
    var setupLine = document.getElementById('raceSetupLine');
    if (setupLine) {
      setupLine.textContent = setEnds === 2 ? 'Ready' : setEnds === 1 ? '1 end set' : 'Not set';
      setupLine.style.color = setEnds === 2 ? '#4fd39a' : '#ffbb45';
    }
  }

  // Course list highlight + detail + lap selector
  renderRaceCourseCard(d);
}

function markChip(name, port) {
  var rnd = port ? '<span style="font-size:0.65rem;color:#4caf82">P</span>' : '<span style="font-size:0.65rem;color:#e8a830">S</span>';
  return '<span style="display:inline-block;background:#0d2244;border:1px solid #1e4080;border-radius:4px;padding:1px 6px;margin:2px;font-size:0.78rem">' + escHtml(name) + ' ' + rnd + '</span>';
}

function renderRaceCourseCard(d) {
  var activeCourseId = d ? (d.courseId || '') : '';
  var activeLaps     = d ? (d.laps || 1) : 1;
  var state          = d ? (d.state || 'idle') : 'idle';

  // Expanded sequence for selected course
  var detailEl = document.getElementById('raceCourseDetail');
  var marksEl  = document.getElementById('raceCourseMarks');
  var lapSelEl = document.getElementById('raceLapSelector');
  var statusEl = document.getElementById('raceCourseStatus');

  var activeCourse = raceCourseCache.filter(function(c) { return c.id === activeCourseId; })[0] || null;

  if (activeCourse) {
    var setupCourse = document.getElementById('raceSetupCourse');
    if (setupCourse) setupCourse.textContent = activeCourse.name;
    // Build expanded mark list
    var raw = activeCourse.marks;
    var expanded = [];
    if (raw.length >= 3 && activeLaps > 1) {
      expanded.push(raw[0]);
      var interior = raw.slice(1, raw.length - 1);
      for (var lap = 0; lap < activeLaps; lap++) {
        interior.forEach(function(m) { expanded.push(m); });
      }
      expanded.push(raw[raw.length - 1]);
    } else {
      expanded = raw.slice();
    }

    if (marksEl) {
      marksEl.innerHTML = expanded.map(function(ref) {
        return markChip(raceMarkMapCache[ref.mark_id] || ref.mark_id, ref.port);
      }).join('<span style="color:#5a7a9a;margin:0 1px">&#8594;</span>');
    }
    if (detailEl) detailEl.style.display = '';
    if (lapSelEl) {
      lapSelEl.style.display = '';
      [1,2,3,4,5].forEach(function(n) {
        var btn = document.getElementById('lapBtn' + n);
        if (btn) btn.style.background = (activeLaps === n) ? '#1e5080' : '';
      });
    }
    if (statusEl) {
      if (state === 'racing') {
        statusEl.style.color = '#4caf82';
        statusEl.textContent = 'Active — leg ' + ((d.legIdx || 0) + 1) + ' of ' + expanded.length;
      } else if (state === 'idle') {
        statusEl.style.color = '#4caf82';
        var lapLabel = activeLaps > 1 ? ' · ' + activeLaps + ' laps' : '';
        statusEl.textContent = activeCourse.name + lapLabel + ' — ' + expanded.length + ' marks';
      } else {
        statusEl.style.color = '#4caf82';
        statusEl.textContent = 'Active — ' + activeCourse.name;
      }
    }
  } else {
    var setupCourse = document.getElementById('raceSetupCourse');
    if (setupCourse) setupCourse.textContent = 'None';
    if (detailEl) detailEl.style.display = 'none';
    if (lapSelEl) lapSelEl.style.display = 'none';
    if (statusEl) { statusEl.style.color = '#5a7a9a'; statusEl.textContent = 'No course selected'; }
  }
}

function updateTTL(raceD) {
  var sog = raceStatusCache ? (raceStatusCache.sog || 0) : 0;
  document.getElementById('raceTTLSog').textContent = sog.toFixed(1) + ' kts';
  var line = raceD ? raceD.line : null;
  if (!line || !line[0].set || !line[1].set || !raceStatusCache) {
    document.getElementById('raceTTL').textContent = '--:--';
    return;
  }
  var lat = raceStatusCache.lat, lon = raceStatusCache.lon;
  if (!lat && !lon) { document.getElementById('raceTTL').textContent = '--:--'; return; }
  var distNm = distToSegNm(lat, lon, line[0].lat, line[0].lon, line[1].lat, line[1].lon);
  if (sog < 0.1) { document.getElementById('raceTTL').textContent = '--:--'; return; }
  var ttlSec = Math.round((distNm / sog) * 3600);
  document.getElementById('raceTTL').textContent = fmtHMS(ttlSec);
}

function resetMarkClosingSpeed() {
  markClosingKey = '';
  markClosingSamples = [];
  markSmgFiltered = null;
}

function measuredSmg(markKey, distanceNm) {
  if (markClosingKey !== markKey) {
    resetMarkClosingSpeed();
    markClosingKey = markKey;
  }

  var now = Date.now();
  var last = markClosingSamples.length ? markClosingSamples[markClosingSamples.length - 1] : null;
  if (last && now - last.time > 3000) {
    resetMarkClosingSpeed();
    markClosingKey = markKey;
    last = null;
  }
  if (!last || now - last.time >= 400)
    markClosingSamples.push({time:now, distance:distanceNm});
  markClosingSamples = markClosingSamples.filter(function(sample) {
    return now - sample.time <= 10000;
  });

  var anchor = null;
  for (var i = 0; i < markClosingSamples.length; i++) {
    if (now - markClosingSamples[i].time >= 4000) {
      anchor = markClosingSamples[i];
      break;
    }
  }
  if (!anchor) return markSmgFiltered;

  var elapsedHours = (now - anchor.time) / 3600000;
  if (elapsedHours <= 0) return markSmgFiltered;
  var rawSmg = (anchor.distance - distanceNm) / elapsedHours;
  if (!isFinite(rawSmg) || Math.abs(rawSmg) > 60) return markSmgFiltered;
  markSmgFiltered = markSmgFiltered == null ? rawSmg :
    markSmgFiltered * 0.7 + rawSmg * 0.3;
  return markSmgFiltered;
}

function updateMarkInfo(mark) {
  if (!raceStatusCache || !mark) return;
  var lat = Number(raceStatusCache.lat), lon = Number(raceStatusCache.lon);
  if (!raceStatusCache.fix || !isFinite(lat) || !isFinite(lon)) {
    resetMarkClosingSpeed();
    setText('raceMarkDist', '-- nm');
    setText('raceMarkBrg', '--\u00b0');
    setText('raceMarkCmg', '--\u00b0');
    setText('raceMarkVmg', '-- kt');
    setText('raceMarkSmg', '-- kt');
    setText('raceMarkTTM', '--:--');
    return;
  }

  var sog = Number(raceStatusCache.sog) || 0;
  var dist = haversineNm(lat, lon, mark.lat, mark.lon);
  var brg  = bearingDeg(lat, lon, mark.lat, mark.lon);
  var cmg = raceStatusCache.cogValid ? Number(raceStatusCache.cog) : null;
  var vmg = null;
  if (cmg != null && isFinite(cmg)) {
    var courseError = ((cmg - brg + 540) % 360) - 180;
    vmg = sog * Math.cos(courseError * Math.PI / 180);
  }

  var markKey = String(mark.lat) + ',' + String(mark.lon);
  var smg = measuredSmg(markKey, dist);
  document.getElementById('raceMarkDist').textContent = dist.toFixed(2) + ' nm';
  document.getElementById('raceMarkBrg').textContent  = Math.round(brg) + '\u00b0 T';
  document.getElementById('raceMarkCmg').textContent =
    cmg != null && isFinite(cmg) ? Math.round(cmg) + '\u00b0 T' : '--\u00b0';
  document.getElementById('raceMarkVmg').textContent =
    vmg != null ? vmg.toFixed(1) + ' kt' : '-- kt';
  document.getElementById('raceMarkSmg').textContent =
    smg != null ? smg.toFixed(1) + ' kt' : '-- kt';

  var smgEl = document.getElementById('raceMarkSmg');
  smgEl.style.color = smg == null ? '#e0e8f0' : (smg > 0.1 ? '#4fd39a' : '#ffbb45');
  var vmgEl = document.getElementById('raceMarkVmg');
  vmgEl.style.color = vmg == null ? '#e0e8f0' : (vmg > 0.1 ? '#4fd39a' : '#ffbb45');

  document.getElementById('raceMarkTTM').textContent =
    smg != null && smg > 0.1 ? fmtHMS(Math.round((dist / smg) * 3600)) : '--:--';
}

function haversineNm(lat1, lon1, lat2, lon2) {
  var R    = 3440.065;
  var dLat = (lat2 - lat1) * Math.PI / 180;
  var dLon = (lon2 - lon1) * Math.PI / 180;
  var a    = Math.sin(dLat/2)*Math.sin(dLat/2) +
             Math.cos(lat1*Math.PI/180)*Math.cos(lat2*Math.PI/180)*
             Math.sin(dLon/2)*Math.sin(dLon/2);
  return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
}

function bearingDeg(lat1, lon1, lat2, lon2) {
  var y = Math.sin((lon2-lon1)*Math.PI/180)*Math.cos(lat2*Math.PI/180);
  var x = Math.cos(lat1*Math.PI/180)*Math.sin(lat2*Math.PI/180) -
          Math.sin(lat1*Math.PI/180)*Math.cos(lat2*Math.PI/180)*Math.cos((lon2-lon1)*Math.PI/180);
  return (Math.atan2(y, x) * 180 / Math.PI + 360) % 360;
}

function distToSegNm(plat, plon, alat, alon, blat, blon) {
  var clat = (alat + blat) / 2 * Math.PI / 180;
  var cos  = Math.cos(clat);
  var ax = alon*cos, ay = alat, bx = blon*cos, by = blat;
  var px = plon*cos, py = plat;
  var dx = bx-ax,    dy = by-ay;
  var lenSq = dx*dx + dy*dy;
  if (lenSq === 0) return haversineNm(plat, plon, alat, alon);
  var t  = Math.max(0, Math.min(1, ((px-ax)*dx + (py-ay)*dy) / lenSq));
  return haversineNm(plat, plon, ay + t*dy, (ax + t*dx) / cos);
}

function armRace() {
  fetch('/race/start', {method:'POST'}).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) loadRaceState(); else toast('Arm failed: ' + (d.err || ''), false);
  }).catch(function() { toast('Request failed', false); });
}

function endRace() {
  if (!confirm('Finish this race and show the results?')) return;
  fetch('/race/end', {method:'POST'}).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) loadRaceState(); else toast('Failed: ' + (d.err || ''), false);
  }).catch(function() { toast('Request failed', false); });
}

function resetRace() {
  fetch('/race/stop', {method:'POST'}).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) loadRaceState(); else toast('Reset failed', false);
  }).catch(function() { toast('Request failed', false); });
}

function syncRace() {
  fetch('/race/sync', {method:'POST'}).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) { loadRaceState(); toast('Synced to nearest minute'); }
    else toast('Sync failed: ' + (d.err || ''), false);
  }).catch(function() { toast('Request failed', false); });
}

function adjustDuration(deltaSec) {
  var cur  = raceStateCache ? raceStateCache.duration_s : 300;
  var next = Math.max(60, Math.min(1800, cur + deltaSec));
  fetch('/race/duration', {method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({seconds: next})
  }).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) loadRaceState();
  }).catch(function() {});
}

function setDuration(sec) {
  fetch('/race/duration', {method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({seconds: sec})
  }).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) loadRaceState();
  }).catch(function() {});
}

function setLaps(n) {
  fetch('/race/laps', {method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({laps: n})
  }).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) loadRaceState();
  }).catch(function() {});
}

function useGpsForLine(endIdx) {
  fetch('/status').then(function(r) { return r.json(); }).then(function(s) {
    if (!s.fix) { toast('No GPS fix', false); return; }
    var name = endIdx === 0 ? 'Port End' : 'Stbd End';
    fetch('/race/startline', {method:'POST',
      headers:{'Content-Type':'application/json'},
      body: JSON.stringify({end: endIdx, lat: s.lat, lon: s.lon, name: name})
    }).then(function(r) { return r.json(); }).then(function(d) {
      if (d.ok) { toast('Line end set'); loadRaceState(); }
      else toast('Failed to set line end', false);
    });
  }).catch(function() { toast('Failed to get GPS', false); });
}

function useMarkForLine(endIdx) {
  var sel    = document.getElementById('lineMarkSel' + endIdx);
  var markId = sel ? sel.value : '';
  if (!markId) return;
  fetch('/race/startline', {method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({end: endIdx, markId: markId})
  }).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) { toast('Line end set'); loadRaceState(); }
    else { toast('Mark not found', false); if (sel) sel.value = ''; }
  }).catch(function() { toast('Request failed', false); });
}

function setRaceCourse() {
  var sel      = document.getElementById('raceCourseSelect');
  var courseId = sel ? sel.value : '';
  fetch('/race/course', {method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({courseId: courseId, leg: 0})
  }).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) {
      toast(courseId ? 'Course set' : 'Course cleared');
      if (sel) sel.value = courseId;
      loadRaceState();
    } else toast('Failed', false);
  }).catch(function() { toast('Request failed', false); });
}

function nextLeg() {
  fetch('/race/nextleg', {method:'POST'}).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) { if (d.complete) toast('Race complete!'); loadRaceState(); }
    else toast('Failed: ' + (d.err || ''), false);
  }).catch(function() { toast('Request failed', false); });
}

function prevLeg() {
  fetch('/race/prevleg', {method:'POST'}).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) loadRaceState();
    else toast('Failed: ' + (d.err || ''), false);
  }).catch(function() { toast('Request failed', false); });
}

function toggleStartLine() {
  var body    = document.getElementById('startLineBody');
  var chevron = document.getElementById('startLineChevron');
  if (!body) return;
  var collapsed = body.style.display === 'none';
  body.style.display    = collapsed ? '' : 'none';
  chevron.style.transform = collapsed ? '' : 'rotate(-90deg)';
}

function loadRaceMarksAndCourses() {
  // Marks for start line pickers
  fetch('/marks').then(function(r) { return r.json(); }).then(function(marks) {
    [0, 1].forEach(function(i) {
      var sel = document.getElementById('lineMarkSel' + i);
      if (!sel) return;
      var cur = sel.value;
      while (sel.options.length > 1) sel.remove(1);
      marks.forEach(function(m) {
        var opt = document.createElement('option');
        opt.value = m.id; opt.textContent = m.name;
        sel.appendChild(opt);
      });
      if (cur) sel.value = cur;
    });
    populateCrewMarkSelects(marks);
  }).catch(function() {});

  // Courses + marks + race state fetched together so options exist before value is set
  Promise.all([
    fetch('/courses').then(function(r) { return r.json(); }),
    fetch('/marks').then(function(r) { return r.json(); }),
    fetch('/race/state').then(function(r) { return r.json(); })
  ]).then(function(res) {
    raceCourseCache = res[0]; var marks = res[1], state = res[2];
    raceMarkMapCache = {};
    marks.forEach(function(m) { raceMarkMapCache[m.id] = m.name; });
    raceStateCache = state;
    var sel = document.getElementById('raceCourseSelect');
    if (sel) {
      while (sel.options.length > 1) sel.remove(1);
      raceCourseCache.forEach(function(c) {
        var opt = document.createElement('option');
        opt.value = c.id;
        opt.textContent = c.name + ' (' + c.marks.length + ' marks)';
        sel.appendChild(opt);
      });
      sel.value = state.courseId || '';
    }
    renderRacePage(state);
  }).catch(function() {});
}

var crewFieldOptions = [
  ['auto', 'Automatic for view'],
  ['countdown', 'Race countdown / time'],
  ['distance_line', 'Distance to start line'],
  ['time_line', 'Time to start line'],
  ['speed', 'Boat speed'],
  ['heading', 'Heading'],
  ['course', 'Course over ground'],
  ['active_mark', 'Active mark'],
  ['bearing_mark', 'Bearing to mark'],
  ['distance_mark', 'Distance to mark'],
  ['vmg_mark', 'VMG to mark'],
  ['time_mark', 'Time to mark'],
  ['target_heading', 'Target heading'],
  ['next_mark', 'Next mark'],
  ['maneuver', 'Maneuver message'],
  ['status', 'Status message'],
  ['heel', 'Heel / roll'],
  ['leeway', 'Leeway'],
  ['drive_speed', 'Drive speed']
];

function initCrewPrioritySelects() {
  [0, 1, 2, 3].forEach(function(index) {
    var select = document.getElementById('crewPriority' + index);
    if (!select || select.options.length) return;
    crewFieldOptions.forEach(function(field) {
      var option = document.createElement('option');
      option.value = field[0];
      option.textContent = field[1];
      select.appendChild(option);
    });
  });
}

function populateCrewMarkSelects(marks) {
  ['crewActiveMark', 'crewNextMark'].forEach(function(id) {
    var select = document.getElementById(id);
    if (!select) return;
    var current = select.value;
    while (select.options.length > 1) select.remove(1);
    marks.forEach(function(mark) {
      var option = document.createElement('option');
      option.value = mark.id;
      option.textContent = mark.name;
      select.appendChild(option);
    });
    select.value = current;
  });
  renderCrewControlState();
}

function renderCrewControlState() {
  if (!crewControlState) return;
  initCrewPrioritySelects();
  var d = crewControlState;
  document.getElementById('crewPhase').value = d.phase || 'prestart';
  document.getElementById('crewMode').value = d.mode || 'auto';
  document.getElementById('crewStartTarget').value = d.startTarget || 'line';
  document.getElementById('crewActiveMark').value = d.activeMarkId || '';
  document.getElementById('crewNextMark').value = d.nextMarkId || '';
  document.getElementById('crewTargetHeading').value =
    Number(d.targetHeading) >= 0 ? d.targetHeading : '';
  document.getElementById('crewManeuver').value = d.maneuver || '';
  document.getElementById('crewStatus').value = d.status || '';
  document.getElementById('crewLocked').checked = !!d.locked;
  (d.priorities || ['auto', 'auto', 'auto', 'auto']).forEach(function(field, index) {
    var select = document.getElementById('crewPriority' + index);
    if (select) select.value = field || 'auto';
  });
  document.getElementById('crewControlStatus').textContent =
    'Shared state revision ' + d.revision + (d.locked ? ' - local view changes locked' : ' - local views allowed');
}

function loadCrewControlState() {
  initCrewPrioritySelects();
  fetch('/crew/state', {cache:'no-store'}).then(function(r) {
    return r.json();
  }).then(function(d) {
    crewControlState = {
      revision: d.revision,
      phase: d.phase,
      mode: d.mode,
      startTarget: d.startTarget,
      targetHeading: d.targetHeading,
      maneuver: d.maneuver,
      status: d.status,
      locked: d.locked,
      priorities: d.priorities,
      activeMarkId: d.activeMarkId || '',
      nextMarkId: d.nextMarkId || ''
    };
    renderCrewControlState();
  }).catch(function() {
    document.getElementById('crewControlStatus').textContent =
      'Could not load crew display state';
  });
}

function pushCrewDisplay() {
  var headingText = document.getElementById('crewTargetHeading').value.trim();
  var heading = headingText === '' ? -1 : parseInt(headingText, 10);
  if (headingText !== '' && (!isFinite(heading) || heading < 0 || heading > 359)) {
    toast('Target heading must be 0-359', false);
    return;
  }
  var payload = {
    phase: document.getElementById('crewPhase').value,
    mode: document.getElementById('crewMode').value,
    activeMarkId: document.getElementById('crewActiveMark').value,
    nextMarkId: document.getElementById('crewNextMark').value,
    startTarget: document.getElementById('crewStartTarget').value,
    targetHeading: heading,
    maneuver: document.getElementById('crewManeuver').value.trim(),
    status: document.getElementById('crewStatus').value.trim(),
    locked: document.getElementById('crewLocked').checked,
    priorities: [0, 1, 2, 3].map(function(index) {
      return document.getElementById('crewPriority' + index).value;
    })
  };
  var status = document.getElementById('crewControlStatus');
  status.textContent = 'Pushing update...';
  fetch('/crew/state', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify(payload)
  }).then(function(r) { return r.json(); }).then(function(d) {
    if (!d.ok) throw new Error('Update failed');
    status.textContent = 'Pushed to crew displays - revision ' + d.revision;
    toast('Crew displays updated');
    loadCrewControlState();
  }).catch(function() {
    status.textContent = 'Crew display update failed';
    toast('Crew display update failed', false);
  });
}

// ── Marks / Routes ────────────────────────────────────────────────────────────
function loadMarks() {
  fetch('/marks').then(r => r.json()).then(function(marks) {
    var el = document.getElementById('markList');
    if (!marks.length) {
      el.innerHTML = '<p style="color:#5a7a9a;font-size:0.85rem">No marks saved.</p>';
      return;
    }
    var rows = marks.map(function(m) {
      return '<div class="mark-row">' +
        '<div><div class="mark-name">' + escHtml(m.name) + '</div>' +
        '<div class="mark-coords">' + m.lat.toFixed(7) + ', ' + m.lon.toFixed(7) +
        ' &nbsp; ' + escHtml(m.id) + '</div></div>' +
        '<button class="btn btn-danger" aria-label="Delete ' + escHtml(m.name) +
        '" onclick="deleteMark(\'' + escHtml(m.id) + '\')">Delete</button>' +
        '</div>';
    }).join('');
    el.innerHTML = rows;
  }).catch(function() {
    document.getElementById('markList').innerHTML = '<p style="color:#e85a5a;font-size:0.85rem">Failed to load marks.</p>';
  });
}

function addMark() {
  var name = document.getElementById('mkName').value.trim();
  var lat  = parseFloat(document.getElementById('mkLat').value);
  var lon  = parseFloat(document.getElementById('mkLon').value);
  if (!name) { toast('Enter a mark name', false); return; }
  if (isNaN(lat) || isNaN(lon)) { toast('Enter valid lat/lon', false); return; }
  fetch('/marks', { method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({name:name, lat:lat, lon:lon})
  }).then(r => r.json()).then(function(res) {
    if (res.ok) {
      toast('Mark saved (' + res.id + ')');
      document.getElementById('mkName').value = '';
      document.getElementById('mkLat').value  = '';
      document.getElementById('mkLon').value  = '';
      loadMarks();
      loadCourses();
      loadRaceMarksAndCourses();
    } else { toast('Save failed', false); }
  }).catch(function() { toast('Request failed', false); });
}

function deleteMark(id) {
  if (!confirm('Delete mark ' + id + '?')) return;
  fetch('/marks/delete', { method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({id:id})
  }).then(parseJsonResponse).then(function() {
    toast('Mark deleted');
    loadMarks();
    loadCourses();
    loadRaceMarksAndCourses();
  }).catch(function(err) {
    toast(err.message || 'Could not delete mark', false);
  });
}

function useGpsForMark() {
  fetch('/status').then(r => r.json()).then(function(s) {
    if (!s.lat && !s.lon) { toast('No GPS fix', false); return; }
    document.getElementById('mkLat').value = s.lat.toFixed(7);
    document.getElementById('mkLon').value = s.lon.toFixed(7);
    toast('GPS position loaded');
  }).catch(function() { toast('Failed to get GPS', false); });
}

var coursePageCache = [];
var courseEditorMarkCache = [];
var courseEditorId = '';
var courseEditorRefs = [];

function loadCourses() {
  Promise.all([
    fetch('/courses').then(function(r) { return r.json(); }),
    fetch('/marks').then(function(r) { return r.json(); })
  ]).then(function(res) {
    coursePageCache = res[0];
    courseEditorMarkCache = res[1];
    renderCourseList();
    if (document.getElementById('courseEditor').style.display !== 'none')
      renderCourseEditorMarks();
  }).catch(function() {
    document.getElementById('courseList').innerHTML =
      '<p style="color:#e85a5a;font-size:0.85rem">Failed to load courses.</p>';
  });
}

function renderCourseList() {
  var el = document.getElementById('courseList');
  if (!coursePageCache.length) {
    el.innerHTML = '<p style="color:#5a7a9a;font-size:0.85rem">No courses saved. Create one from the mark library or import a GPX route.</p>';
    return;
  }

  var markMap = {};
  courseEditorMarkCache.forEach(function(m) { markMap[m.id] = m.name; });
  el.innerHTML = coursePageCache.map(function(c) {
    var markNames = c.marks.map(function(ref) {
      var n = markMap[ref.mark_id] || ref.mark_id;
      var rnd = ref.port ?
        '<span style="font-size:0.68rem;color:#4caf82">P</span>' :
        '<span style="font-size:0.68rem;color:#e8a830">S</span>';
      return '<span style="display:inline-block;background:#0d2244;border:1px solid #1e4080;border-radius:4px;padding:2px 7px;margin:2px;font-size:0.8rem">' +
        escHtml(n) + ' ' + rnd + '</span>';
    }).join('<span style="color:#5a7a9a;margin:0 2px">&#8594;</span>');

    return '<div class="course-list-item">' +
      '<div style="font-weight:bold;color:#edf5fc;font-size:1rem;margin-bottom:6px">' + escHtml(c.name) +
      ' <span style="font-size:0.72rem;color:#7895af">(' + c.marks.length + ' marks)</span></div>' +
      '<div style="line-height:2">' + (markNames || '<em style="color:#5a7a9a">No marks</em>') + '</div>' +
      '<div class="course-list-actions">' +
        '<button type="button" class="btn" onclick="editCourse(\'' + escHtml(c.id) + '\')">Edit</button>' +
        '<button type="button" class="btn btn-danger" onclick="deleteCourse(\'' + escHtml(c.id) + '\')">Delete</button>' +
      '</div>' +
    '</div>';
  }).join('');
}

function newCourse() {
  if (!courseEditorMarkCache.length) {
    toast('Add at least one mark before creating a course', false);
    return;
  }
  courseEditorId = '';
  courseEditorRefs = [{mark_id:courseEditorMarkCache[0].id, port:true}];
  document.getElementById('courseEditName').value = '';
  document.getElementById('courseEditorTitle').textContent = 'New Course';
  document.getElementById('courseEditor').style.display = '';
  renderCourseEditorMarks();
  document.getElementById('courseEditName').focus();
}

function editCourse(id) {
  var course = coursePageCache.filter(function(c) { return c.id === id; })[0];
  if (!course) { toast('Course not found', false); return; }
  courseEditorId = course.id;
  courseEditorRefs = course.marks.map(function(ref) {
    return {mark_id:ref.mark_id, port:ref.port !== false};
  });
  document.getElementById('courseEditName').value = course.name || '';
  document.getElementById('courseEditorTitle').textContent = 'Edit Course';
  document.getElementById('courseEditor').style.display = '';
  renderCourseEditorMarks();
  document.getElementById('courseEditor').scrollIntoView({behavior:'smooth', block:'start'});
}

function cancelCourseEdit() {
  courseEditorId = '';
  courseEditorRefs = [];
  document.getElementById('courseEditor').style.display = 'none';
}

function addCourseMark() {
  if (courseEditorRefs.length >= 12) {
    toast('A course can contain at most 12 marks', false);
    return;
  }
  if (!courseEditorMarkCache.length) {
    toast('No marks available', false);
    return;
  }
  courseEditorRefs.push({mark_id:courseEditorMarkCache[0].id, port:true});
  renderCourseEditorMarks();
}

function setCourseMark(index, markId) {
  if (courseEditorRefs[index]) courseEditorRefs[index].mark_id = markId;
}

function toggleCourseRounding(index) {
  if (!courseEditorRefs[index]) return;
  courseEditorRefs[index].port = !courseEditorRefs[index].port;
  renderCourseEditorMarks();
}

function moveCourseMark(index, delta) {
  var target = index + delta;
  if (target < 0 || target >= courseEditorRefs.length) return;
  var ref = courseEditorRefs[index];
  courseEditorRefs[index] = courseEditorRefs[target];
  courseEditorRefs[target] = ref;
  renderCourseEditorMarks();
}

function removeCourseMark(index) {
  courseEditorRefs.splice(index, 1);
  renderCourseEditorMarks();
}

function renderCourseEditorMarks() {
  var el = document.getElementById('courseEditorMarks');
  if (!courseEditorRefs.length) {
    el.innerHTML = '<p style="color:#ffbb45;font-size:0.82rem;padding:8px 0">Add at least one mark.</p>';
    return;
  }

  el.innerHTML = courseEditorRefs.map(function(ref, index) {
    var options = courseEditorMarkCache.map(function(mark) {
      return '<option value="' + escHtml(mark.id) + '"' +
        (mark.id === ref.mark_id ? ' selected' : '') + '>' + escHtml(mark.name) + '</option>';
    }).join('');
    var roundColor = ref.port ? '#4fd39a' : '#ffbb45';
    var roundText = ref.port ? 'PORT' : 'STBD';
    return '<div class="course-mark-edit">' +
      '<div class="course-mark-line">' +
        '<select aria-label="Course mark ' + (index + 1) + '" onchange="setCourseMark(' + index + ',this.value)">' +
          options +
        '</select>' +
        '<span style="color:#7895af;font-weight:800">#' + (index + 1) + '</span>' +
      '</div>' +
      '<div class="course-mark-controls">' +
        '<button type="button" class="btn course-rounding" style="color:' + roundColor + '" onclick="toggleCourseRounding(' + index + ')">' + roundText + '</button>' +
        '<button type="button" class="btn" aria-label="Move mark up" onclick="moveCourseMark(' + index + ',-1)"' + (index === 0 ? ' disabled' : '') + '>&uarr;</button>' +
        '<button type="button" class="btn" aria-label="Move mark down" onclick="moveCourseMark(' + index + ',1)"' + (index === courseEditorRefs.length - 1 ? ' disabled' : '') + '>&darr;</button>' +
        '<button type="button" class="btn btn-danger" style="margin-left:auto" aria-label="Remove mark" onclick="removeCourseMark(' + index + ')">Remove</button>' +
      '</div>' +
    '</div>';
  }).join('');
}

function parseJsonResponse(r) {
  return r.text().then(function(text) {
    var data = {};
    try { data = text ? JSON.parse(text) : {}; } catch(e) {}
    if (!r.ok || data.ok === false)
      throw new Error(data.error || text || ('Request failed (' + r.status + ')'));
    return data;
  });
}

function saveCourse() {
  var name = document.getElementById('courseEditName').value.trim();
  if (!name) { toast('Enter a course name', false); return; }
  if (!courseEditorRefs.length) { toast('Add at least one mark', false); return; }

  fetch('/courses', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({id:courseEditorId, name:name, marks:courseEditorRefs})
  }).then(parseJsonResponse).then(function(res) {
    toast(courseEditorId ? 'Course updated' : 'Course created');
    cancelCourseEdit();
    loadCourses();
    loadRaceMarksAndCourses();
  }).catch(function(err) {
    toast(err.message || 'Could not save course', false);
  });
}

function deleteCourse(id) {
  var course = coursePageCache.filter(function(c) { return c.id === id; })[0];
  if (!confirm('Delete course "' + (course ? course.name : id) + '"?')) return;
  fetch('/courses/delete', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({id:id})
  }).then(parseJsonResponse).then(function() {
    if (courseEditorId === id) cancelCourseEdit();
    toast('Course deleted');
    loadCourses();
    loadRaceMarksAndCourses();
  }).catch(function(err) {
    toast(err.message || 'Could not delete course', false);
  });
}

function importGpx() {
  var fileInput = document.getElementById('gpxFile');
  var resultEl  = document.getElementById('gpxResult');
  if (!fileInput.files.length) { toast('Select a GPX file first', false); return; }
  var file = fileInput.files[0];
  resultEl.style.display = 'block';
  resultEl.style.color   = '#7a9ab8';
  resultEl.textContent   = 'Importing ' + file.name + '…';

  fetch('/gpx/import', {
    method: 'POST',
    headers: { 'Content-Type': 'application/gpx+xml' },
    body: file
  }).then(r => r.json()).then(function(res) {
    if (res.ok) {
      resultEl.style.color = '#4caf82';
      resultEl.textContent = '✓ Done: ' + res.marks_found + ' waypoints found, ' +
        res.marks_added + ' marks added; ' + res.routes_found + ' routes found, ' +
        res.courses_added + ' courses added.';
      loadMarks();
      loadCourses();
    } else {
      resultEl.style.color = '#e85a5a';
      resultEl.textContent = 'Import failed.';
    }
  }).catch(function(err) {
    resultEl.style.color = '#e85a5a';
    resultEl.textContent = 'Request failed: ' + err;
  });
}

function loadStorageInfo() {
  fetch('/storage/info').then(r => r.json()).then(function(s) {
    var pct = s.total > 0 ? Math.round(s.used / s.total * 100) : 0;
    var backend = s.backend === 'sdcard' ? 'SD card' :
                  (s.backend === 'spiffs' ? 'Internal flash fallback' : 'Unavailable');
    if (!s.available) {
      document.getElementById('storageInfo').textContent = backend;
      return;
    }
    document.getElementById('storageInfo').innerHTML =
      '<span style="color:#5ab4e8">' + backend + '</span> &nbsp;&#8212;&nbsp; ' +
      '<span style="color:#e0e8f0">' + (s.used/1024).toFixed(1) + ' KB</span> used of ' +
      (s.total/1024).toFixed(0) + ' KB total &nbsp;&#8212;&nbsp; ' +
      '<span style="color:#4caf82">' + (s.free/1024).toFixed(1) + ' KB free</span> (' + pct + '%)';
    document.getElementById('storageBar').style.width = pct + '%';
    document.getElementById('storageBar').style.background = pct > 80 ? '#e85a5a' : '#5ab4e8';
  }).catch(function() {
    document.getElementById('storageInfo').textContent = 'Failed to load storage info.';
  });
}

function escHtml(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}

// ── Files page ────────────────────────────────────────────────────────────────
var fileBrowserPath = '/sdcard';
var fileEntries     = [];
var storageMountPt  = '/sdcard';

function initFilesPage() {
  fetch('/storage/info').then(r => r.json()).then(function(s) {
    storageMountPt  = s.mount_point || '/sdcard';
    fileBrowserPath = storageMountPt;
    loadStorageInfo();
    loadFiles(fileBrowserPath);
  }).catch(function() {
    loadStorageInfo();
    loadFiles(fileBrowserPath);
  });
}

function loadFiles(path) {
  fileBrowserPath = path;
  renderBreadcrumb(path);
  var tbody = document.getElementById('fileList');
  tbody.innerHTML = '<tr><td colspan="3" style="padding:10px 8px;color:#5a7a9a">Loading&hellip;</td></tr>';
  fetch('/files/list?path=' + encodeURIComponent(path))
    .then(r => r.json())
    .then(function(entries) {
      fileEntries = entries.sort(function(a, b) {
        if (a.is_dir !== b.is_dir) return a.is_dir ? -1 : 1;
        return a.name < b.name ? -1 : a.name > b.name ? 1 : 0;
      });
      var rows = '';
      // Parent row when not at mount root
      if (path !== storageMountPt) {
        var parent = path.substring(0, path.lastIndexOf('/')) || storageMountPt;
        rows += '<tr style="border-bottom:1px solid #0d2244">' +
          '<td colspan="3" style="padding:5px 8px">' +
          '<span style="cursor:pointer;color:#5ab4e8" onclick="loadFiles(\'' + escHtml(parent) + '\')">' +
          '&#8679; ..</span></td></tr>';
      }
      if (!fileEntries.length) {
        rows += '<tr><td colspan="3" style="padding:10px 8px;color:#5a7a9a">Empty directory</td></tr>';
      }
      fileEntries.forEach(function(e, i) {
        var sizeStr = e.is_dir ? '<span style="color:#5a7a9a">Folder</span>' :
          (e.size >= 1048576 ? (e.size/1048576).toFixed(1)+' MB' :
           e.size >= 1024    ? (e.size/1024).toFixed(1)+' KB'    :
           e.size + ' B');
        var icon = e.is_dir ? '&#128193;' : '&#128196;';
        var nameCell = e.is_dir
          ? '<span style="cursor:pointer;color:#5ab4e8" onclick="loadFiles(fileEntries['+i+'].path)">' + icon + ' ' + escHtml(e.name) + '</span>'
          : icon + ' ' + escHtml(e.name);
        var actions = '<span style="white-space:nowrap">';
        if (!e.is_dir)
          actions += '<button class="btn" style="padding:2px 7px;font-size:0.75rem;background:#1a3a5c;margin:1px" title="Download" onclick="downloadFile('+i+')">&#8595;</button>';
        actions += '<button class="btn" style="padding:2px 7px;font-size:0.75rem;background:#1a3a5c;margin:1px" title="Rename" onclick="renameEntry('+i+')">&#9998;</button>';
        if (!e.is_dir)
          actions += '<button class="btn" style="padding:2px 7px;font-size:0.75rem;background:#1a3a5c;margin:1px" title="Copy" onclick="copyFilePrompt('+i+')">&#10064;</button>';
        actions += '<button class="btn btn-danger" style="padding:2px 7px;font-size:0.75rem;margin:1px" title="Delete" onclick="deleteEntry('+i+')">&#128465;</button>';
        actions += '</span>';
        rows += '<tr style="border-bottom:1px solid #0d2244">' +
          '<td style="padding:6px 8px">' + nameCell + '</td>' +
          '<td style="padding:6px 8px;text-align:right;color:#aac8e0;font-variant-numeric:tabular-nums">' + sizeStr + '</td>' +
          '<td style="padding:6px 4px;text-align:right">' + actions + '</td></tr>';
      });
      tbody.innerHTML = rows;
    })
    .catch(function() {
      document.getElementById('fileList').innerHTML =
        '<tr><td colspan="3" style="padding:10px 8px;color:#e85a5a">Failed to load directory</td></tr>';
    });
}

function renderBreadcrumb(path) {
  var parts = path.split('/').filter(function(p) { return p.length > 0; });
  var html  = '';
  var built = '';
  parts.forEach(function(part, i) {
    built += '/' + part;
    var b = built;
    if (i < parts.length - 1) {
      html += '<span style="cursor:pointer;color:#5ab4e8" onclick="loadFiles(\'' + escHtml(b) + '\')">' +
              escHtml(part) + '</span><span style="color:#2a4a7f;margin:0 4px">/</span>';
    } else {
      html += '<span style="color:#e0e8f0">' + escHtml(part) + '</span>';
    }
  });
  document.getElementById('fileBreadcrumb').innerHTML =
    '<span style="color:#2a4a7f;margin-right:4px">/</span>' + html;
}

function renameEntry(idx) {
  var e = fileEntries[idx];
  var newName = prompt('Rename "' + e.name + '" to:', e.name);
  if (!newName || newName === e.name) return;
  var dir = e.path.substring(0, e.path.lastIndexOf('/'));
  fetch('/files/rename', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({from: e.path, to: dir + '/' + newName})
  }).then(r => r.json()).then(function(res) {
    if (res.ok) { toast('Renamed'); loadFiles(fileBrowserPath); }
    else toast('Rename failed', false);
  }).catch(function() { toast('Request failed', false); });
}

function deleteEntry(idx) {
  var e = fileEntries[idx];
  if (!confirm('Delete ' + (e.is_dir ? 'folder' : 'file') + ' "' + e.name + '"?')) return;
  fetch('/files/delete', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({path: e.path})
  }).then(r => r.json()).then(function(res) {
    if (res.ok) { toast('Deleted'); loadFiles(fileBrowserPath); loadStorageInfo(); }
    else toast('Delete failed', false);
  }).catch(function() { toast('Request failed', false); });
}

function copyFilePrompt(idx) {
  var e = fileEntries[idx];
  var newName = prompt('Copy "' + e.name + '" as:', 'copy_of_' + e.name);
  if (!newName) return;
  var dir = e.path.substring(0, e.path.lastIndexOf('/'));
  fetch('/files/copy', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({src: e.path, dst: dir + '/' + newName})
  }).then(r => r.json()).then(function(res) {
    if (res.ok) { toast('Copied'); loadFiles(fileBrowserPath); loadStorageInfo(); }
    else toast('Copy failed', false);
  }).catch(function() { toast('Request failed', false); });
}

function downloadFile(idx) {
  window.location.href = '/files/download?path=' + encodeURIComponent(fileEntries[idx].path);
}

function formatSd() {
  if (!confirm('WARNING: This will permanently erase ALL files on the SD card.\n\nContinue?')) return;
  if (!confirm('Second confirmation required.\n\nFormat SD card and lose all data?')) return;
  fetch('/sdcard/format', {method:'POST'}).then(r => r.json()).then(function(res) {
    if (res.ok) {
      toast('SD card formatted');
      fileBrowserPath = storageMountPt;
      loadFiles(storageMountPt);
      loadStorageInfo();
    } else {
      toast('Format failed', false);
    }
  }).catch(function() { toast('Request failed', false); });
}

// ── Tracks tab ────────────────────────────────────────────────────────────────

var trackPollTimer = null;

function startTrackPolling() {
  loadTrackStatus();
  if (!trackPollTimer) trackPollTimer = setInterval(loadTrackStatus, 2000);
}

function stopTrackPolling() {
  if (trackPollTimer) { clearInterval(trackPollTimer); trackPollTimer = null; }
}

function loadTrackStatus() {
  fetch('/tracks/status').then(function(r) { return r.json(); }).then(function(d) {
    renderTrackPage(d);
  }).catch(function() {});
}

function fmtStorage(kb) {
  if (kb >= 1024 * 1024) return (kb / 1024 / 1024).toFixed(1) + ' GB';
  if (kb >= 1024)        return Math.round(kb / 1024) + ' MB';
  return kb + ' KB';
}

function renderTrackPage(d) {
  var sdOk     = d.sdAvailable;
  var sdFreeKB = d.sdFreeKB || 0;
  var sdLow    = sdOk && sdFreeKB < 2048;  // < 2 MB free

  // SD / space warning banner
  var warn = document.getElementById('trackSdWarning');
  if (!sdOk) {
    warn.textContent = 'No SD card detected — insert a card to enable track recording.';
    warn.style.display = 'block';
  } else if (sdLow) {
    warn.textContent = 'SD card low on space (' + fmtStorage(sdFreeKB) + ' free) — exports may fail.';
    warn.style.display = 'block';
    warn.style.background = '#3a2800';
    warn.style.color = '#f0a000';
  } else {
    warn.style.display = 'none';
  }

  // Status line
  var sl = document.getElementById('trackStatusLine');
  if (!sdOk) {
    sl.innerHTML = '&#9675; No SD card';
    sl.style.color = '#c04040';
  } else if (d.loopRunning) {
    var pct = d.maxPoints > 0 ? Math.round(d.count / d.maxPoints * 100) : 0;
    var hs = d.historySec || 0;
    var hist = hs < 60    ? hs + 's'
             : hs < 3600  ? Math.floor(hs/60) + 'm'
             : (Math.floor(hs/3600) + 'h ' + (Math.floor(hs/60)%60) + 'm');
    var freeStr = fmtStorage(sdFreeKB);
    sl.innerHTML = '&#9679; Loop running &nbsp;|&nbsp; ' + d.count + ' pts &nbsp;|&nbsp; ' +
                   hist + ' history &nbsp;|&nbsp; ' + pct + '% full &nbsp;|&nbsp; ' +
                   freeStr + ' free';
    sl.style.color = '#4caf82';
  } else {
    var freeStr2 = fmtStorage(sdFreeKB);
    sl.innerHTML = '&#9675; Loop stopped &nbsp;|&nbsp; ' + freeStr2 + ' free';
    sl.style.color = '#888';
  }

  // Loop start/stop button
  var lb = document.getElementById('trackLoopBtn');
  lb.disabled = !sdOk;
  lb.textContent = d.loopRunning ? 'Disable Loop Recording' : 'Enable Loop Recording';

  // Export segment sliders — show when buffer has data
  var firstTs = d.firstTs || 0;
  var lastTs  = d.lastTs  || 0;
  var hasData = (d.count > 0) && (firstTs > 0);
  var exportSec = document.getElementById('trackExportSection');
  exportSec.style.display = hasData ? '' : 'none';

  if (hasData) {
    trackSliderMin = firstTs;
    trackSliderMax = lastTs;
    var t0el = document.getElementById('trackT0');
    var t1el = document.getElementById('trackT1');
    t0el.min = t1el.min = String(firstTs);
    t0el.max = t1el.max = String(lastTs);
    t0el.step = t1el.step = '60';
    if (!trackSliderInit) {
      // First time data is available — set to full buffer range
      t0el.value = String(firstTs);
      t1el.value = String(lastTs);
      trackSliderInit = true;
    } else {
      // If end slider was at the old max, auto-advance to new latest point
      var prevMax = parseInt(t1el._prevMax || lastTs);
      if (parseInt(t1el.value) >= prevMax) t1el.value = String(lastTs);
    }
    t1el._prevMax = String(lastTs);
    updateTrackExportLabels();
  }

  // File written status (last successful export)
  var fw = document.getElementById('trackFileStatus');
  if (d.fileReady && d.lastFile) {
    document.getElementById('trackFileName').textContent = d.lastFile;
    fw.style.display = 'block';
  } else if (!d.fileReady) {
    fw.style.display = 'none';
  }

  // Populate settings selectors on first load (only if they haven't been
  // touched by the user in this session)
  var iv = document.getElementById('trackIntervalSel');
  var lh = document.getElementById('trackLoopHrsSel');
  if (!iv._loaded) { iv.value = String(d.intervalSec); iv._loaded = true; }
  if (!lh._loaded) { lh.value = String(d.loopHours);   lh._loaded = true; }

  // Settings save is always allowed (persists to NVS; loop rebuilt when SD returns)
  // but warn via the existing banner if SD is absent
}

function toggleTrackLoop() {
  var running = document.getElementById('trackLoopBtn').textContent.indexOf('Stop') >= 0;
  fetch(running ? '/tracks/loop/stop' : '/tracks/loop/start', {method:'POST'})
    .then(function() { loadTrackStatus(); })
    .catch(function() { toast('Request failed', false); });
}

var trackSliderMin = 0, trackSliderMax = 0, trackSliderInit = false;

function fmtUtcTs(ts) {
  if (!ts) return '--';
  return new Date(ts * 1000).toISOString().replace('T',' ').slice(0,19) + 'Z';
}

function updateTrackExportLabels() {
  var t0 = parseInt(document.getElementById('trackT0').value) || 0;
  var t1 = parseInt(document.getElementById('trackT1').value) || 0;
  document.getElementById('trackT0Label').textContent = fmtUtcTs(t0);
  document.getElementById('trackT1Label').textContent = fmtUtcTs(t1);
  var dur = t1 > t0 ? t1 - t0 : 0;
  var ds = dur < 60   ? dur + 's'
         : dur < 3600 ? Math.floor(dur/60) + 'm ' + (dur%60) + 's'
         : Math.floor(dur/3600) + 'h ' + (Math.floor(dur/60)%60) + 'm';
  document.getElementById('trackExportDur').textContent = 'Duration: ' + (dur > 0 ? ds : '--');
}

function onTrackT0() {
  var t0 = parseInt(document.getElementById('trackT0').value);
  var t1el = document.getElementById('trackT1');
  if (t0 >= parseInt(t1el.value)) t1el.value = String(Math.min(t0 + 60, trackSliderMax));
  updateTrackExportLabels();
}

function onTrackT1() {
  var t1 = parseInt(document.getElementById('trackT1').value);
  var t0el = document.getElementById('trackT0');
  if (t1 <= parseInt(t0el.value)) t0el.value = String(Math.max(t1 - 60, trackSliderMin));
  updateTrackExportLabels();
}

function exportTrackSegment() {
  var t0 = parseInt(document.getElementById('trackT0').value);
  var t1 = parseInt(document.getElementById('trackT1').value);
  if (t0 >= t1) { toast('Start must be before end', false); return; }
  var btn = document.getElementById('trackExportBtn');
  btn.disabled = true; btn.textContent = 'Exporting…';
  fetch('/tracks/segment/export', {
    method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({t0:t0, t1:t1})
  }).then(function(r){return r.json();}).then(function(d){
    btn.disabled = false; btn.textContent = 'Export GPX';
    if (d.ok) {
      toast('Track saved: ' + d.file);
      document.getElementById('trackFileName').textContent = d.file;
      document.getElementById('trackFileStatus').style.display = 'block';
      loadTrackStatus();
    } else {
      toast(d.error || 'Export failed', false);
    }
  }).catch(function(){
    btn.disabled = false; btn.textContent = 'Export GPX';
    toast('Request failed', false);
  });
}

function toggleTrackSettings() {
  var body = document.getElementById('trackSettingsBody');
  var chev = document.getElementById('trackSettingsChevron');
  var open = body.style.display !== 'none';
  body.style.display = open ? 'none' : '';
  chev.style.transform = open ? 'rotate(-90deg)' : '';
}

function saveTrackSettings() {
  var iv = parseInt(document.getElementById('trackIntervalSel').value);
  var lh = parseInt(document.getElementById('trackLoopHrsSel').value);
  fetch('/tracks/config', {
    method: 'POST',
    headers: {'Content-Type':'application/json'},
    body: JSON.stringify({intervalSec: iv, loopHours: lh})
  }).then(function(r) { return r.json(); }).then(function(d) {
    if (d.ok) { toast('Track settings saved'); loadTrackStatus(); }
    else toast('Save failed', false);
  }).catch(function() { toast('Request failed', false); });
}

// Race is the phone-first home screen.
loadRaceState();
loadRaceMarksAndCourses();
loadCrewControlState();
startRacePolling();
initRaceCompass();
</script>
</body>
</html>)rawhtml";
    return ui;
}
