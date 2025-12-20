# RosettaPad Debug Relay

Captures and logs the complete PS3 <-> DS3 Bluetooth HID protocol by inserting
a relay between them.

## Architecture

```
PS3 <--Bluetooth--> Pi A <--TCP--> Pi B <--Bluetooth--> DS3
                    (emulator)     (server)    (real controller)
```

- **Pi A**: Connects to PS3 as a DS3 controller, relays all traffic to Pi B
- **Pi B**: Connects to real DS3 controller, relays all traffic to Pi A, **logs everything**

## Hardware Required

- 2x Raspberry Pi (Zero 2W, Pi 3, or Pi 4) with Bluetooth
- 1x Real DualShock 3 controller
- 1x PlayStation 3 console
- USB cable (for initial Pi A <-> PS3 pairing)

## Setup

### Pi B (DS3 Server)

```bash
cd pi_b

# Install dependencies
sudo apt-get update
sudo apt-get install -y python3-bluetooth bluez bluez-tools

# Pair DS3 with Pi B using sixpair
# Connect DS3 to Pi B via USB
sudo sixpair
# Unplug DS3

# Run the server
sudo python3 main.py --port 5555
```

### Pi A (PS3 Relay)

```bash
cd pi_a

# Install dependencies
sudo apt-get update
sudo apt-get install -y libbluetooth-dev bluez

# Build
make

# Step 1: USB pairing with PS3
# Connect Pi A to PS3 via USB cable
sudo ./debug-relay --usb
# Wait for "Pairing complete", then unplug USB

# Step 2: Run relay mode (after Pi B is running and DS3 connected)
sudo ./debug-relay --relay <Pi-B-IP> --port 5555
```

## Usage Sequence

1. **Pi B**: Run `sudo python3 main.py`
2. **DS3**: Press PS button to connect to Pi B
3. **Pi A**: Run `sudo ./debug-relay --relay <Pi-B-IP>`
4. **PS3**: Should see controller connect

All traffic is logged on Pi B in this format:
```
<timestamp> <direction> <channel> <hex bytes>
1734621234.567 PS3 CTRL 43 F2
1734621234.572 DS3 CTRL F2 FF FF 00 34 C7 31 25 AE 60...
1734621234.600 DS3 INTR A1 01 00 00 80 80 80 80...
```

Where:
- `PS3` = message originated from PlayStation 3
- `DS3` = message originated from DualShock 3
- `CTRL` = HID Control channel (PSM 0x11) - feature reports, handshake
- `INTR` = HID Interrupt channel (PSM 0x13) - input/output reports

## Troubleshooting

### "No PS3 pairing found"
Run USB pairing mode first:
```bash
sudo ./debug-relay --usb
```

### "Failed to connect to Pi B"
- Make sure Pi B server is running
- Check IP address and port
- Verify network connectivity: `ping <Pi-B-IP>`

### DS3 not connecting to Pi B
- Make sure DS3 was paired with Pi B using `sixpair`
- Try pressing the PS button again
- Check: `hciconfig hci0` should show UP RUNNING

### PS3 not connecting
- Make sure Pi A was paired via USB first
- PS3 must be powered ON (not standby)
- Try: `sudo rm /etc/rosettapad/pairing.conf` and re-pair via USB

## File Structure

```
pi_a/                   # Runs on Pi connected to PS3
├── Makefile
├── main.c              # Entry point
├── bt_hid.c            # Bluetooth HID + TCP relay
├── bt_hid.h
├── usb_gadget.c        # USB pairing mode
├── usb_gadget.h
├── ds3.c               # DS3 feature reports
├── ds3.h
├── common.c
├── common.h
├── debug.c
└── debug.h

pi_b/                   # Runs on Pi connected to DS3
├── main.py             # Python server
└── setup.sh            # DS3 pairing helper
```

## Debug Options (Pi A)

```bash
./debug-relay --relay <IP> --debug all        # Everything
./debug-relay --relay <IP> --debug bt,reports # BT + HID reports
./debug-relay --relay <IP> --debug handshake  # Protocol handshake
```