#!/usr/bin/env sh
# Install build dependencies for 5G-CASH on common Linux distributions.
# Defaults to a headless daemon/test build. Pass --gui to include Qt packages,
# --windows to include Windows cross-build packages, or --full for both.
# This script intentionally avoids forcing the legacy bitcoin/bitcoin BDB 4.8 PPA;
# modern Ubuntu/Debian builds should use system Berkeley DB with
# ./configure --with-incompatible-bdb, or use ./scripts/build-linux.sh --depends
# for a repository-managed dependency prefix.

set -eu

WITH_GUI=0
WITH_WINDOWS=0
DRY_RUN=0
USE_SUDO=1
USE_SYSTEM_APT_SOURCES=0

usage() {
  cat <<'USAGE'
Usage: scripts/install-linux-deps.sh [options]

Options:
  --gui          Install Qt/GUI build dependencies as well as daemon deps.
  --no-gui       Do not install Qt/GUI build dependencies (default).
  --windows      Install Windows cross-build dependencies (mingw-w64, NSIS, zip).
  --no-windows   Do not install Windows cross-build dependencies (default).
  --full         Install headless, Qt/GUI, and Windows cross-build dependencies.
  --dry-run      Print commands without executing them.
  --no-sudo      Do not prefix package-manager commands with sudo.
  --system-apt   Use the machine's configured apt sources instead of an
                 isolated Ubuntu/Debian source list.
  -h, --help     Show this help.

Supported package managers: apt-get, dnf, yum, pacman, zypper, apk.
USAGE
}

while [ $# -gt 0 ]; do
  case "$1" in
    --gui) WITH_GUI=1 ;;
    --no-gui) WITH_GUI=0 ;;
    --windows) WITH_WINDOWS=1 ;;
    --no-windows) WITH_WINDOWS=0 ;;
    --full) WITH_GUI=1; WITH_WINDOWS=1 ;;
    --dry-run) DRY_RUN=1 ;;
    --no-sudo) USE_SUDO=0 ;;
    --system-apt) USE_SYSTEM_APT_SOURCES=1 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

if [ "$(id -u)" -eq 0 ]; then
  USE_SUDO=0
fi

SUDO=""
if [ "$USE_SUDO" -eq 1 ]; then
  if command -v sudo >/dev/null 2>&1; then
    SUDO=sudo
  else
    echo "sudo is required but was not found. Re-run as root or pass --no-sudo." >&2
    exit 1
  fi
fi

run() {
  printf '+ %s\n' "$*"
  if [ "$DRY_RUN" -eq 0 ]; then
    "$@"
  fi
}

fix_exec_bits() {
  for script in \
    ./autogen.sh \
    ./contrib/install_db4.sh \
    ./build_windows_fiveg.sh \
    ./src/secp256k1/autogen.sh \
    ./src/tor/autogen.sh \
    ./src/univalue/autogen.sh \
    ./share/genbuild.sh
  do
    if [ -f "$script" ] && [ ! -x "$script" ]; then
      run chmod +x "$script"
    elif [ -f "$script" ]; then
      printf '# executable already set: %s\n' "$script"
    fi
  done
}

if command -v apt-get >/dev/null 2>&1; then
  BASE_PKGS="build-essential libtool autotools-dev automake autoconf pkg-config bsdmainutils python3 git ca-certificates curl cmake zip unzip"
  LIB_PKGS="libssl-dev libevent-dev libboost-all-dev libzmq3-dev libminizip-dev libminiupnpc-dev libprotobuf-dev protobuf-compiler libqrencode-dev libleveldb-dev libdb-dev libdb++-dev"
  GUI_PKGS="qtbase5-dev qttools5-dev qttools5-dev-tools libqt5gui5 libqt5core5a libqt5dbus5"
  WINDOWS_PKGS="mingw-w64 g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64 nsis"

  APT_OPTS=""
  APT_TMP=""
  if [ "$USE_SYSTEM_APT_SOURCES" -eq 0 ] && [ -r /etc/os-release ]; then
    # CI/dev containers often contain third-party apt lists that break update.
    # Build from a minimal official source list unless explicitly disabled.
    # shellcheck disable=SC1091
    . /etc/os-release
    CODENAME="${VERSION_CODENAME:-${UBUNTU_CODENAME:-}}"
    if [ -n "$CODENAME" ]; then
      APT_TMP=$(mktemp -d)
      trap 'if [ -n "$APT_TMP" ]; then rm -rf "$APT_TMP"; fi' EXIT HUP INT TERM
      mkdir -p "$APT_TMP/empty"
      if [ "${ID:-}" = "ubuntu" ] || printf '%s' "${ID_LIKE:-}" | grep -q debian; then
        UBUNTU_MIRROR="${UBUNTU_MIRROR:-http://archive.ubuntu.com/ubuntu}"
        UBUNTU_SECURITY_MIRROR="${UBUNTU_SECURITY_MIRROR:-http://security.ubuntu.com/ubuntu}"
        cat > "$APT_TMP/sources.list" <<EOF_APT
