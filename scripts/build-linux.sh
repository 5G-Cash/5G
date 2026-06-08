#!/usr/bin/env sh
# Configure, compile, and optionally test 5G-CASH on Linux.

set -eu

WITH_GUI=0
USE_DEPENDS=0
RUN_TESTS=1
CLEAN=0
JOBS="${JOBS:-}"
CONFIGURE_EXTRA="${CONFIGURE_EXTRA:-}"

usage() {
  cat <<'USAGE'
Usage: scripts/build-linux.sh [options]

Options:
  --gui             Build the Qt GUI if Qt dependencies are installed.
  --no-gui          Build daemon/CLI only (default).
  --depends         Build and use the repository depends/ prefix first.
  --no-tests        Skip make check.
  --clean           Run make clean before compiling when Makefile exists.
  -j, --jobs N      Parallel make jobs. Defaults to nproc/getconf.
  -h, --help        Show this help.

Environment:
  CONFIGURE_EXTRA   Extra flags appended to ./configure.
  JOBS              Default parallel job count if --jobs is not supplied.
USAGE
}

while [ $# -gt 0 ]; do
  case "$1" in
    --gui) WITH_GUI=1 ;;
    --no-gui) WITH_GUI=0 ;;
    --depends) USE_DEPENDS=1 ;;
    --no-tests) RUN_TESTS=0 ;;
    --clean) CLEAN=1 ;;
    -j|--jobs)
      shift
      [ $# -gt 0 ] || { echo "--jobs requires a value" >&2; exit 2; }
      JOBS="$1"
      ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

require_tools() {
  missing=""
  for tool in "$@"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
      missing="$missing $tool"
    fi
  done
  if [ -n "$missing" ]; then
    echo "Missing required build tools:$missing" >&2
    echo "Install them with: ./scripts/install-linux-deps.sh --no-gui" >&2
    echo "In restricted containers, package installation may be blocked by network/proxy policy." >&2
    exit 127
  fi
}

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
cd "$REPO_ROOT"

require_tools make gcc g++ autoconf autoreconf automake aclocal libtoolize pkg-config

if [ -z "$JOBS" ]; then
  if command -v nproc >/dev/null 2>&1; then
    JOBS=$(nproc)
  else
    JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')
  fi
fi

HOST_TRIPLET="${HOST_TRIPLET:-}"
if [ -z "$HOST_TRIPLET" ]; then
  if command -v gcc >/dev/null 2>&1; then
    HOST_TRIPLET=$(gcc -dumpmachine)
  else
    HOST_TRIPLET=$(./depends/config.guess)
  fi
fi

CONFIG_SITE_ARG=""
PREFIX_ARG=""
if [ "$USE_DEPENDS" -eq 1 ]; then
  echo "Building depends for HOST=$HOST_TRIPLET with $JOBS jobs..."
  make -C depends HOST="$HOST_TRIPLET" -j"$JOBS"
  if [ -f "depends/$HOST_TRIPLET/share/config.site" ]; then
    CONFIG_SITE_ARG="CONFIG_SITE=$REPO_ROOT/depends/$HOST_TRIPLET/share/config.site"
  fi
  PREFIX_ARG="--prefix=$REPO_ROOT/depends/$HOST_TRIPLET"
fi

if [ ! -x ./autogen.sh ]; then
  chmod +x ./autogen.sh
fi
if [ -f ./src/secp256k1/autogen.sh ] && [ ! -x ./src/secp256k1/autogen.sh ]; then
  chmod +x ./src/secp256k1/autogen.sh
fi
if [ -f ./src/tor/autogen.sh ] && [ ! -x ./src/tor/autogen.sh ]; then
  chmod +x ./src/tor/autogen.sh
fi

./autogen.sh

GUI_ARG="--with-gui=no"
if [ "$WITH_GUI" -eq 1 ]; then
  GUI_ARG="--with-gui=qt5"
fi

# Ubuntu 18.04+ and most modern distributions ship Berkeley DB newer than 4.8.
# Preserve buildability by allowing those versions unless the depends prefix is used.
BDB_ARG="--with-incompatible-bdb"
if [ "$USE_DEPENDS" -eq 1 ]; then
  BDB_ARG=""
fi

# shellcheck disable=SC2086
sh -c "${CONFIG_SITE_ARG:+$CONFIG_SITE_ARG }./configure $GUI_ARG $BDB_ARG $PREFIX_ARG $CONFIGURE_EXTRA"

if [ "$CLEAN" -eq 1 ] && [ -f Makefile ]; then
  make clean
fi

make -j"$JOBS"

if [ "$RUN_TESTS" -eq 1 ]; then
  make check
fi
