# Sailing Computer

An ESP32-P4-based sailing computer using the **Unicore UM982** dual-antenna GNSS module. Provides centimeter-accuracy RTK position, true heading, roll, speed, and course — all accessible via a password-protected HTTPS web dashboard and broadcast as standard NMEA 0183 sentences over TCP for use with OpenCPN, Signal K, or any chart plotter.

---

## Features

- **RTK GNSS position** — RTK Fixed / Float via NTRIP corrections (fix quality 4/5)
- **Dual-antenna true heading** — derived from the UM982 baseline, corrected for antenna mounting orientation
- **Roll (heel angle)** — from the dual-antenna baseline with athwartships mounting
- **NMEA 0183 TCP broadcast** — port 10110, compatible with OpenCPN, Signal K, Navionics, etc.
- **BLE NMEA** — broadcasts NMEA over Bluetooth LE (NUS + HM-10 services) for SW Maps, iNavX, and other iOS/Android navigation apps
- **NTRIP client with failover** — up to 3 correction sources (e.g. rtk2go, onocoy, rtkdata) with automatic failover after 3 consecutive failures
- **HTTPS web dashboard** — live status (public), configuration and system management (password protected) at `https://sailingcomputer.local`
- **Phone-first racing interface** — large controls, safe-area support, and a four-button bottom navigation designed for iPhone portrait use underway
- **WiFi AP + STA modes** — creates its own hotspot or joins an existing network
- **OTA firmware updates** — flash new firmware over WiFi from the System tab (no USB required after initial flash)
- **ESP32-C6 coprocessor updates** — reflash the onboard WiFi/Bluetooth processor from a tested image bundled into the P4 firmware
- **NVS-backed configuration** — all settings persist across reboots
- **SD card storage** — marks, courses, and GPX files stored on SD card with SPIFFS fallback
- **Mark & course manager** — save GPS positions as named marks, build courses from mark sequences, import from GPX
- **Race start sequence** — countdown clock (5 / 10 / 15 min, adjustable ±1 min), tap-to-sync with committee boat, time-to-start-line
- **Race navigation** — bearing, distance, and ETA to next mark; previous/next mark controls
- **Multi-lap courses** — select 1–5 laps and automatically repeat the course's interior marks before finishing
- **Race stats** — elapsed time, leg splits, and marks-rounded summary on race completion
- **SD card file manager** — browse, rename, copy, delete files and directories; format SD card
- **Track recording** — auto-starting continuous loop buffer on SD card, time-range GPX export with heading/heel/SOG/COG extensions, configurable recording interval (1–60 s) and loop duration (1–24 h)

---

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32-P4 (esp32-p4-function-ev-board) |
| GNSS module | Unicore UM982 dual-antenna |
| Storage | MicroSD card (SPIFFS fallback) |
| Power | 3.3 V |

### Wiring

| Signal | UM982 Pin | ESP32-P4 Pin | Purpose |
|--------|-----------|--------------|---------|
| NMEA/control | COM2 TX | GPIO 32 (RX) | NMEA sentences and responses → ESP32 |
| NMEA/control | COM2 RX | GPIO 33 (TX) | ESP32 commands → UM982 |
| RTCM | COM1 RX | GPIO 48 (TX) | NTRIP corrections → UM982 |
| RTCM diag | COM1 TX | GPIO 47 (RX) | Auxiliary / diagnostic |
| PPS | PPS out | GPIO 27 | 1 Hz timing pulse |
| Debug | — | USB (UART0) | Debug console only |

### Antenna Mounting

Two SMA antennas are required, spaced **≥ 1 metre apart**.

**Default configuration (aft rail, athwartships):**
- **ANT1 (primary)** — starboard
- **ANT2 (heading)** — port
- Heading offset: **+90°** (configurable in web UI)

With this mounting the UM982 baseline measures:
- **Heading** — corrected to true bow heading
- **Roll** — heel angle (baseline tilt = port/starboard lean)

For **fore/aft mounting** (bow/stern), set heading offset to 0° or 180° in the Antenna section of the Configuration tab.

---

## Software

### Requirements

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- ESP-IDF framework (installed automatically by PlatformIO via `framework = espidf` in `platformio.ini`)
- NimBLE component (fetched automatically via `idf_component.yml`)

### Build & Flash (first time, USB)

```bash
git clone https://github.com/aziobro/SailingComputer.git
cd SailingComputer
pio run --target upload
pio device monitor
```

The upload port is set in `platformio.ini`. Adjust if your port differs.

The current 32 MB partition table provides two 8 MB application slots. A full
USB flash is required when first installing this layout; application-only OTA
updates do not rewrite the partition table.

