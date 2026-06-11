#pragma once

// High-visibility, read-only race display for helm and trim crew.
inline const char* getCrewUI() {
    static const char ui[] = R"crewhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="theme-color" content="#02060b">
<title>Crew Display</title>
<style>
  :root {
    color-scheme: dark;
    --bg: #02060b;
    --panel: #081522;
    --line: #24445f;
    --text: #f7fbff;
    --muted: #9bb0c2;
    --cyan: #53d7ff;
    --green: #50f0a8;
    --amber: #ffd057;
    --red: #ff675f;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  html, body {
    width: 100%;
    min-height: 100%;
    background: var(--bg);
    color: var(--text);
    overscroll-behavior: none;
    -webkit-text-size-adjust: 100%;
  }
  body {
    min-height: 100vh;
    min-height: 100dvh;
    overflow: hidden;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif;
    font-weight: 800;
  }
  button { font: inherit; touch-action: manipulation; -webkit-tap-highlight-color: transparent; }
  #app {
    display: grid;
    grid-template-rows: auto minmax(0, 1fr) auto;
    min-height: 100vh;
    min-height: 100dvh;
    padding: env(safe-area-inset-top) env(safe-area-inset-right)
             env(safe-area-inset-bottom) env(safe-area-inset-left);
  }
  .topbar {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 1rem;
    min-height: 54px;
    padding: 8px clamp(14px, 2vw, 28px);
    border-bottom: 2px solid var(--line);
    background: #050d15;
  }
  .phase {
    color: var(--cyan);
    font-size: clamp(0.92rem, 1.6vw, 1.35rem);
    letter-spacing: 0.16em;
    text-transform: uppercase;
  }
  .top-actions { display: flex; align-items: center; gap: 10px; }
  .connection {
    display: inline-flex;
    align-items: center;
    gap: 8px;
    color: var(--green);
    font-size: clamp(0.72rem, 1.1vw, 0.9rem);
    letter-spacing: 0.08em;
    text-transform: uppercase;
  }
  .connection::before {
    width: 10px;
    height: 10px;
    border-radius: 50%;
    background: currentColor;
    content: "";
  }
  .connection.offline { color: var(--red); }
  .view-button {
    min-height: 42px;
    padding: 7px 12px;
    border: 2px solid #476783;
    border-radius: 8px;
    background: #102538;
    color: var(--text);
    font-size: clamp(0.7rem, 1vw, 0.86rem);
    letter-spacing: 0.05em;
    text-transform: uppercase;
  }
  .view-button[hidden] { display: none; }
  main {
    display: grid;
    grid-template-columns: minmax(0, 1.16fr) minmax(0, 1fr);
    gap: clamp(10px, 1.4vw, 18px);
    min-height: 0;
    padding: clamp(10px, 1.4vw, 18px);
  }
  .hero {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-width: 0;
    min-height: 0;
    padding: clamp(12px, 2vw, 28px);
    border: 3px solid #315a75;
    border-radius: 18px;
    background: radial-gradient(circle at 50% 35%, #102a3c 0, #07121d 70%);
    text-align: center;
  }
  .hero-label {
    color: var(--muted);
    font-size: clamp(0.9rem, 1.7vw, 1.45rem);
    letter-spacing: 0.16em;
    text-transform: uppercase;
  }
  .hero-value {
    max-width: 100%;
    margin-top: clamp(4px, 1vh, 12px);
    color: var(--text);
    font-size: clamp(4.6rem, 12vw, 10.5rem);
    font-variant-numeric: tabular-nums;
    letter-spacing: -0.055em;
    line-height: 0.94;
    overflow-wrap: anywhere;
  }
  .hero-value.word {
    font-size: clamp(3.2rem, 8vw, 7rem);
    letter-spacing: -0.025em;
    line-height: 1;
  }
  .hero-sub {
    min-height: 1.4em;
    margin-top: clamp(8px, 1.4vh, 16px);
    color: var(--cyan);
    font-size: clamp(1rem, 2.2vw, 2rem);
    letter-spacing: 0.04em;
  }
  .metrics {
    display: grid;
    grid-template-columns: 1fr 1fr;
    grid-template-rows: 1fr 1fr;
    gap: clamp(10px, 1.4vw, 18px);
    min-width: 0;
    min-height: 0;
  }
  .metric {
    display: flex;
    flex-direction: column;
    justify-content: center;
    min-width: 0;
    min-height: 0;
    padding: clamp(10px, 1.8vw, 22px);
    border: 2px solid var(--line);
    border-radius: 16px;
    background: var(--panel);
    text-align: center;
  }
  .metric-label {
    color: var(--muted);
    font-size: clamp(0.72rem, 1.25vw, 1.08rem);
    letter-spacing: 0.12em;
    text-transform: uppercase;
  }
  .metric-value {
    margin-top: 6px;
    color: var(--text);
    font-size: clamp(2rem, 5.2vw, 4.7rem);
    font-variant-numeric: tabular-nums;
    letter-spacing: -0.035em;
    line-height: 1;
    overflow-wrap: anywhere;
  }
  .metric-value.word { font-size: clamp(1.45rem, 3.6vw, 3.2rem); }
  .metric.accent .metric-value { color: var(--green); }
  .metric.warn .metric-value { color: var(--amber); }
  .metric.alert .metric-value { color: var(--red); }
  .footer {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    align-items: center;
    gap: 16px;
    min-height: 58px;
    padding: 8px clamp(14px, 2vw, 28px);
    border-top: 2px solid var(--line);
    background: #050d15;
  }
  .message {
    min-width: 0;
    color: var(--amber);
    font-size: clamp(1rem, 2vw, 1.65rem);
    letter-spacing: 0.04em;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  .footer-meta {
    color: #7f98aa;
    font-size: clamp(0.72rem, 1.1vw, 0.9rem);
    letter-spacing: 0.08em;
    text-align: right;
    text-transform: uppercase;
    white-space: nowrap;
  }
  body.start-mode .hero { border-color: #7f6426; }
  body.start-mode .hero-value { color: var(--amber); }
  body.start-mode.final-minute .hero {
    border-color: #9b3934;
    background: radial-gradient(circle at 50% 35%, #391512 0, #120807 72%);
  }
  body.start-mode.final-minute .hero-value { color: var(--red); }
  body.rounding-mode .hero {
    border-color: #b47d24;
    background: radial-gradient(circle at 50% 35%, #38270c 0, #130e05 72%);
  }
  body.finish-mode .hero { border-color: #30845e; }
  body.finish-mode .hero-value { color: var(--green); }
  .offline-cover {
    position: fixed;
    inset: 0;
    z-index: 20;
    display: none;
    place-items: center;
    padding: 30px;
    background: rgba(2, 6, 11, 0.88);
    color: var(--red);
    font-size: clamp(2rem, 6vw, 5rem);
    letter-spacing: 0.08em;
    text-align: center;
    text-transform: uppercase;
  }
  body.long-offline .offline-cover { display: grid; }

  @media (orientation: portrait) {
    body { overflow: auto; }
    #app { min-height: 100dvh; }
    main {
      grid-template-columns: 1fr;
      grid-template-rows: minmax(250px, 0.85fr) minmax(360px, 1fr);
      overflow: visible;
    }
    .hero-value { font-size: clamp(4.8rem, 22vw, 9rem); }
    .hero-value.word { font-size: clamp(3rem, 14vw, 6rem); }
    .metric-value { font-size: clamp(2.3rem, 10vw, 4.5rem); }
    .metric-value.word { font-size: clamp(1.5rem, 7vw, 3rem); }
    .footer { position: sticky; bottom: 0; }
  }

  @media (max-height: 600px) and (orientation: landscape) {
    .topbar { min-height: 46px; padding-top: 5px; padding-bottom: 5px; }
    .footer { min-height: 48px; padding-top: 5px; padding-bottom: 5px; }
    .hero-value { font-size: clamp(4.2rem, 11vw, 8rem); }
  }
</style>
</head>
<body>
<div id="app">
  <header class="topbar">
    <div id="phase" class="phase">Crew Display</div>
    <div class="top-actions">
      <button id="viewButton" class="view-button" type="button" hidden>View: Follow</button>
      <div id="connection" class="connection offline">Connecting</div>
    </div>
  </header>

  <main>
    <section class="hero" aria-live="polite">
      <div id="heroLabel" class="hero-label">Waiting for tactician</div>
      <div id="heroValue" class="hero-value">--</div>
      <div id="heroSub" class="hero-sub"></div>
    </section>

    <section class="metrics" aria-label="Priority race data">
      <div id="metric0" class="metric"><div class="metric-label">--</div><div class="metric-value">--</div></div>
      <div id="metric1" class="metric"><div class="metric-label">--</div><div class="metric-value">--</div></div>
      <div id="metric2" class="metric"><div class="metric-label">--</div><div class="metric-value">--</div></div>
      <div id="metric3" class="metric"><div class="metric-label">--</div><div class="metric-value">--</div></div>
    </section>
  </main>

  <footer class="footer">
    <div id="message" class="message">Waiting for display state...</div>
    <div id="footerMeta" class="footer-meta">Sailing Computer</div>
  </footer>
</div>
<div class="offline-cover">Connection Lost</div>

<script>
var state = null;
var receivedAt = 0;
var lastFetchAt = 0;
var lastRevision = null;
var localMode = 'follow';
var localModes = ['follow', 'start', 'navigation', 'performance', 'rounding'];
var wakeLock = null;
var fetchInFlight = false;

var phaseLabels = {
  prestart: 'Pre-Start',
  starting: 'Starting Sequence',
  upwind: 'Upwind Leg',
  downwind: 'Downwind Leg',
  reaching: 'Reaching Leg',
  rounding: 'Mark Rounding',
  finish: 'Finish',
  custom: 'Custom'
};

var modeDefaults = {
  start: ['distance_line', 'time_line', 'speed', 'heading'],
  navigation: ['bearing_mark', 'distance_mark', 'vmg_mark', 'speed'],
  performance: ['speed', 'vmg_mark', 'heading', 'leeway'],
  rounding: ['distance_mark', 'bearing_mark', 'next_mark', 'maneuver'],
  custom: ['speed', 'heading', 'status', 'target_heading'],
  finish: ['speed', 'heading', 'course', 'status']
};

function finiteNumber(value) {
  var number = Number(value);
  return isFinite(number) ? number : null;
}

function nowServerMs() {
  if (!state) return 0;
  return Number(state.server_now_ms) + (performance.now() - receivedAt);
}

function formatClock(seconds, showHours) {
  seconds = Math.max(0, Math.floor(seconds));
  var h = Math.floor(seconds / 3600);
  var m = Math.floor((seconds % 3600) / 60);
  var s = seconds % 60;
  if (showHours || h > 0)
    return h + ':' + (m < 10 ? '0' : '') + m + ':' + (s < 10 ? '0' : '') + s;
  return m + ':' + (s < 10 ? '0' : '') + s;
}

function haversineNm(lat1, lon1, lat2, lon2) {
  var radius = 3440.065;
  var dLat = (lat2 - lat1) * Math.PI / 180;
  var dLon = (lon2 - lon1) * Math.PI / 180;
  var a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
          Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
          Math.sin(dLon / 2) * Math.sin(dLon / 2);
  return radius * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
}

function bearingDeg(lat1, lon1, lat2, lon2) {
  var y = Math.sin((lon2 - lon1) * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180);
  var x = Math.cos(lat1 * Math.PI / 180) * Math.sin(lat2 * Math.PI / 180) -
          Math.sin(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
          Math.cos((lon2 - lon1) * Math.PI / 180);
  return (Math.atan2(y, x) * 180 / Math.PI + 360) % 360;
}

function distToSegmentNm(plat, plon, alat, alon, blat, blon) {
  var clat = (alat + blat) / 2 * Math.PI / 180;
  var cos = Math.cos(clat);
  var ax = alon * cos, ay = alat, bx = blon * cos, by = blat;
  var px = plon * cos, py = plat;
  var dx = bx - ax, dy = by - ay;
  var lengthSquared = dx * dx + dy * dy;
  if (!lengthSquared) return haversineNm(plat, plon, alat, alon);
  var t = Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / lengthSquared));
  return haversineNm(plat, plon, ay + t * dy, (ax + t * dx) / cos);
}

function calculations() {
  var t = state.telemetry || {};
  var race = state.race || {};
  var lat = finiteNumber(t.lat), lon = finiteNumber(t.lon);
  var validPosition = Number(t.fix) > 0 && lat !== null && lon !== null;
  var speed = finiteNumber(t.sog);
  var course = t.cogValid ? finiteNumber(t.cog) : null;
  var heading = t.hdtValid ? finiteNumber(t.heading) : null;
  var active = state.activeMark;
  var markDistance = null, markBearing = null, markVmg = null;

  if (validPosition && active) {
    markDistance = haversineNm(lat, lon, Number(active.lat), Number(active.lon));
    markBearing = bearingDeg(lat, lon, Number(active.lat), Number(active.lon));
    if (course !== null && speed !== null) {
      var error = ((course - markBearing + 540) % 360) - 180;
      markVmg = speed * Math.cos(error * Math.PI / 180);
    }
  }

  var lineDistance = null;
  var line = state.line || [];
  if (validPosition && line[0] && line[1]) {
    if (state.startTarget === 'pin' && line[0].set)
      lineDistance = haversineNm(lat, lon, Number(line[0].lat), Number(line[0].lon));
    else if (state.startTarget === 'boat' && line[1].set)
      lineDistance = haversineNm(lat, lon, Number(line[1].lat), Number(line[1].lon));
    else if (line[0].set && line[1].set)
      lineDistance = distToSegmentNm(lat, lon, Number(line[0].lat), Number(line[0].lon),
                                     Number(line[1].lat), Number(line[1].lon));
  }

  var remainingMs = Number(race.t0_ms || 0) - nowServerMs();
  return {
    speed: speed,
    course: course,
    heading: heading,
    heel: t.rollValid ? finiteNumber(t.roll) : null,
    leeway: t.sailingValid ? finiteNumber(t.leeway) : null,
    drive: t.sailingValid ? finiteNumber(t.driveSpeed) : null,
    markDistance: markDistance,
    markBearing: markBearing,
    markVmg: markVmg,
    markTime: markDistance !== null && markVmg !== null && markVmg > 0.1 ?
      markDistance / markVmg * 3600 : null,
    lineDistance: lineDistance,
    lineTime: lineDistance !== null && speed !== null && speed > 0.1 ?
      lineDistance / speed * 3600 : null,
    remainingMs: remainingMs
  };
}

function distanceText(nm, startLine) {
  if (nm === null) return '--';
  if (startLine || nm < 0.1) return Math.round(nm * 6076.12) + ' ft';
  return nm.toFixed(nm < 1 ? 2 : 1) + ' nm';
}

function fieldData(key, calc) {
  var activeName = state.activeMark ? state.activeMark.name : '--';
  var nextName = state.nextMark ? state.nextMark.name : '--';
  var target = Number(state.targetHeading);
  var countdown = calc.remainingMs;
  var raceState = state.race ? state.race.state : 'idle';
  var countdownValue = raceState === 'idle' || !Number(state.race.t0_ms) ? '--' :
    countdown > 0 ? formatClock(Math.ceil(countdown / 1000), false) :
    '+' + formatClock(Math.floor(-countdown / 1000), true);
  var heelValue = calc.heel === null ? '--' :
    Math.abs(calc.heel).toFixed(1) + '\u00b0 ' +
    (Math.abs(calc.heel) < 0.5 ? 'LEVEL' : calc.heel > 0 ? 'STBD' : 'PORT');

  var fields = {
    countdown: {label: countdown > 0 ? 'Start In' : 'Race Time', value: countdownValue,
                tone: countdown > 0 && countdown <= 60000 ? 'alert' : 'warn'},
    time_line: {label: 'Time To Line', value: calc.lineTime === null ? '--:--' :
                formatClock(Math.round(calc.lineTime), false), tone: 'warn'},
    distance_line: {label: state.startTarget === 'pin' ? 'Distance To Pin' :
                    state.startTarget === 'boat' ? 'Distance To Boat' : 'Distance To Line',
                    value: distanceText(calc.lineDistance, true)},
    speed: {label: 'Boat Speed', value: calc.speed === null ? '--' : calc.speed.toFixed(1) + ' kt',
            tone: 'accent'},
    heading: {label: 'Heading', value: calc.heading === null ? '--' :
              Math.round(calc.heading) + '\u00b0 T'},
    course: {label: 'Course Over Ground', value: calc.course === null ? '--' :
             Math.round(calc.course) + '\u00b0 T'},
    active_mark: {label: 'Active Mark', value: activeName, word: true, tone: 'accent'},
    bearing_mark: {label: 'Bearing To Mark', value: calc.markBearing === null ? '--' :
                   Math.round(calc.markBearing) + '\u00b0 T'},
    distance_mark: {label: 'Distance To Mark', value: distanceText(calc.markDistance, false),
                    tone: calc.markDistance !== null && calc.markDistance < 0.02 ? 'alert' :
                          calc.markDistance !== null && calc.markDistance < 0.05 ? 'warn' : ''},
    vmg_mark: {label: 'VMG To Mark', value: calc.markVmg === null ? '--' :
               calc.markVmg.toFixed(1) + ' kt', tone: calc.markVmg > 0.1 ? 'accent' : 'warn'},
    time_mark: {label: 'Time To Mark', value: calc.markTime === null ? '--:--' :
                formatClock(Math.round(calc.markTime), true)},
    target_heading: {label: 'Target Heading', value: target >= 0 && target <= 359 ?
                     target + '\u00b0 T' : '--'},
    next_mark: {label: 'Next Mark', value: nextName, word: true},
    maneuver: {label: 'Prepare', value: state.maneuver || '--', word: true, tone: 'warn'},
    status: {label: 'Status', value: state.status || '--', word: true, tone: 'accent'},
    heel: {label: 'Heel', value: heelValue,
           tone: calc.heel !== null && Math.abs(calc.heel) >= 20 ? 'warn' : ''},
    leeway: {label: 'Leeway', value: calc.leeway === null ? '--' :
             (calc.leeway >= 0 ? '+' : '') + calc.leeway.toFixed(1) + '\u00b0'},
    drive_speed: {label: 'Drive Speed', value: calc.drive === null ? '--' :
                  calc.drive.toFixed(1) + ' kt', tone: 'accent'}
  };
  return fields[key] || {label: 'Data', value: '--'};
}

function automaticMode() {
  var phase = state.phase || 'prestart';
  if (phase === 'prestart' || phase === 'starting') return 'start';
  if (phase === 'rounding') return 'rounding';
  if (phase === 'finish') return 'finish';
  if (phase === 'custom') return 'custom';
  return 'navigation';
}

function effectiveMode() {
  if (!state) return 'custom';
  if (!state.locked && localMode !== 'follow') return localMode;
  return state.mode === 'auto' ? automaticMode() : state.mode;
}

function resolvedPriorities(mode) {
  var defaults = modeDefaults[mode] || modeDefaults.custom;
  var configured = state.priorities || [];
  return [0, 1, 2, 3].map(function(index) {
    return !configured[index] || configured[index] === 'auto' ?
      defaults[index] : configured[index];
  });
}

function setHero(label, value, sub, word) {
  document.getElementById('heroLabel').textContent = label;
  var valueEl = document.getElementById('heroValue');
  valueEl.textContent = value;
  valueEl.className = 'hero-value' + (word ? ' word' : '');
  document.getElementById('heroSub').textContent = sub || '';
}

function renderMetric(index, data) {
  var card = document.getElementById('metric' + index);
  card.className = 'metric' + (data.tone ? ' ' + data.tone : '');
  card.querySelector('.metric-label').textContent = data.label;
  var value = card.querySelector('.metric-value');
  value.textContent = data.value;
  value.className = 'metric-value' + (data.word ? ' word' : '');
}

function render() {
  if (!state) return;
  var calc = calculations();
  var mode = effectiveMode();
  var phase = phaseLabels[state.phase] || 'Crew Display';
  document.getElementById('phase').textContent = phase;
  document.body.className = mode + '-mode';

  if (mode === 'start') {
    var beforeStart = calc.remainingMs > 0;
    var value = !state.race || state.race.state === 'idle' || !Number(state.race.t0_ms) ? '--:--' :
      beforeStart ? formatClock(Math.ceil(calc.remainingMs / 1000), false) :
      '+' + formatClock(Math.floor(-calc.remainingMs / 1000), true);
    setHero(beforeStart ? 'Start In' : 'Race Time', value,
      state.startTarget === 'pin' ? 'Target: Pin End' :
      state.startTarget === 'boat' ? 'Target: Boat End' : 'Target: Start Line', false);
    if (beforeStart && calc.remainingMs <= 60000) document.body.classList.add('final-minute');
  } else if (mode === 'rounding') {
    var approach = calc.markDistance !== null && calc.markDistance < 0.02 ? ' - FINAL 120 FT' :
                   calc.markDistance !== null && calc.markDistance < 0.05 ? ' - FINAL APPROACH' :
                   calc.markDistance !== null && calc.markDistance < 0.10 ? ' - APPROACH' : '';
    setHero('Rounding', state.activeMark ? state.activeMark.name : 'No Mark',
      calc.markDistance === null ? '' : distanceText(calc.markDistance, false) + approach, true);
  } else if (mode === 'performance') {
    setHero('Boat Speed', calc.speed === null ? '--' : calc.speed.toFixed(1),
      calc.speed === null ? 'knots' : 'knots', false);
  } else if (mode === 'finish') {
    var elapsed = state.race && Number(state.race.t0_ms) ?
      Math.max(0, (Number(state.race.end_ms || nowServerMs()) - Number(state.race.t0_ms)) / 1000) : 0;
    setHero('Finished', formatClock(elapsed, true), state.courseName || 'Race complete', false);
  } else if (mode === 'custom') {
    var first = fieldData(resolvedPriorities(mode)[0], calc);
    setHero(first.label, first.value, state.status || '', first.word);
  } else {
    setHero('Active Mark', state.activeMark ? state.activeMark.name : 'No Mark',
      state.courseName || (state.status || ''), true);
  }

  resolvedPriorities(mode).forEach(function(key, index) {
    renderMetric(index, fieldData(key, calc));
  });

  var message = '';
  if (mode === 'rounding' && state.maneuver) message = 'PREPARE: ' + state.maneuver;
  else if (state.status) message = state.status;
  else if (state.maneuver) message = 'PREPARE: ' + state.maneuver;
  else if (state.nextMark) message = 'NEXT: ' + state.nextMark.name;
  else message = state.locked ? 'Display controlled by tactician' : 'Local view enabled';
  document.getElementById('message').textContent = message;

  var meta = mode.replace('_', ' ') + (state.locked ? ' / LOCKED' : '');
  document.getElementById('footerMeta').textContent = meta;
  var viewButton = document.getElementById('viewButton');
  viewButton.hidden = !!state.locked;
  viewButton.textContent = 'View: ' + (localMode === 'follow' ? 'Follow' : localMode);
}

async function requestDisplayMode() {
  try {
    if (document.documentElement.requestFullscreen && !document.fullscreenElement)
      await document.documentElement.requestFullscreen();
  } catch (error) {}
  try {
    if ('wakeLock' in navigator) wakeLock = await navigator.wakeLock.request('screen');
  } catch (error) {}
}

document.getElementById('viewButton').addEventListener('click', function() {
  requestDisplayMode();
  var index = localModes.indexOf(localMode);
  localMode = localModes[(index + 1) % localModes.length];
  try { localStorage.setItem('crewLocalMode', localMode); } catch (error) {}
  render();
});

document.addEventListener('visibilitychange', function() {
  if (document.visibilityState === 'visible') requestDisplayMode();
});

function updateConnection() {
  var age = Date.now() - lastFetchAt;
  var connected = state && age < 1800;
  var connection = document.getElementById('connection');
  connection.textContent = connected ? 'Live' : 'Reconnecting';
  connection.className = 'connection' + (connected ? '' : ' offline');
  document.body.classList.toggle('long-offline', !!state && age > 8000);
}

function fetchState() {
  if (fetchInFlight) return;
  fetchInFlight = true;
  fetch('/crew/state', {cache: 'no-store'}).then(function(response) {
    if (!response.ok) throw new Error('HTTP ' + response.status);
    return response.json();
  }).then(function(next) {
    if (lastRevision !== null && next.revision !== lastRevision) {
      localMode = 'follow';
      try { localStorage.setItem('crewLocalMode', localMode); } catch (error) {}
    }
    if (next.locked) localMode = 'follow';
    lastRevision = next.revision;
    state = next;
    receivedAt = performance.now();
    lastFetchAt = Date.now();
    render();
    updateConnection();
  }).catch(function() {
    updateConnection();
  }).then(function() {
    fetchInFlight = false;
  });
}

try {
  var storedMode = localStorage.getItem('crewLocalMode');
  if (localModes.indexOf(storedMode) >= 0) localMode = storedMode;
} catch (error) {}

setInterval(fetchState, 250);
setInterval(function() { render(); updateConnection(); }, 100);
fetchState();
</script>
</body>
</html>)crewhtml";
    return ui;
}
