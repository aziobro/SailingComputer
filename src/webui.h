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
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: system-ui, sans-serif; background: #0a1628; color: #e0e8f0; min-height: 100vh; }
  header { background: #0d2244; padding: 16px 24px; border-bottom: 2px solid #1e4080; }
  header h1 { font-size: 1.4rem; color: #5ab4e8; letter-spacing: 1px; }
  header span { font-size: 0.8rem; color: #7a9ab8; margin-left: 8px; }
  nav { display: flex; background: #0d2244; border-bottom: 1px solid #1e4080; }
  nav button { flex: 1; padding: 10px; background: none; border: none; color: #7a9ab8;
               cursor: pointer; font-size: 0.9rem; transition: all 0.2s; }
  nav button.active, nav button:hover { color: #5ab4e8; border-bottom: 2px solid #5ab4e8; }
  nav button.logout { flex: 0; padding: 10px 14px; color: #c05050; font-size: 0.8rem; }
  nav button.logout:hover { color: #e06060; border-bottom: 2px solid #e06060; }
  .page { display: none; padding: 20px; max-width: 700px; margin: 0 auto; }
  .page.active { display: block; }

  .card { background: #0d2244; border: 1px solid #1e4080; border-radius: 8px; padding: 16px; margin-bottom: 16px; }
  .card h2 { font-size: 0.85rem; text-transform: uppercase; letter-spacing: 1px; color: #5ab4e8; margin-bottom: 12px; }

  .stat-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(140px, 1fr)); gap: 12px; }
  .stat { background: #091a36; border-radius: 6px; padding: 12px; text-align: center; }
  .stat-label { font-size: 0.7rem; color: #5a7a9a; text-transform: uppercase; letter-spacing: 0.5px; }
  .stat-value { font-size: 1.4rem; font-weight: bold; color: #e0e8f0; margin-top: 4px; font-variant-numeric: tabular-nums; }
  .stat-value.ok  { color: #4caf82; }
  .stat-value.rtk { color: #5ab4e8; }
  .stat-value.warn { color: #e8a830; }
  .stat-value.err { color: #e85a5a; }

  label { display: block; margin-bottom: 4px; font-size: 0.8rem; color: #7a9ab8; }
  input, select { width: 100%; padding: 8px 10px; background: #091a36; border: 1px solid #1e4080;
                  border-radius: 5px; color: #e0e8f0; font-size: 0.9rem; margin-bottom: 12px; }
  input:focus { outline: none; border-color: #5ab4e8; }
  .row { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }

  button.btn { padding: 10px 20px; border: none; border-radius: 6px; cursor: pointer;
               font-size: 0.9rem; font-weight: bold; transition: opacity 0.2s; }
  button.btn:hover { opacity: 0.85; }
  button.btn-primary { background: #1e6eb8; color: #fff; }
  button.btn-danger  { background: #8b2020; color: #fff; }

  .toast { display: none; position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%);
           padding: 10px 20px; border-radius: 6px; font-size: 0.9rem; z-index: 100; }
  .section-title { font-size: 0.8rem; font-weight: bold; text-transform: uppercase;
                   letter-spacing: 0.5px; color: #5ab4e8; margin: 16px 0 8px;
                   border-bottom: 1px solid #1e4080; padding-bottom: 4px; }
  .toggle-row { display: flex; align-items: center; margin-bottom: 12px; }
  .toggle-row label { margin: 0 0 0 8px; font-size: 0.9rem; color: #e0e8f0; }
  input[type=checkbox] { width: auto; margin: 0; accent-color: #5ab4e8; transform: scale(1.3); cursor: pointer; }

  /* Instrument panel */
  .instruments { display: flex; gap: 12px; flex-wrap: wrap; justify-content: center; align-items: flex-start; }
  .instrument  { display: flex; flex-direction: column; align-items: center; gap: 6px; }
  .instrument-label { font-size: 0.7rem; color: #5a7a9a; text-transform: uppercase; letter-spacing: 0.5px; }
  .instr-legend { font-size: 0.72rem; color: #8899aa; margin-top: 2px; }
</style>
</head>
<body>
<header>
  <h1>&#9741; Sailing Computer</h1>
  <span id="deviceIP"></span><span id="fwVersion" style="font-size:0.75rem;color:#3a6a8a;margin-left:10px"></span>
</header>
<nav>
  <button class="active" onclick="showPage('status',this)">Status</button>
  <button onclick="showPage('config',this)">Configuration</button>
  <button onclick="showPage('racing',this)">Racing</button>
  <button onclick="showPage('files',this)">Files</button>
  <button onclick="showPage('system',this)">System</button>
  <button class="logout" onclick="doLogout()">&#x23FB; Log Out</button>
</nav>

<!-- STATUS PAGE -->
<div id="status" class="page active">
  <div class="card">
    <h2>GNSS Fix</h2>
    <div class="stat-grid">
      <div class="stat"><div class="stat-label">Fix Type</div><div class="stat-value" id="s-fix">--</div></div>
      <div class="stat"><div class="stat-label">Satellites</div><div class="stat-value" id="s-sats">--</div></div>
      <div class="stat"><div class="stat-label">Latitude</div><div class="stat-value" id="s-lat">--</div></div>
      <div class="stat"><div class="stat-label">Longitude</div><div class="stat-value" id="s-lon">--</div></div>
      <div class="stat"><div class="stat-label">True Heading</div><div class="stat-value" id="s-hdt">--&deg;</div></div>
      <div class="stat"><div class="stat-label">Roll</div><div class="stat-value" id="s-roll">--&deg;</div></div>
      <div class="stat"><div class="stat-label">Heading Src</div><div class="stat-value" id="s-hdtsrc" style="font-size:0.9rem">--</div></div>
      <div class="stat"><div class="stat-label">SOG (kts)</div><div class="stat-value" id="s-sog">--</div></div>
      <div class="stat"><div class="stat-label">COG</div><div class="stat-value" id="s-cog">--&deg;</div></div>
      <div class="stat"><div class="stat-label">HDOP</div><div class="stat-value" id="s-hdop">--</div></div>
      <div class="stat"><div class="stat-label">Altitude (m)</div><div class="stat-value" id="s-alt">--</div></div>
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
        <div class="stat-label">Tack</div>
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
        <div class="stat-label">Drive</div>
        <div class="stat-value" id="s-drive">--</div>
        <div style="font-size:0.7rem;color:#8899aa;margin-top:2px">fwd speed (kts)</div>
      </div>
    </div>
    <div id="s-favor-row" style="display:none;margin-top:0.6rem;padding:0.5rem 0.7rem;border-radius:6px;background:#0d1f33;font-size:0.8rem">
      <span style="color:#8899aa">Current favor: </span>
      <span id="s-favor-val" style="font-weight:bold"></span>
      <span id="s-favor-desc" style="color:#8899aa;margin-left:0.4rem"></span>
    </div>
    <p style="font-size:0.75rem;color:#556677;margin:0.5rem 0 0">
      Leeway = COG &minus; Heading &mdash; includes keel slip &amp; tidal current.
      Tack from heel angle (&gt;3&deg; threshold). Current favor = lateral drift toward windward.
    </p>
  </div>
  <div class="card">
    <h2>Connections</h2>
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
      <p style="font-size:0.8rem;color:#8899aa;margin:0.25rem 0 0">COG is frozen below this speed to suppress GPS position noise. Default 0.1 kts. Increase if COG is still unstable at rest.</p>
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
    <h2>Firmware Update (OTA)</h2>
    <p style="font-size:0.85rem;color:#7a9ab8;margin-bottom:1rem;">
      Build with PlatformIO, then upload
      <code style="background:#0a1628;padding:2px 6px;border-radius:4px">.pio/build/esp32dev/firmware.bin</code>.
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
<div id="racing" class="page">
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
        <button class="btn btn-primary" style="margin-top:22px;width:100%" onclick="useGpsForMark()">&#8982; Use GPS Position</button>
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
    <h2>Courses</h2>
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
    if (r.ok) { hideAuthWall(page); if (page === 'config') loadConfig(); }
    else showAuthWall(page);
  }).catch(function() { showAuthWall(page); });
}

function showPage(id, btn) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('nav button').forEach(b => b.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  btn.classList.add('active');
  if (id === 'config') loadConfig();
  if (id === 'racing') { loadMarks(); loadCourses(); }
  if (id === 'files')  { initFilesPage(); }
  if (id === 'system') {
    fetch('/config').then(function(r) {
      if (r.ok) hideAuthWall('system');
      else showAuthWall('system');
    }).catch(function() { showAuthWall('system'); });
  }
}

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
          favorDesc.textContent = 'current helping — stay on ' + tack + ' tack';
        } else if (favor < -0.1) {
          favorVal.style.color  = '#f87171';
          favorDesc.textContent = 'current hurting — consider tacking';
        } else {
          favorVal.style.color  = '#8899aa';
          favorDesc.textContent = 'current neutral';
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
setInterval(updateStatus, 2000);
updateStatus();

// ── Instrument initialisation ─────────────────────────────────────────────────

function initCompass() {
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
initCompass();
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

// ── Racing: marks ─────────────────────────────────────────────────────────────
function loadMarks() {
  fetch('/marks').then(r => r.json()).then(function(marks) {
    var el = document.getElementById('markList');
    if (!marks.length) {
      el.innerHTML = '<p style="color:#5a7a9a;font-size:0.85rem">No marks saved.</p>';
      return;
    }
    var rows = marks.map(function(m) {
      return '<tr>' +
        '<td style="padding:6px 8px;color:#e0e8f0">' + escHtml(m.name) + '</td>' +
        '<td style="padding:6px 8px;color:#aac8e0;font-variant-numeric:tabular-nums">' + m.lat.toFixed(7) + '</td>' +
        '<td style="padding:6px 8px;color:#aac8e0;font-variant-numeric:tabular-nums">' + m.lon.toFixed(7) + '</td>' +
        '<td style="padding:6px 8px"><span style="font-size:0.72rem;color:#5a7a9a">' + escHtml(m.id) + '</span></td>' +
        '<td style="padding:6px 4px"><button class="btn btn-danger" style="padding:3px 10px;font-size:0.8rem" onclick="deleteMark(\'' + escHtml(m.id) + '\')">&#128465;</button></td>' +
        '</tr>';
    }).join('');
    el.innerHTML = '<table style="width:100%;border-collapse:collapse">' +
      '<thead><tr style="font-size:0.72rem;color:#5a7a9a;text-transform:uppercase">' +
      '<th style="text-align:left;padding:4px 8px">Name</th>' +
      '<th style="text-align:left;padding:4px 8px">Lat</th>' +
      '<th style="text-align:left;padding:4px 8px">Lon</th>' +
      '<th style="text-align:left;padding:4px 8px">ID</th>' +
      '<th></th></tr></thead><tbody>' + rows + '</tbody></table>';
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
    } else { toast('Save failed', false); }
  }).catch(function() { toast('Request failed', false); });
}

function deleteMark(id) {
  if (!confirm('Delete mark ' + id + '?')) return;
  fetch('/marks/delete', { method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({id:id})
  }).then(r => r.json()).then(function(res) {
    if (res.ok) { toast('Mark deleted'); loadMarks(); }
    else toast('Delete failed', false);
  }).catch(function() { toast('Request failed', false); });
}

function useGpsForMark() {
  fetch('/status').then(r => r.json()).then(function(s) {
    if (!s.lat && !s.lon) { toast('No GPS fix', false); return; }
    document.getElementById('mkLat').value = s.lat.toFixed(7);
    document.getElementById('mkLon').value = s.lon.toFixed(7);
    toast('GPS position loaded');
  }).catch(function() { toast('Failed to get GPS', false); });
}

function loadCourses() {
  fetch('/courses').then(r => r.json()).then(function(courses) {
    var el = document.getElementById('courseList');
    if (!courses.length) {
      el.innerHTML = '<p style="color:#5a7a9a;font-size:0.85rem">No courses saved. Import a GPX file to add courses.</p>';
      return;
    }
    // Also fetch marks so we can show names instead of IDs
    fetch('/marks').then(r => r.json()).then(function(marks) {
      var markMap = {};
      marks.forEach(function(m) { markMap[m.id] = m.name; });
      var html = courses.map(function(c) {
        var markNames = c.marks.map(function(ref) {
          var n = markMap[ref.mark_id] || ref.mark_id;
          var rnd = ref.port ? '<span style="font-size:0.68rem;color:#4caf82">P</span>' : '<span style="font-size:0.68rem;color:#e8a830">S</span>';
          return '<span style="display:inline-block;background:#0d2244;border:1px solid #1e4080;border-radius:4px;padding:1px 6px;margin:2px;font-size:0.78rem">' + escHtml(n) + ' ' + rnd + '</span>';
        }).join('<span style="color:#5a7a9a;margin:0 2px">&#8594;</span>');
        return '<div style="border-bottom:1px solid #1e4080;padding:10px 0">' +
          '<div style="font-weight:bold;color:#e0e8f0;margin-bottom:6px">' + escHtml(c.name) +
          ' <span style="font-size:0.72rem;color:#5a7a9a">(' + c.marks.length + ' marks)</span></div>' +
          '<div style="line-height:2">' + (markNames || '<em style="color:#5a7a9a">No marks</em>') + '</div>' +
          '</div>';
      }).join('');
      el.innerHTML = html;
    });
  }).catch(function() {
    document.getElementById('courseList').innerHTML = '<p style="color:#e85a5a;font-size:0.85rem">Failed to load courses.</p>';
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
</script>
</body>
</html>)rawhtml";
    return ui;
}