### OTA Updates (after initial flash)

Use the included `flash.sh` script, or update manually:

1. Build the firmware: `pio run`
2. Open `https://sailingcomputer.local` → **System** tab (login with admin credentials)
3. Click **Choose .bin file** → select `.pio/build/esp32p4/firmware.bin`
4. Click **Upload & Flash** — progress bar shows upload status, device restarts automatically

### ESP32-C6 Coprocessor Updates

The **System** page shows the C6 firmware currently running and the version
bundled into the ESP32-P4 application. Press **Update ESP32-C6** to stream the
bundled image to the coprocessor over the existing `esp_hosted` SDIO link.
Progress remains visible in the web interface. When activation completes, the
C6 and P4 restart so the WiFi transport can reconnect cleanly.

The running value is the C6's `esp_hosted` protocol version. The bundled value
and build date come from the embedded image metadata, so a custom image that
keeps the same protocol version can still report a different bundled version.

The bundled image is `src/c6_slave_fw.bin`. To replace it with another
compatible `network_adapter` build:

```bash
cp path/to/network_adapter.bin src/c6_slave_fw.bin
pio run -e esp32p4
```

The image must target the same ESP32-C6 hardware and SDIO wiring. Perform
coprocessor updates ashore because WiFi disconnects at the end of the process.

---

## Network

| Service | Details |
|---------|---------|
| Default AP SSID | `SailingComputer` |
| Default AP Password | `sailing123` |
| mDNS | `sailingcomputer.local` |
| Web UI | `https://sailingcomputer.local` (port 443, self-signed cert) |
| HTTP redirect | port 80 → HTTPS |
| NMEA TCP | port **10110** |

> **Note:** The web UI uses a self-signed TLS certificate. Your browser will show a security warning on first visit — this is expected. Accept the exception to proceed; the connection is still encrypted.

---

## Web Interface

The single-page app opens directly on **Race** and is designed for iPhone
portrait use. The fixed bottom bar keeps the primary underway pages within
thumb reach:

- **Race** — start sequence, start line, course, and live race navigation
- **Data** — GNSS, heading, motion, NTRIP, and connection status
- **Marks** — mark library, courses, and GPX import
- **More** — Track Recording, Files and Storage, Device Settings, System and Updates, and Log Out

Controls use large touch targets, 16 px form text to avoid iOS zoom, and safe
area padding for phones with a home indicator.

### Data (public — no login required)

Live display (500 ms refresh):

| Field | Source |
|-------|--------|
| Fix Type | GGA field 7 (1=GPS, 2=DGPS, 4=RTK Fixed, 5=RTK Float) |
| Satellites | GGA field 8 |
| Latitude / Longitude | GGA (7 dp at RTK, 5 dp at GPS) |
| True Heading | HEADINGA + heading offset |
| Roll (heel) | HEADINGA baseline tilt |
| Leeway / Lateral Drift | Heading vs COG difference |
| SOG (knots) | VTG |
| COG | VTG (smoothed, frozen below min-SOG threshold) |
| Drive Speed | Speed component in heading direction |
| HDOP / Altitude | GGA |
| NTRIP status | Connected / Off + active source |
| BLE status | Advertising / Connected (if enabled) |

### Configuration (login required)

- **WiFi** — AP mode toggle, SSID, password; or join existing network
- **Web Admin Password** — change the HTTP Basic Auth password
- **Antenna** — heading offset (default 90° for athwartships), COG minimum SOG
- **NTRIP Corrections** — 3 source slots with enable toggle and automatic failover
- **BLE NMEA** — enable/disable Bluetooth LE NMEA broadcast
- **GPS Update Rate** — 1 / 2 / 5 / 10 / 20 Hz

### Race

Race sequence management and live racing navigation.

#### Pre-start setup

- **Countdown clock** — large color-coded display: white → yellow at T‑4:00 → red at T‑1:00
- **Duration** — 5 / 10 / 15 min presets plus ±1 min increment/decrement buttons
- **ARM** — starts the countdown; **Reset** aborts and returns to setup
- **Tap to sync** — tapping the clock face snaps T‑0 to the nearest whole minute, allowing re-sync with the committee boat gun at any point during the sequence

#### Start Line (collapsible card)

Each end (Port / Starboard) can be set by:
- **GPS** — captures current GPS position
- **Saved mark** — pick from the mark library dropdown

When both ends are set, **Time to Line** is shown during the countdown (distance to start line ÷ current SOG).

#### Course

Select an active course from saved courses. The selected course drives
mark-by-mark navigation after the start. For courses with distinct start and
finish marks, choose **1–5 laps**; interior marks repeat for each lap while the
start and finish remain single occurrences.

