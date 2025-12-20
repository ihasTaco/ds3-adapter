#!/usr/bin/env python3
"""
RosettaPad Debug Relay - Pi B Server
DS3 Controller <-> TCP Bridge

Architecture:
  PS3 <--BT--> Pi A <--TCP--> Pi B <--BT--> DS3

This script:
  1. Accepts DS3 Bluetooth connection (L2CAP PSM 0x11, 0x13)
  2. Accepts TCP connection from Pi A
  3. Relays traffic bidirectionally
  4. Logs ALL traffic in format: timestamp direction hex_bytes

Usage:
  ./main.py [--port 5555]

Output format:
  <timestamp> <direction> <channel> <hex bytes>
  1734621234.567 PS3 CTRL 43 F2
  1734621234.572 DS3 CTRL F2 FF FF 00 34 C7...
  1734621234.600 DS3 INTR A1 01 00 00...
"""

import socket
import select
import threading
import argparse
import time
import sys

try:
    import bluetooth
except ImportError:
    print("ERROR: pybluez not installed. Run: pip3 install pybluez")
    sys.exit(1)

# L2CAP PSMs for HID
PSM_HID_CONTROL = 0x11
PSM_HID_INTERRUPT = 0x13

# Global state
running = True
ds3_control_sock = None
ds3_interrupt_sock = None
tcp_sock = None


def log_packet(direction: str, channel: str, data: bytes):
    """Log packet in format: timestamp direction channel hex_bytes"""
    timestamp = time.time()
    hex_str = ' '.join(f'{b:02X}' for b in data)
    print(f"{timestamp:.3f} {direction} {channel} {hex_str}", flush=True)


def setup_l2cap_server(psm: int) -> socket.socket:
    """Create L2CAP server socket for given PSM"""
    sock = bluetooth.BluetoothSocket(bluetooth.L2CAP)
    sock.setblocking(True)
    sock.bind(("", psm))
    sock.listen(1)
    print(f"[Server] Listening on L2CAP PSM 0x{psm:04X}")
    return sock


def accept_ds3_connection(control_server: socket.socket,
                          interrupt_server: socket.socket):
    """Wait for DS3 to connect on both channels"""
    global ds3_control_sock, ds3_interrupt_sock

    print("[Server] Waiting for DS3 connection...")
    print("[Server] Press PS button on DS3 controller")

    # Accept control channel first
    ds3_control_sock, addr = control_server.accept()
    print(f"[Server] DS3 connected on control channel: {addr[0]}")

    # Then interrupt channel
    ds3_interrupt_sock, addr = interrupt_server.accept()
    print(f"[Server] DS3 connected on interrupt channel: {addr[0]}")

    # Set non-blocking for select()
    ds3_control_sock.setblocking(False)
    ds3_interrupt_sock.setblocking(False)

    print("[Server] DS3 fully connected!")


