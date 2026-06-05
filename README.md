# Sailing Computer

An ESP32-based sailing computer using the **Unicore UM982** dual-antenna GNSS module. Provides centimeter-accuracy RTK position, true heading, roll, speed, and course — all accessible via a password-protected HTTPS web dashboard and broadcast as standard NMEA 0183 sentences over TCP for use with OpenCPN, Signal K, or any chart plotter.

---

## Features

- **RTK GNSS position** — RTK Fixed / Float via NTRIP corrections (fix quality 4/5)
- **Dual-antenna true heading** — derived from the UM982 baseline, corrected for antenna mounting orientation
- **Roll (heel angle)** — from the dual-antenna baseline with athwartships mounting
- **NMEA 0183 TCP broadcast** — port 10110, compatible with OpenCPN, Signal K, Navionics, etc.
- **BLE NMEA** — broadcasts NMEA over Bluetooth LE (NUS + HM-10 services) for SW Maps, iNavX, and other iOS/Android navigation apps
- **NTRIP client with failover** — up to 3 correction sources (e.g. rtk2go, onocoy, rtkdata) with automatic failover after 3 consecutive failures
- **HTTPS web dashboard** — live status (public), configuration and system management (password protected) at `https://sailingcomputer.local`
- **WiFi AP + STA modes** — creates its own hotspot or joins an existing network
- **OTA firmware updates** — flash new firmware over WiFi from the System tab (no USB required after initial flash)
- **NVS-backed configuration** — all settings persist across reboots
- **UM982 factory reset** — one-click reconfiguration from the web UI

---

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32-WROOM (MiniESP32 v1.0) |
| GNSS module | Unicore UM982 dual-antenna breakout |
| Power | 3.3V |

### Wiring

| Wire | UM982 Pin | ESP32 Pin | Purpose |
|------|-----------|-----------|---------|
| Red | VCC | 3.3V | Power |
| Black | GND | GND | Ground |
| Yellow | TXD (COM1) | IO16 (RX1) | NMEA data → ESP32 |
| Orange | RXD (COM1) | IO17 (TX1) | ESP32 → UM982 commands |
| White | RX2 (COM2) | IO18 (TX2) | RTCM corrections → UM982 |
| Brown | TX2 (COM2) | IO19 (RX2) | UM982 → ESP32 (unused) |

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

The upload port is set to `/dev/tty.usbserial-0001` in `platformio.ini`. Adjust if your port differs.

### OTA Updates (after initial flash)

1. Build the firmware: `pio run`
2. Open `https://sailingcomputer.local` → **System** tab (login with admin credentials)
3. Click **Choose .bin file** → select `.pio/build/esp32dev/firmware.bin`
4. Click **Upload & Flash** — progress bar shows upload status, device restarts automatically

---

## Serial Ports

| Port | Pins | Purpose |
|------|------|---------|
| UART0 | USB | Debug console (115200 baud) |
| UART1 | RX=16, TX=17 | UM982 COM1 — NMEA output |
| UART2 | RX=19, TX=18 | UM982 COM2 — RTCM input |

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

### Status Tab (public — no login required)

Live-updating display (2 second refresh):

| Field | Source |
|-------|--------|
| Fix Type | GGA field 7 (1=GPS, 2=DGPS, 4=RTK Fixed, 5=RTK Float) |
| Satellites | GGA field 8 |
| Latitude / Longitude | GGA fields 3–6 (7 decimal places at RTK, 5 at GPS) |
| True Heading | HEADINGA field 4 + heading offset |
| Roll | HEADINGA field 5 (tilt of athwartships baseline = heel angle) |
| Leeway / Lateral Drift | Derived from heading vs COG difference |
| SOG (knots) | VTG field 6 |
| COG | VTG field 2 |
| Drive Speed | Speed component in heading direction |
| HDOP | GGA field 9 |
| Altitude (m) | GGA field 10 |
| NTRIP | Connected / Off + active source number |
| RTCM Bytes | Total bytes received from NTRIP caster |
| BLE | On/Off status |
| NMEA sentences/sec | Throughput counter |

### Configuration Tab (login required)

