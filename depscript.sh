#!/usr/bin/env sh
# Backward-compatible dependency installer entrypoint.
# Prefer scripts/install-linux-deps.sh for documented options.

set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$SCRIPT_DIR/scripts/install-linux-deps.sh" "$@"
