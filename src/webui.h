#pragma once
#include <Arduino.h>

// Returns the web UI HTML as a String.
inline String getWebUI() {
    return R"rawhtml(<!DOCTYPE html>
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
</style>
</head>
<body>
<header>
  <h1>&#9741; Sailing Computer</h1>
  <span id="deviceIP"></span>
</header>
<nav>
  <button class="active" onclick="showPage('status',this)">Status</button>
  <button onclick="showPage('config',this)">Configuration</button>
  <button onclick="showPage('system',this)">System</button>
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
    <h2>Connections</h2>
    <div class="stat-grid">
      <div class="stat"><div class="stat-label">WiFi Mode</div><div class="stat-value" id="s-wifimode">--</div></div>
      <div class="stat"><div class="stat-label">IP Address</div><div class="stat-value" id="s-ip" style="font-size:0.9rem">--</div></div>
      <div class="stat"><div class="stat-label">NTRIP</div><div class="stat-value" id="s-ntrip">--</div></div>
      <div class="stat"><div class="stat-label">NTRIP Source</div><div class="stat-value" id="s-ntrip-src" style="font-size:0.85rem">--</div></div>
      <div class="stat"><div class="stat-label">RTCM Bytes</div><div class="stat-value" id="s-rtcm">0</div></div>
    </div>
  </div>
</div>

<!-- CONFIG PAGE -->
<div id="config" class="page">
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
    </div>
    <div class="card">
      <h2>Antenna</h2>
      <label>Heading Offset (&deg;)</label>
      <input type="number" name="headingOffset" id="headingOffset" value="90" min="-360" max="360" step="0.1">
      <p style="font-size:0.8rem;color:#8899aa;margin:0.25rem 0 0">Degrees added to UM982 heading. Default 90&deg; for port/starboard aft-rail mounting (ANT1=stbd, ANT2=port). Set 0 for fore/aft mounting.</p>
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
    <h2>Firmware Update (OTA)</h2>
    <p style="font-size:0.85rem;color:#7a9ab8;margin-bottom:1rem;">
      Build with PlatformIO, then upload
      <code style="background:#0a1628;padding:2px 6px;border-radius:4px">.pio/build/esp32dev/firmware.bin</code>.
      The device will restart automatically after a successful flash.
    </p>
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
</div>

<div class="toast" id="toast"></div>

<script>
function showPage(id, btn) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('nav button').forEach(b => b.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  btn.classList.add('active');
  if (id === 'config') loadConfig();
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
    var fixColors = {0:'err',1:'warn',2:'warn',4:'rtk',5:'warn'};
    var el = document.getElementById('s-fix');
    el.textContent = d.fixLabel || '--';
    el.className = 'stat-value ' + (fixColors[d.fix] || 'err');
    setText('s-sats', d.sats != null ? d.sats : '--');
    setText('s-lat',  d.lat  != null ? Number(d.lat).toFixed(6)  : '--');
    setText('s-lon',  d.lon  != null ? Number(d.lon).toFixed(6)  : '--');
    setText('s-hdt',   d.hdtValid   ? Number(d.heading).toFixed(1) + '\u00b0' : '--\u00b0');
    setText('s-roll',  d.rollValid  ? Number(d.roll).toFixed(1)    + '\u00b0' : '--\u00b0');
    var hdtEl = document.getElementById('s-hdtsrc');
    hdtEl.textContent = d.hdtValid ? 'Dual Antenna' : 'No ANT2 Lock';
    hdtEl.className = 'stat-value ' + (d.hdtValid ? 'ok' : 'warn');
    setText('s-sog',  d.sog  != null ? Number(d.sog).toFixed(1)  : '--');
    setText('s-cog',  d.cog  != null ? Number(d.cog).toFixed(1) + '\u00b0' : '--\u00b0');
    setText('s-hdop', d.hdop != null ? Number(d.hdop).toFixed(2) : '--');
    setText('s-alt',  d.altitude != null ? Number(d.altitude).toFixed(1) + ' m' : '--');
    setText('s-wifimode', d.wifiMode || '--');
    var ip = d.wifiMode === 'AP' ? d.apIP : d.ip;
    setText('s-ip', ip || '--');
    setText('s-rtcm', formatBytes(d.ntripBytesIn || 0));
    setText('sysIP', ip || '--');
    setText('deviceIP', ip || '--');
    var ntrip = document.getElementById('s-ntrip');
    ntrip.textContent = d.ntripConnected ? 'Connected' : 'Off';
    ntrip.className = 'stat-value ' + (d.ntripConnected ? 'ok' : 'err');
    setText('s-ntrip-src', d.ntripConnected ? 'Source ' + ((d.ntripActiveIdx||0)+1) : '--');
  }).catch(function(e) { console.error('Status fetch error:', e); });
}
setInterval(updateStatus, 2000);
updateStatus();

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
            '<input type="text" name="n'+i+'host" id="n'+i+'host" value="' + (src.host||'') + '" placeholder="rtk2go.com" maxlength="127"></div>' +
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
  fetch('/config').then(r=>r.json()).then(d => {
    document.getElementById('apMode').checked = d.apMode;
    document.getElementById('wifiSSID').value  = d.wifiSSID || '';
    document.getElementById('headingOffset').value = d.headingOffset != null ? d.headingOffset : 90;
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
  const fd = new FormData(this);
  const data = {};
  for (const [k, v] of fd.entries()) data[k] = v;
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
  var fd = new FormData();
  fd.append('firmware', file);
  xhr.send(fd);
}
</script>
</body>
</html>)rawhtml";
}