def setup_tcp_server(port: int) -> socket.socket:
    """Create TCP server for Pi A connection"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    sock.bind(('0.0.0.0', port))
    sock.listen(1)
    print(f"[Server] Listening on TCP port {port}")
    return sock


def accept_tcp_connection(server: socket.socket):
    """Wait for Pi A to connect"""
    global tcp_sock

    print("[Server] Waiting for Pi A connection...")
    tcp_sock, addr = server.accept()
    tcp_sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    tcp_sock.setblocking(False)
    print(f"[Server] Pi A connected from {addr[0]}:{addr[1]}")


def recv_tcp_packet(sock) -> tuple:
    """
    Receive packet from Pi A (non-blocking, returns immediately if no data)
    Protocol: [channel:1][length:2][payload:N]
    Returns: (channel, data) or (None, None) if no complete packet
    """
    try:
        # Try to read header
        header = b''
        while len(header) < 3:
            try:
                chunk = sock.recv(3 - len(header))
                if not chunk:
                    return -1, None  # Connection closed
                header += chunk
            except BlockingIOError:
                if len(header) == 0:
                    return None, None  # No data available
                # Partial header - wait for more
                time.sleep(0.001)
                continue

        channel = header[0]
        length = (header[1] << 8) | header[2]

        if length == 0:
            return channel, b''

        # Read payload
        data = b''
        while len(data) < length:
            try:
                chunk = sock.recv(length - len(data))
                if not chunk:
                    return -1, None
                data += chunk
            except BlockingIOError:
                time.sleep(0.001)
                continue

        return channel, data

    except Exception as e:
        print(f"[Server] TCP recv error: {e}")
        return -1, None


def send_tcp_packet(sock, channel: int, data: bytes) -> bool:
    """
    Send packet to Pi A
    Protocol: [channel:1][length:2][payload:N]
    """
    try:
        header = bytes([channel, (len(data) >> 8) & 0xFF, len(data) & 0xFF])
        sock.sendall(header + data)
        return True
    except Exception as e:
        print(f"[Server] TCP send error: {e}")
        return False


def relay_loop():
    """
    Main relay loop using select() for efficient multiplexing
    
    Handles:
      - PS3 -> DS3: TCP(from Pi A) -> BT(to DS3)
      - DS3 -> PS3: BT(from DS3) -> TCP(to Pi A)
    """
    global running, tcp_sock, ds3_control_sock, ds3_interrupt_sock

    print("[Relay] Starting relay loop...")

    while running:
        # Build list of sockets to monitor
        read_sockets = [tcp_sock, ds3_control_sock, ds3_interrupt_sock]

        try:
            readable, _, exceptional = select.select(read_sockets, [], read_sockets, 0.1)
        except (ValueError, OSError) as e:
            print(f"[Relay] Select error: {e}")
            break

        # Handle errors
        for sock in exceptional:
            print(f"[Relay] Socket error")
            running = False
            break

        for sock in readable:
            # === TCP from Pi A (PS3's messages) ===
            if sock == tcp_sock:
                channel, data = recv_tcp_packet(tcp_sock)

                if channel == -1:
                    print("[Relay] Pi A disconnected")
                    running = False
                    break

                if channel is None or data is None:
                    continue

                # Log PS3's message
                chan_name = "CTRL" if channel == PSM_HID_CONTROL else "INTR"
                log_packet("PS3", chan_name, data)

                # Forward to DS3
                try:
                    if channel == PSM_HID_CONTROL:
                        ds3_control_sock.send(data)
                    elif channel == PSM_HID_INTERRUPT:
                        ds3_interrupt_sock.send(data)
                except Exception as e:
                    print(f"[Relay] DS3 send error: {e}")
                    running = False
                    break

            # === DS3 Control Channel ===
            elif sock == ds3_control_sock:
                try:
                    data = ds3_control_sock.recv(256)
                    if not data:
                        print("[Relay] DS3 control disconnected")
                        running = False
                        break

                    # Log DS3's message
                    log_packet("DS3", "CTRL", data)

                    # Forward to Pi A
                    if not send_tcp_packet(tcp_sock, PSM_HID_CONTROL, data):
                        running = False
                        break

                except BlockingIOError:
                    pass
                except Exception as e:
                    print(f"[Relay] DS3 control recv error: {e}")
                    running = False
                    break

            # === DS3 Interrupt Channel ===
            elif sock == ds3_interrupt_sock:
                try:
                    data = ds3_interrupt_sock.recv(256)
                    if not data:
                        print("[Relay] DS3 interrupt disconnected")
                        running = False
                        break

                    # Log DS3's message
                    log_packet("DS3", "INTR", data)

                    # Forward to Pi A
                    if not send_tcp_packet(tcp_sock, PSM_HID_INTERRUPT, data):
                        running = False
                        break

                except BlockingIOError:
                    pass
                except Exception as e:
                    print(f"[Relay] DS3 interrupt recv error: {e}")
                    running = False
                    break

    print("[Relay] Loop ended")


def main():
    global running

    parser = argparse.ArgumentParser(description='DS3 Relay Server (Pi B)')
    parser.add_argument('-p', '--port', type=int, default=5555,
                        help='TCP port for Pi A connection (default: 5555)')
    args = parser.parse_args()

    print()
    print("=" * 60)
    print("  RosettaPad Debug Relay - Pi B Server")
    print("  DS3 Controller <-> TCP Bridge")
    print("=" * 60)
    print()
    print("  Data Flow:")
    print("    PS3 <--BT--> Pi A <--TCP--> Pi B <--BT--> DS3")
    print()
    print("  Log Format:")
    print("    <timestamp> <direction> <channel> <hex bytes>")
    print("    PS3 = from PlayStation 3")
    print("    DS3 = from DualShock 3")
    print("    CTRL = control channel (0x11)")
    print("    INTR = interrupt channel (0x13)")
    print()

    control_server = None
    interrupt_server = None
    tcp_server = None

    try:
        # Setup L2CAP servers for DS3
        control_server = setup_l2cap_server(PSM_HID_CONTROL)
        interrupt_server = setup_l2cap_server(PSM_HID_INTERRUPT)

        # Setup TCP server for Pi A
        tcp_server = setup_tcp_server(args.port)

        print()
        print("-" * 60)
        print("  Step 1: Press PS button on DS3 to connect")
        print("-" * 60)
        print()

        # Wait for DS3
        accept_ds3_connection(control_server, interrupt_server)

        print()
        print("-" * 60)
        print("  Step 2: Start relay on Pi A:")
        print(f"    ./debug-relay --relay <this-pi-ip> --port {args.port}")
        print("-" * 60)
        print()

        # Wait for Pi A
        accept_tcp_connection(tcp_server)

        print()
        print("=" * 60)
        print("  Relay Active!")
        print()
        print("  Logging all traffic. Press Ctrl+C to stop.")
        print("=" * 60)
        print()

        # Run the relay
        relay_loop()

    except KeyboardInterrupt:
        print("\n[Server] Shutting down...")

    except bluetooth.btcommon.BluetoothError as e:
        print(f"[Server] Bluetooth error: {e}")
        print("[Server] Make sure Bluetooth is enabled and you have root permissions")

    except Exception as e:
        print(f"[Server] Error: {e}")
        import traceback
        traceback.print_exc()

    finally:
        running = False

        # Close all sockets
        if ds3_control_sock:
            try:
                ds3_control_sock.close()
            except:
                pass
        if ds3_interrupt_sock:
            try:
                ds3_interrupt_sock.close()
            except:
                pass
        if tcp_sock:
            try:
                tcp_sock.close()
            except:
                pass
        if control_server:
            try:
                control_server.close()
            except:
                pass
        if interrupt_server:
            try:
                interrupt_server.close()
            except:
                pass
        if tcp_server:
            try:
                tcp_server.close()
            except:
                pass

        print("[Server] Done")


if __name__ == '__main__':
    main()