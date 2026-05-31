# Sailing Computer

An ESP32-based sailing computer using the **Unicore UM982** dual-antenna GNSS module. Provides centimeter-accuracy RTK position, true heading, roll, speed, and course — all accessible via a web dashboard and broadcast as standard NMEA 0183 sentences over TCP for use with OpenCPN, Signal K, or any chart plotter.

---

## Features

- **RTK GNSS position** — RTK Fixed / Float via NTRIP corrections (fix quality 4/5)
- **Dual-antenna true heading** — derived from the UM982 baseline, corrected for antenna mounting orientation
- **Roll (heel angle)** — from the dual-antenna baseline with athwartships mounting
- **NMEA 0183 TCP broadcast** — port 10110, compatible with OpenCPN, Signal K, Navionics, etc.
- **NTRIP client with failover** — up to 3 correction sources (e.g. rtk2go, onocoy, rtkdata) with automatic failover after 3 consecutive failures
- **Web dashboard** — live status, configuration, and system management at `http://sailingcomputer.local`
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
- ESP32 Arduino core (installed automatically by PlatformIO)
- No external libraries required — all dependencies are built into the ESP32 Arduino core

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
2. Open `http://sailingcomputer.local` → **System** tab
3. Click **Choose .bin file** → select `.pio/build/esp32dev/firmware.bin`
4. Click **Upload & Flash** — progress bar shows upload status, device restarts automatically

---

## Serial Ports

| Port | Pins | Purpose |
|------|------|---------|
| Serial0 (UART0) | USB | Debug console (115200 baud) |
| Serial1 (UART1) | RX=16, TX=17 | UM982 COM1 — NMEA output |
| Serial2 (UART2) | RX=19, TX=18 | UM982 COM2 — RTCM input |

---

## Network

| Service | Details |
|---------|---------|
| Default AP SSID | `SailingComputer` |
| Default AP Password | `sailing123` |
| mDNS | `sailingcomputer.local` |
| Web UI | `http://sailingcomputer.local` (port 80) |
| NMEA TCP | port **10110** |

---

## Web Interface

### Status Tab

Live-updating display (2 second refresh):

| Field | Source |
|-------|--------|
| Fix Type | GGA field 7 (1=GPS, 4=RTK Fixed, 5=RTK Float) |
| Satellites | GGA field 8 |
| Latitude / Longitude | GGA fields 3–6, converted from DDMM.MMMMM to decimal degrees |
| True Heading | HEADINGA field 4 + heading offset |
| Roll | HEADINGA field 5 (tilt of athwartships baseline = heel angle) |
| Heading Src | Dual Antenna / No ANT2 Lock |
| SOG (knots) | VTG field 6 |
| COG | VTG field 2 (Doppler-based, no correction needed) |
| HDOP | GGA field 9 |
| Altitude (m) | GGA field 10 |
| NTRIP | Connected / Off + active source number |
| RTCM Bytes | Total bytes received from NTRIP caster |

### Configuration Tab

- **WiFi** — AP mode toggle, SSID, password
- **Antenna** — heading offset in degrees (default 90° for athwartships mounting)
- **NTRIP Corrections** — 3 source slots (host, port, mountpoint, username, password) with enable toggle per source; automatic failover after 3 consecutive failures
- **AP Credentials** — change the hotspot SSID and password

### System Tab

- NMEA TCP connection instructions
- **Restart Device**
- **UM982 Factory Reset** — sends `FRESET`, reconfigures `SIGNALGROUP 4 5`, restores NMEA outputs (~15 seconds)
- **Firmware Update (OTA)** — upload `.bin` file with progress bar

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
├── platformio.ini       # PlatformIO build config
└── src/
    ├── main.cpp         # Main sketch — WiFi, NMEA, NTRIP, web handlers, OTA
    ├── config.h         # Config struct + NVS load/save (ConfigManager)
    ├── um982.h          # UM982 init, factory reset, NMEA configuration
    └── webui.h          # Inline HTML/CSS/JS web interface
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
| `n0host`–`n2host` | string | NTRIP source hostnames |
| `n0port`–`n2port` | uint16 | NTRIP source ports |
| `n0mount`–`n2mount` | string | NTRIP mountpoints |
| `n0user`–`n2user` | string | NTRIP usernames |
| `n0pass`–`n2pass` | string | NTRIP passwords |
| `n0enabled`–`n2enabled` | bool | Source enable flags |
| `hdgOffset` | float | Heading correction in degrees (default 90.0) |

---

## License

MIT