deb $UBUNTU_MIRROR $CODENAME main universe restricted multiverse
deb $UBUNTU_MIRROR $CODENAME-updates main universe restricted multiverse
deb $UBUNTU_MIRROR $CODENAME-backports main universe restricted multiverse
deb $UBUNTU_SECURITY_MIRROR $CODENAME-security main universe restricted multiverse
EOF_APT
        APT_OPTS="-o Dir::Etc::sourcelist=$APT_TMP/sources.list -o Dir::Etc::sourceparts=$APT_TMP/empty -o APT::Get::List-Cleanup=0"
      elif [ "${ID:-}" = "debian" ]; then
        DEBIAN_MIRROR="${DEBIAN_MIRROR:-http://deb.debian.org/debian}"
        DEBIAN_SECURITY_MIRROR="${DEBIAN_SECURITY_MIRROR:-http://security.debian.org/debian-security}"
        cat > "$APT_TMP/sources.list" <<EOF_APT
deb $DEBIAN_MIRROR $CODENAME main contrib non-free non-free-firmware
deb $DEBIAN_MIRROR $CODENAME-updates main contrib non-free non-free-firmware
deb $DEBIAN_SECURITY_MIRROR $CODENAME-security main contrib non-free non-free-firmware
EOF_APT
        APT_OPTS="-o Dir::Etc::sourcelist=$APT_TMP/sources.list -o Dir::Etc::sourceparts=$APT_TMP/empty -o APT::Get::List-Cleanup=0"
      fi
    fi
  fi

  PKGS="$BASE_PKGS $LIB_PKGS"
  if [ "$WITH_GUI" -eq 1 ]; then
    PKGS="$PKGS $GUI_PKGS"
  fi
  if [ "$WITH_WINDOWS" -eq 1 ]; then
    PKGS="$PKGS $WINDOWS_PKGS"
  fi
  # shellcheck disable=SC2086
  run $SUDO apt-get $APT_OPTS update
  # shellcheck disable=SC2086
  run $SUDO apt-get $APT_OPTS install -y $PKGS
elif command -v dnf >/dev/null 2>&1; then
  BASE_PKGS="gcc gcc-c++ make libtool automake autoconf pkgconf-pkg-config python3 git ca-certificates curl cmake zip unzip"
  LIB_PKGS="openssl-devel libevent-devel boost-devel zeromq-devel minizip-devel miniupnpc-devel protobuf-devel protobuf-compiler leveldb-devel libdb-devel qrencode-devel"
  GUI_PKGS="qt5-qtbase-devel qt5-linguist"
  WINDOWS_PKGS="mingw64-gcc-c++ mingw64-binutils nsis"
  PKGS="$BASE_PKGS $LIB_PKGS"
  if [ "$WITH_GUI" -eq 1 ]; then
    PKGS="$PKGS $GUI_PKGS"
  fi
  if [ "$WITH_WINDOWS" -eq 1 ]; then
    PKGS="$PKGS $WINDOWS_PKGS"
  fi
  run $SUDO dnf install -y $PKGS
