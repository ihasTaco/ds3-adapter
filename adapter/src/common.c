/*
 * RosettaPad Debug Relay - Common utilities and global state
 */

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include "common.h"

// =================================================================
// Global State Definitions
// =================================================================
volatile int g_running = 1;
volatile int g_usb_enabled = 0;
volatile int g_mode_switching = 0;
volatile int g_pairing_complete = 0;

// File descriptors
int g_ep0_fd = -1;
int g_ep1_fd = -1;
int g_ep2_fd = -1;

// DS3 report - will be populated by relay from Pi B
uint8_t g_ds3_report[DS3_REPORT_SIZE] = {0};
pthread_mutex_t g_report_mutex = PTHREAD_MUTEX_INITIALIZER;

// =================================================================
// Debug Utilities
// =================================================================
void print_hex(const char* label, const uint8_t* data, size_t len) {
    printf("%s (%zu bytes):\n  ", label, len);
    for (size_t i = 0; i < len && i < 64; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < len) printf("\n  ");
    }
    printf("\n");
    fflush(stdout);
}