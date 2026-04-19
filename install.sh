#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BINARY="$BUILD_DIR/crystal-dock"
INSTALL_PATH="/usr/bin/crystal-dock"

# Always rebuild
echo "Building crystal-dock..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake ../src -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
cd "$SCRIPT_DIR"

# Kill running instance
if pgrep -x crystal-dock >/dev/null 2>&1; then
    echo "Stopping running crystal-dock..."
    killall crystal-dock
    sleep 1
fi

# Install
echo "Installing to $INSTALL_PATH..."
sudo cp "$BINARY" "$INSTALL_PATH"

# Restart
echo "Starting crystal-dock..."
nohup crystal-dock >/dev/null 2>&1 &
disown

echo "Done."