#### Racing (post-start)

- **Elapsed clock** — time since the gun (green)
- **Next mark** — name, distance (nm), true bearing, and ETA at current SOG
- **← Prev Mark / Next Mark →** — step forward or backward through course legs
- **End Race** — stops the timer and navigates to the race stats view

#### Race Stats

Displayed after **End Race** or automatic completion at the last mark:

- Total elapsed time (gun to finish)
- Course name and marks-rounded count
- Leg splits table: each mark with elapsed time from T‑0 and per-leg split time

**New Race** clears all data and returns to setup.

### Marks / Routes (login required)

- **Mark library** — list all saved marks with coordinates; delete individual marks
- **Add mark** — enter name + lat/lon manually, or tap "Use GPS Position" to capture current position
- **Course list** — view saved courses with mark sequences and rounding directions
- **GPX import** — upload a `.gpx` file to bulk-import waypoints as marks and routes as courses

### Tracks (public)

Loop-buffer track recording and GPX export.

- **Loop recording** — the circular buffer auto-starts at boot on SD card at `/sdcard/tracks/.loop.bin`; it can be disabled manually and pauses automatically when GPS fix is lost
- **Segment extraction** — choose start and end times with the range controls, then tap **Export GPX**
- **File written indicator** — shows the filename of the last successfully exported GPX
- **Recording Settings** — configure recording interval (1 / 5 / 10 / 30 / 60 s) and loop buffer duration (1 – 24 h); changing these settings recreates the loop file
- Downloaded via the Files tab — no separate download UI

GPX files are written to `/sdcard/tracks/` and named `track_YYYYMMDD_HHMMSSz_to_HHMMSSz.gpx`.  Each `<trkpt>` includes extensions `<sc:hdt>`, `<sc:heel>`, `<sc:sog>`, `<sc:cog>`.

### Files (public)

Browse the SD card (or SPIFFS) file system:
- Navigate directories with breadcrumb trail
- Download, rename, copy, or delete individual files
- **Format SD card** — two-step confirmation erase and reinitialise

### System (login required)

- Device IP and NMEA TCP connection instructions
- **Restart Device**
- **UM982 Factory Reset** — sends `FRESET`, reconfigures signal groups and NMEA outputs (~15 s)
- **ESP32-C6 update** — compare running and bundled versions, then flash the onboard WiFi/Bluetooth coprocessor with live progress
- **ESP32-P4 firmware update (OTA)** — upload `.bin` file with live progress bar
- **BLE toggle** — enable/disable without a full config save/restart

### Authentication

- Default credentials: **admin / admin**
- Change the password in the Configuration tab → "Web Admin Password"
- A **Log Out** action in the More menu clears the browser's cached credentials
- Closing the browser tab also clears credentials (auto-logout via `beforeunload`)

---

## NMEA Output (TCP port 10110)

| Sentence | Content |
|----------|---------|
| `$GNGGA` | Position, fix quality, satellites, HDOP, altitude |
| `$GPHDT` | True heading (corrected for antenna offset) |
| `$GPVTG` | Course and speed over ground |
| `$GNRMC` | Date, time, position, speed, course |

### OpenCPN Setup

1. Options → Connections → Add Connection
2. Type: **Network**, Protocol: **TCP**
3. Address: device IP, Port: `10110`
4. Direction: **Input**

---

## BLE NMEA

When enabled, NMEA sentences are broadcast over Bluetooth LE using two services simultaneously:

| Service | UUID | Compatible Apps |
|---------|------|-----------------|
| NUS (Nordic UART) | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | iNavX, ArduSimple BLE Bridge, SparkFun RTK |
| HM-10 | `FFE0` / `FFE1` | SW Maps (iOS) |

> **SW Maps note:** Select **Generic NMEA (Bluetooth LE)** in SW Maps → Bluetooth GPS connections.

---

## UM982 Configuration

The UM982 is configured at boot via `um982Init()`. Configuration is saved to UM982 flash and survives power cycles.

**Signal groups for dual-antenna heading:**

| Group | Constellations |
|-------|---------------|
| 4 (ANT1) | GPS L1/L2 + GLONASS L1/L2 + BeiDou B1/B2 |
| 5 (ANT2) | GPS L1/L2 + Galileo E1/E5b + BeiDou B1/B2 |

Only run **UM982 Factory Reset** when:
- Antennas are first installed
- Heading/roll stops working after a hardware change
- The UM982 flash config is corrupted

---

## Project Structure

