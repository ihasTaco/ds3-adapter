#!/bin/bash
#
# Setup script for Pi B - pairs DS3 controller
#

echo "=================================="
echo "  DS3 Pairing Setup"
echo "=================================="
echo

# Install dependencies
echo "[1/3] Installing dependencies..."
sudo apt-get update
sudo apt-get install -y python3-bluetooth bluez bluez-tools

# Install sixpair if not present
if ! command -v sixpair &> /dev/null; then
    echo "[2/3] Installing sixpair..."
    sudo apt-get install -y sixpair 2>/dev/null || {
        echo "  sixpair not in repos, building from source..."
        sudo apt-get install -y libusb-dev
        wget -q http://www.pabr.org/sixlinux/sixpair.c
        gcc -o sixpair sixpair.c -lusb
        sudo mv sixpair /usr/local/bin/
        rm -f sixpair.c
    }
else
    echo "[2/3] sixpair already installed"
fi

echo "[3/3] Ready to pair DS3"
echo
echo "=================================="
echo "  Connect DS3 via USB and run:"
echo "    sudo sixpair"
echo
echo "  Then unplug USB and run:"
echo "    ./ds3_relay_server.py"
echo "=================================="