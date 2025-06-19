#!/bin/sh
# Grant full read/write/execute permissions on the project directory
# Usage: ./set_permissions.sh [path]

TARGET="${1:-$(pwd)}"

echo "Setting permissions on $TARGET"
sudo chmod -R a+rwx "$TARGET"