```
SailingComputer/
├── platformio.ini          # PlatformIO build config (ESP-IDF, board, port)
├── sdkconfig.defaults      # ESP-IDF build-time config (BLE, HTTPS, mbedTLS, OTA)
├── partitions.csv          # 32 MB table: dual 8 MB OTA slots + NVS + SPIFFS
├── flash.sh                # Build and OTA-upload helper script
├── certs/
│   ├── cert.pem            # Self-signed TLS certificate (10-year validity)
│   └── key.pem             # RSA-2048 private key
└── src/
    ├── main.cpp            # WiFi, NMEA parsing, NTRIP, web server, OTA, race engine
    ├── c6_slave_fw.bin     # Bundled ESP32-C6 esp_hosted network_adapter image
    ├── config.h            # Config struct + NVS load/save (ConfigManager)
    ├── storage.h           # StorageManager — SD card / SPIFFS, marks, courses JSON
    ├── gpx.h               # GPX file parser (waypoints → marks, routes → courses)
    ├── track.h             # TrackRecorder — circular loop buffer + GPX export
    ├── um982.h             # UM982 UART init, command helpers, factory reset
    ├── ble_nmea.h          # BLE NUS + HM-10 GATT server for NMEA broadcast
    ├── certs.h             # TLS cert + key as embedded C string literals
    ├── version.h           # Firmware version string
    └── webui.h             # Single-page web UI (HTML/CSS/JS, inline)
```

---

## Race API

The race engine exposes a REST API consumed by the web UI. All endpoints are unauthenticated (operational controls, not configuration).

| Method | Path | Body / Notes |
|--------|------|--------------|
| GET | `/race/state` | Full race state JSON (clock, line, course, legs) |
| POST | `/race/start` | Arm countdown: now + duration |
| POST | `/race/stop` | Reset to idle, clear all race data |
| POST | `/race/end` | End race mid-course, preserve data, show stats |
| POST | `/race/sync` | Snap T‑0 to nearest whole minute |
| POST | `/race/duration` | `{"seconds":300}` — set sequence length (idle only) |
| POST | `/race/startline` | `{"end":0,"lat":…,"lon":…}` or `{"end":0,"markId":"…"}` |
| POST | `/race/course` | `{"courseId":"…","leg":0}` — set active course |
| POST | `/race/laps` | `{"laps":2}` — select 1–5 laps (idle only) |
| POST | `/race/nextleg` | Advance to next mark (auto-completes at last mark) |
| POST | `/race/prevleg` | Step back to previous mark |

Race state machine: `idle` → `countdown` → `racing` → `complete`

---

## Track API

| Method | Path | Notes |
|--------|------|-------|
| GET | `/tracks/status` | Loop state JSON (running, count, timestamps, segment, last file) |
| POST | `/tracks/loop/start` | Start loop recording |
| POST | `/tracks/loop/stop` | Stop loop recording (also cancels open segment) |
| POST | `/tracks/segment/export` | `{"t0":…,"t1":…}` — export a selected UTC time range |
| POST | `/tracks/config` | `{"intervalSec":5,"loopHours":3}` — save settings + rebuild loop |

## Firmware API

Firmware-management endpoints require HTTP Basic Auth.

| Method | Path | Notes |
|--------|------|-------|
| POST | `/update` | Upload and install a raw ESP32-P4 application binary |
| GET | `/c6/status` | Running/bundled C6 versions, image metadata, and update progress |
| POST | `/c6/update` | Stream the bundled image to the ESP32-C6 and restart both processors |

---

## Configuration Reference

All settings stored in NVS namespace `sailcomp`.

| Key | Type | Description |
|-----|------|-------------|
| `wifiSSID` | string | Station mode SSID |
| `wifiPass` | string | Station mode password |
| `apMode` | bool | true = AP mode |
| `apSSID` | string | AP hotspot name |
| `apPass` | string | AP hotspot password |
| `adminPass` | string | HTTP Basic Auth password (username: admin) |
| `n0host`–`n2host` | string | NTRIP source hostnames |
| `n0port`–`n2port` | uint16 | NTRIP source ports |
| `n0mount`–`n2mount` | string | NTRIP mountpoints |
| `n0user`–`n2user` | string | NTRIP usernames |
| `n0pass`–`n2pass` | string | NTRIP passwords |
| `n0enabled`–`n2enabled` | bool | Source enable flags |
| `hdgOffset` | float | Heading correction in degrees (default 90.0) |
| `cogMinSog` | float | COG freeze threshold in knots (default 0.1) |
| `bleNmea` | bool | BLE NMEA broadcast enabled |
| `gpsRate` | uint8 | GPS update rate in Hz (1/2/5/10/20) |
| `trackInterval` | uint8 | Track recording interval in seconds (default 5) |
| `trackLoopHrs` | uint8 | Loop buffer duration in hours (default 3) |

---

## License

MIT