- **WiFi** — AP mode toggle, SSID, password; or join existing network
- **Web Admin Password** — change the HTTP Basic Auth password (username always `admin`)
- **Antenna** — heading offset in degrees (default 90° for athwartships mounting), COG minimum SOG
- **NTRIP Corrections** — 3 source slots (host, port, mountpoint, username, password) with enable toggle; automatic failover after 3 consecutive failures
- **BLE NMEA** — enable/disable Bluetooth LE NMEA broadcast

### System Tab (login required)

- NMEA TCP connection instructions with device IP
- **Restart Device**
- **UM982 Factory Reset** — sends `FRESET`, reconfigures `SIGNALGROUP 4 5`, restores NMEA outputs (~15 seconds)
- **Firmware Update (OTA)** — upload `.bin` file with progress bar

### Authentication

- Default credentials: **admin / admin**
- Change the password in the Configuration tab → "Web Admin Password" section
- A **Log Out** button in the nav bar clears the browser's cached credentials
- Closing the browser tab also clears credentials (auto-logout via `beforeunload`)

---

## NMEA Output (TCP port 10110)

Sentences broadcast to all connected clients:

| Sentence | Content |
|----------|---------|
| `$GNGGA` | Position, fix quality, satellites, HDOP, altitude |
| `$GPHDT` | True heading (corrected for antenna offset) |
| `$GPVTG` | Course and speed over ground |
| `$GNRMC` | Date, time, position, speed, course |

> **Note:** `#HEADINGA` (Unicore proprietary) is parsed internally for heading/roll but is **not** broadcast — only standard `$` sentences are sent.

### OpenCPN Setup

1. Options → Connections → Add Connection
2. Type: **Network**, Protocol: **TCP**
3. Address: `192.168.x.x` (device IP), Port: `10110`
4. Direction: **Input**

---

## BLE NMEA

When enabled, NMEA sentences are broadcast over Bluetooth LE using two services simultaneously:

| Service | UUID | Compatible Apps |
|---------|------|-----------------|
| NUS (Nordic UART) | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | iNavX, ArduSimple BLE Bridge, SparkFun RTK |
| HM-10 | `FFE0` / `FFE1` | SW Maps (iOS) |

> **SW Maps note:** SW Maps iOS scans for the HM-10 `FFE0` service. Select **Generic NMEA (Bluetooth LE)** in SW Maps → Bluetooth GPS connections.

---

## UM982 Configuration

The UM982 is configured at boot via `um982Init()` which restores NMEA output settings. Configuration is saved to UM982 flash, so it survives power cycles.

**Signal groups for dual-antenna heading:**

| Group | Constellations |
|-------|---------------|
| 4 (ANT1) | GPS L1/L2 + GLONASS L1/L2 + BeiDou B1/B2 |
| 5 (ANT2) | GPS L1/L2 + Galileo E1/E5b + BeiDou B1/B2 |

Only run **UM982 Factory Reset** (System tab) when:
- Antennas are first installed
- Heading/roll stops working after a hardware change
- The UM982 flash config is corrupted

---

## Project Structure

```
SailingComputer/
├── platformio.ini          # PlatformIO build config (ESP-IDF framework, port, monitor filters)
├── sdkconfig.defaults      # ESP-IDF build-time config (BLE, HTTPS, mbedTLS, OTA, logging)
├── partitions.csv          # Custom partition table (OTA + NVS)
├── certs/
│   ├── cert.pem            # Self-signed TLS certificate (sailingcomputer.local, 10 years)
│   └── key.pem             # RSA-2048 private key
└── src/
    ├── main.cpp            # WiFi, NMEA parsing, NTRIP client, web server, OTA, BLE glue
    ├── config.h            # Config struct + NVS load/save (ConfigManager)
    ├── um982.h             # UM982 UART init, command helpers, factory reset
    ├── ble_nmea.h          # BLE NUS + HM-10 GATT server for NMEA broadcast
    ├── certs.h             # TLS cert + key as embedded C string literals
    └── webui.h             # Single-page web UI (HTML/CSS/JS, inline)
```

---

## Configuration Reference

All settings are stored in NVS namespace `sailcomp`.

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

---

## License

MIT