elif command -v yum >/dev/null 2>&1; then
  BASE_PKGS="gcc gcc-c++ make libtool automake autoconf pkgconfig python3 git ca-certificates curl cmake zip unzip"
  LIB_PKGS="openssl-devel libevent-devel boost-devel zeromq-devel minizip-devel miniupnpc-devel protobuf-devel protobuf-compiler leveldb-devel libdb-devel qrencode-devel"
  GUI_PKGS="qt5-qtbase-devel qt5-linguist"
  WINDOWS_PKGS="mingw64-gcc-c++ mingw64-binutils nsis"
  PKGS="$BASE_PKGS $LIB_PKGS"
  if [ "$WITH_GUI" -eq 1 ]; then
    PKGS="$PKGS $GUI_PKGS"
  fi
  if [ "$WITH_WINDOWS" -eq 1 ]; then
    PKGS="$PKGS $WINDOWS_PKGS"
  fi
  run $SUDO yum install -y $PKGS
elif command -v pacman >/dev/null 2>&1; then
  BASE_PKGS="base-devel libtool automake autoconf pkgconf python git ca-certificates curl cmake zip unzip"
  LIB_PKGS="openssl libevent boost zeromq minizip miniupnpc protobuf leveldb db qrencode"
  GUI_PKGS="qt5-base qt5-tools"
  WINDOWS_PKGS="mingw-w64-gcc nsis"
  PKGS="$BASE_PKGS $LIB_PKGS"
  if [ "$WITH_GUI" -eq 1 ]; then
    PKGS="$PKGS $GUI_PKGS"
  fi
  if [ "$WITH_WINDOWS" -eq 1 ]; then
    PKGS="$PKGS $WINDOWS_PKGS"
  fi
  run $SUDO pacman -Sy --needed --noconfirm $PKGS
elif command -v zypper >/dev/null 2>&1; then
  BASE_PKGS="gcc gcc-c++ make libtool automake autoconf pkg-config python3 git ca-certificates curl cmake zip unzip"
  LIB_PKGS="libopenssl-devel libevent-devel boost-devel zeromq-devel minizip-devel libminiupnpc-devel protobuf-devel leveldb-devel libdb-4_8-devel qrencode-devel"
  GUI_PKGS="libqt5-qtbase-devel libqt5-linguist-devel"
  WINDOWS_PKGS="mingw64-gcc-c++ mingw64-binutils nsis"
  PKGS="$BASE_PKGS $LIB_PKGS"
  if [ "$WITH_GUI" -eq 1 ]; then
    PKGS="$PKGS $GUI_PKGS"
  fi
  if [ "$WITH_WINDOWS" -eq 1 ]; then
    PKGS="$PKGS $WINDOWS_PKGS"
  fi
  run $SUDO zypper --non-interactive install $PKGS
elif command -v apk >/dev/null 2>&1; then
  BASE_PKGS="build-base libtool automake autoconf pkgconf python3 git ca-certificates curl cmake zip unzip"
  LIB_PKGS="openssl-dev libevent-dev boost-dev zeromq-dev minizip-dev miniupnpc-dev protobuf-dev leveldb-dev db-dev qrencode-dev"
  GUI_PKGS="qt5-qtbase-dev qt5-qttools-dev"
  WINDOWS_PKGS="mingw-w64-gcc nsis"
  PKGS="$BASE_PKGS $LIB_PKGS"
  if [ "$WITH_GUI" -eq 1 ]; then
    PKGS="$PKGS $GUI_PKGS"
  fi
  if [ "$WITH_WINDOWS" -eq 1 ]; then
    PKGS="$PKGS $WINDOWS_PKGS"
  fi
  run $SUDO apk add --no-cache $PKGS
else
  echo "No supported package manager found. Install the packages listed in doc/build-unix.md or use depends/." >&2
  exit 1
fi

fix_exec_bits

cat <<'NOTE'

Dependency installation completed and repository helper scripts are executable.
For the broadest Linux compatibility, build with:
  ./scripts/build-linux.sh --no-gui

If your system Berkeley DB is newer than 4.8, the build script passes
--with-incompatible-bdb automatically. For wallet database compatibility with
older releases, use:
  ./scripts/build-linux.sh --depends --no-gui

For a one-shot full Ubuntu/Debian environment setup matching the historical
manual dependency list, run:
  ./scripts/setup-build-env.sh
NOTE
