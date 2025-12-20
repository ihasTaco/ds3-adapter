/*
 * RosettaPad Debug Relay
 * 
 * Captures PS3 <-> DS3 Bluetooth protocol by relaying between:
 *   - Pi A (this): Connected to PS3 via Bluetooth
 *   - Pi B: Connected to real DS3 via Bluetooth
 * 
 * Usage:
 *   ./debug-bt-relay --relay <Pi-B-IP> [--port 5555]
 * 
 * Output format:
 *   <timestamp> <direction> <hex bytes>
 *   1734621234.567 PS3 43 F2
 *   1734621234.572 DS3 F2 FF FF 00 34 C7...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>

#include "common.h"
#include "debug.h"
#include "ds3.h"
#include "usb_gadget.h"
#include "bt_hid.h"

// =================================================================
// Signal Handler
// =================================================================

static void signal_handler(int sig) {
    (void)sig;
    printf("\n[Main] Shutdown requested...\n");
    g_running = 0;
}

// =================================================================
// Banner
// =================================================================

static void print_banner(void) {
    printf("\n");
    printf("==============================================================\n");
    printf("  RosettaPad Debug Relay\n");
    printf("  PS3 <-> DS3 Bluetooth Protocol Capture\n");
    printf("==============================================================\n");
    printf("\n");
}

// =================================================================
// Help
// =================================================================

static void print_help(const char* prog) {
    printf("Usage: %s [options]\n\n", prog);
    printf("Modes:\n");
    printf("  --usb                  USB pairing mode (get PS3 MAC)\n");
    printf("  --relay <IP>           Relay mode (connect to Pi B)\n");
    printf("\n");
    printf("Options:\n");
    printf("  -p, --port <port>      Pi B port (default: 5555)\n");
    printf("  -d, --debug <cats>     Debug categories\n");
    printf("  -h, --help             Show this help\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --usb                         # Pair with PS3 via USB\n", prog);
    printf("  %s --relay 192.168.1.100         # Relay to Pi B\n", prog);
    printf("  %s --relay 192.168.1.100 -p 5555 # With custom port\n", prog);
    printf("\n");
    printf("Output format:\n");
    printf("  <timestamp> <direction> <hex bytes>\n");
    printf("  1734621234.567 PS3 43 F2\n");
    printf("  1734621234.572 DS3 F2 FF FF 00 34 C7...\n");
    printf("\n");
}

// =================================================================
// USB Pairing Mode
// =================================================================

static int run_usb_mode(void) {
    pthread_t usb_ctrl_tid;
    pthread_t usb_in_tid;
    pthread_t usb_out_tid;

    debug_print(DBG_INFO, "[Main] Starting USB pairing mode");

    if (usb_gadget_init() < 0) {
        debug_print(DBG_ERROR, "[Main] Failed to init USB gadget");
        return 1;
    }

    g_ep0_fd = usb_open_endpoint(0);
    if (g_ep0_fd < 0) {
        debug_print(DBG_ERROR, "[Main] Failed to open ep0");
        return 1;
    }

    if (usb_gadget_write_descriptors(g_ep0_fd) < 0) {
        debug_print(DBG_ERROR, "[Main] Failed to write descriptors");
        close(g_ep0_fd);
        return 1;
    }

    pthread_create(&usb_ctrl_tid, NULL, usb_control_thread, NULL);
    pthread_create(&usb_in_tid, NULL, usb_input_thread, NULL);
    pthread_create(&usb_out_tid, NULL, usb_output_thread, NULL);

    if (usb_gadget_bind() < 0) {
        debug_print(DBG_WARN, "[Main] Failed to bind UDC");
    }

    printf("\n");
    printf("--------------------------------------------------------------\n");
    printf("  USB Pairing Mode\n");
    printf("  Connect Pi to PS3 via USB cable.\n");
    printf("  PS3 will send its Bluetooth MAC.\n");
    printf("  Press Ctrl+C when done.\n");
    printf("--------------------------------------------------------------\n");
    printf("\n");
    fflush(stdout);

    while (g_running && !g_pairing_complete) {
        sleep(1);
    }

    if (g_pairing_complete) {
        printf("\n");
        printf("--------------------------------------------------------------\n");
        printf("  Pairing complete!\n");
        printf("  PS3 MAC saved. You can now run relay mode.\n");
        printf("--------------------------------------------------------------\n");
        printf("\n");
    }

    usb_gadget_unbind();

    if (g_ep1_fd >= 0) { close(g_ep1_fd); g_ep1_fd = -1; }
    if (g_ep2_fd >= 0) { close(g_ep2_fd); g_ep2_fd = -1; }
    if (g_ep0_fd >= 0) { close(g_ep0_fd); g_ep0_fd = -1; }

    return 0;
}

// =================================================================
// Relay Mode
// =================================================================

static int run_relay_mode(const char* relay_host, int relay_port) {
    pthread_t bt_out_tid;
    pthread_t bt_in_tid;

    debug_print(DBG_INFO, "[Main] Starting relay mode");
    debug_print(DBG_INFO, "[Main] Pi B: %s:%d", relay_host, relay_port);

    // Initialize Bluetooth
    if (bt_hid_init() < 0) {
        debug_print(DBG_ERROR, "[Main] Failed to init Bluetooth");
        return 1;
    }

    // Check pairing
    if (!bt_hid_is_paired()) {
        printf("\n");
        printf("--------------------------------------------------------------\n");
        printf("  No PS3 pairing found!\n");
        printf("  Run with --usb first to pair with PS3.\n");
        printf("--------------------------------------------------------------\n");
        printf("\n");
        bt_hid_cleanup();
        return 1;
    }

    // Connect to Pi B
    strncpy(g_relay_host, relay_host, sizeof(g_relay_host) - 1);
    g_relay_port = relay_port;

    printf("[Main] Waiting for Pi B at %s:%d...\n", relay_host, relay_port);
    while (g_running) {
        if (relay_connect(relay_host, relay_port) == 0) {
            break;
        }
        printf("[Main] Pi B not ready, retrying in 2 seconds...\n");
        sleep(2);
    }
    
    if (!g_running) {
        bt_hid_cleanup();
        return 1;
    }

    // Connect to PS3
    debug_print(DBG_INFO, "[Main] Connecting to PS3...");

    if (bt_hid_connect() < 0) {
        printf("\n");
        printf("--------------------------------------------------------------\n");
        printf("  Failed to connect to PS3!\n");
        printf("  Make sure PS3 is on and paired.\n");
        printf("--------------------------------------------------------------\n");
        printf("\n");
        relay_disconnect();
        bt_hid_cleanup();
        return 1;
    }

    // Start threads
    pthread_create(&bt_out_tid, NULL, bt_hid_output_thread, NULL);
    pthread_create(&bt_in_tid, NULL, bt_hid_input_thread, NULL);

    printf("\n");
    printf("==============================================================\n");
    printf("  Relay Active!\n");
    printf("  PS3 <--BT--> Pi A <--TCP--> Pi B <--BT--> DS3\n");
    printf("\n");
    printf("  Logging all traffic. Press Ctrl+C to stop.\n");
    printf("==============================================================\n");
    printf("\n");
    fflush(stdout);

    // Wait for shutdown
    while (g_running) {
        sleep(1);

        if (!bt_hid_is_connected() && g_running) {
            debug_print(DBG_WARN, "[Main] PS3 disconnected, reconnecting...");
            if (bt_hid_connect() == 0) {
                debug_print(DBG_INFO, "[Main] Reconnected to PS3");
            }
            sleep(2);
        }
    }

    // Cleanup
    debug_print(DBG_INFO, "[Main] Shutting down...");

    bt_hid_disconnect();
    relay_disconnect();
    bt_hid_cleanup();

    return 0;
}

// =================================================================
// Main
// =================================================================

int main(int argc, char* argv[]) {
    int usb_mode = 0;
    char* relay_host = NULL;
    int relay_port = 5555;

    // Default debug flags
    debug_set_flags(DBG_ERROR | DBG_WARN | DBG_INFO | DBG_INIT);

    static struct option long_options[] = {
        {"usb",     no_argument,       0, 'u'},
        {"relay",   required_argument, 0, 'r'},
        {"port",    required_argument, 0, 'p'},
        {"debug",   required_argument, 0, 'd'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "ur:p:d:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'u':
                usb_mode = 1;
                break;
            case 'r':
                relay_host = optarg;
                break;
            case 'p':
                relay_port = atoi(optarg);
                break;
            case 'd':
                debug_set_flags(debug_parse_flags(optarg));
                break;
            case 'h':
                print_help(argv[0]);
                return 0;
            default:
                print_help(argv[0]);
                return 1;
        }
    }

    // Validate args
    if (!usb_mode && !relay_host) {
        print_help(argv[0]);
        printf("Error: Must specify --usb or --relay <IP>\n\n");
        return 1;
    }

    if (usb_mode && relay_host) {
        print_help(argv[0]);
        printf("Error: Cannot use --usb and --relay together\n\n");
        return 1;
    }

    print_banner();

    // Setup signals
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize
    debug_init();
    ds3_init();

    // Update F2 report with our BT MAC
    if (bt_hid_init() == 0) {
        uint8_t local_mac[6];
        if (bt_hid_get_local_mac(local_mac) == 0) {
            ds3_set_local_bt_mac(local_mac);
        }
        if (usb_mode) {
            bt_hid_cleanup();
        }
    }

    // Run selected mode
    int result;
    if (usb_mode) {
        result = run_usb_mode();
    } else {
        result = run_relay_mode(relay_host, relay_port);
    }

    debug_print(DBG_INFO, "[Main] Done");
    return result;
}