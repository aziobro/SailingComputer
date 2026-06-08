#!/usr/bin/env bash
# flash.sh — Build and flash the Sailing Computer firmware to the ESP32-P4.
# Usage:
#   ./flash.sh           # build + flash
#   ./flash.sh build     # build only (no flash)
#   ./flash.sh monitor   # build + flash + open serial monitor
#   ./flash.sh clean     # clean build cache, then build + flash

set -euo pipefail

# ── Colours ──────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'

info()    { echo -e "${CYAN}▶  $*${RESET}"; }
success() { echo -e "${GREEN}✔  $*${RESET}"; }
warn()    { echo -e "${YELLOW}⚠  $*${RESET}"; }
die()     { echo -e "${RED}✘  $*${RESET}" >&2; exit 1; }

# ── Config ────────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT=$(grep '^upload_port' "$SCRIPT_DIR/platformio.ini" \
         | awk -F'=' '{print $2}' | tr -d ' ')
VERSION=$(grep 'FW_VERSION' "$SCRIPT_DIR/src/version.h" \
            | grep -oE '"[^"]+"' | tr -d '"')

MODE="${1:-flash}"   # build | flash | monitor | clean

# ── Pre-flight checks ─────────────────────────────────────────────────────────
command -v pio &>/dev/null || die "PlatformIO CLI (pio) not found. Install with: pip install platformio"

if [[ "$MODE" != "build" ]]; then
    if [[ ! -e "$PORT" ]]; then
        warn "Upload port $PORT not found."
        echo    "  Is the ESP32-P4 connected via USB?"
        echo    "  Edit upload_port in platformio.ini if the path has changed."
        echo
        echo    "  Available USB serial ports:"
        ls /dev/tty.usb* /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | sed 's/^/    /' || echo "    (none found)"
        echo
        [[ "$MODE" == "flash" || "$MODE" == "monitor" ]] && die "Cannot flash without a connected device."
    fi
fi

cd "$SCRIPT_DIR"

# ── Auto-bump patch version ───────────────────────────────────────────────────
VERSION_H="$SCRIPT_DIR/src/version.h"
MAJOR=$(echo "$VERSION" | cut -d. -f1)
MINOR=$(echo "$VERSION" | cut -d. -f2)
PATCH=$(echo "$VERSION" | cut -d. -f3)
PATCH=$(( PATCH + 1 ))
VERSION="${MAJOR}.${MINOR}.${PATCH}"
# Rewrite version.h preserving all lines except the #define
{
    grep -v '^#define FW_VERSION' "$VERSION_H"
    echo "#define FW_VERSION \"${VERSION}\""
} > "${VERSION_H}.tmp" && mv "${VERSION_H}.tmp" "$VERSION_H"
info "Version bumped to ${VERSION}"

echo
echo -e "${BOLD}━━━ Sailing Computer Firmware v${VERSION} ━━━${RESET}"
echo -e "  Port   : ${CYAN}${PORT}${RESET}"
echo -e "  Mode   : ${CYAN}${MODE}${RESET}"
echo

START_TIME=$SECONDS

# ── Clean (optional) ──────────────────────────────────────────────────────────
if [[ "$MODE" == "clean" ]]; then
    info "Cleaning build cache…"
    pio run --target clean
    MODE="flash"
fi

# ── Build ─────────────────────────────────────────────────────────────────────
info "Compiling firmware…"
if pio run; then
    success "Build complete"
else
    die "Build failed — check the errors above"
fi

# ── Flash ─────────────────────────────────────────────────────────────────────
if [[ "$MODE" == "flash" || "$MODE" == "monitor" ]]; then
    echo
    info "Flashing to ${PORT}…"
    if pio run --target upload; then
        BUILD_SECS=$(( SECONDS - START_TIME ))
        echo
        success "Flash complete in ${BUILD_SECS}s  —  firmware v${VERSION} is running"
    else
        die "Flash failed — is the device in bootloader mode? Try holding BOOT while pressing RESET."
    fi
fi

# ── Monitor ───────────────────────────────────────────────────────────────────
if [[ "$MODE" == "monitor" ]]; then
    echo
    info "Opening serial monitor on ${PORT} (Ctrl+C to exit)…"
    pio device monitor
fi
