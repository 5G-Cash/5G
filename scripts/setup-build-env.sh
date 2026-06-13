#!/usr/bin/env bash
# One-shot Ubuntu/Debian-oriented build environment setup for 5G-CASH.
# This intentionally mirrors the historical manual dependency list while
# delegating package-manager details to scripts/install-linux-deps.sh.

set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

cd "$REPO_ROOT"

usage() {
  cat <<'USAGE'
Usage: scripts/setup-build-env.sh [install-options]

One-shot full build environment setup. By default this installs the full
Ubuntu/Debian-style toolchain set: headless daemon dependencies, Qt GUI
dependencies, Windows cross-build tools, CMake, zip/unzip, and helper tools.

Common options are forwarded to scripts/install-linux-deps.sh:
  --dry-run      Print package-manager/chmod commands without executing them.
  --system-apt   Use the machine's configured apt sources.
  --no-sudo      Do not prefix package-manager commands with sudo.
  --no-gui       Override --full and skip Qt packages.
  --no-windows   Override --full and skip Windows cross-build packages.
  -h, --help     Show this help.
USAGE
}

for arg in "$@"; do
  case "$arg" in
    -h|--help) usage; exit 0 ;;
  esac
done

# Full setup = daemon deps + Qt GUI deps + Windows cross-build deps:
# build-essential libtool autotools-dev automake pkg-config bsdmainutils curl
# git ca-certificates python3 cmake mingw-w64 g++-mingw-w64-x86-64
# binutils-mingw-w64-x86-64 nsis zip unzip qtbase5-dev qttools5-dev
# qttools5-dev-tools libboost-all-dev libevent-dev libminiupnpc-dev libzmq3-dev
"$SCRIPT_DIR/install-linux-deps.sh" --full "$@"

DRY_RUN=0
for arg in "$@"; do
  if [[ "$arg" == "--dry-run" ]]; then
    DRY_RUN=1
  fi
done

for helper in \
  ./autogen.sh \
  ./contrib/install_db4.sh \
  ./build_windows_fiveg.sh \
  ./src/secp256k1/autogen.sh \
  ./src/tor/autogen.sh \
  ./src/univalue/autogen.sh \
  ./share/genbuild.sh
 do
  if [[ -f "$helper" && ! -x "$helper" ]]; then
    if [[ "$DRY_RUN" -eq 1 ]]; then
      printf '+ chmod +x %s\n' "$helper"
    else
      chmod +x "$helper" || true
    fi
  fi
done

cat <<'NEXT'

Environment setup complete.
Recommended Linux build:
  ./scripts/build-linux.sh --no-gui --no-tests --clean

Recommended Qt build:
  ./scripts/build-linux.sh --gui --no-tests --clean
NEXT
