/*
 * RosettaPad - Common utilities and global state
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

// File descriptors
int g_ep0_fd = -1;
int g_ep1_fd = -1;
int g_ep2_fd = -1;
int g_hidraw_fd = -1;

// DS3 input report - 49 bytes, initialized to neutral state
uint8_t g_ds3_report[DS3_REPORT_SIZE] = {
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80,  // Report ID, reserved, buttons, sticks centered
    0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // More sticks, d-pad pressure
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Trigger/button pressure
    0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xee, 0x12,  // Reserved, battery (USB powered)
    0x00, 0x00, 0x00, 0x00, 0x12, 0x04, 0x77, 0x01,  // Status bytes
    0x80, 0x01, 0xda, 0x01, 0xda, 0x01, 0x01, 0x02,  // Motion: accel X, Y, Z, gyro Z (rest values)
    0x02                                             // Final byte
};
pthread_mutex_t g_report_mutex = PTHREAD_MUTEX_INITIALIZER;

// Rumble state
uint8_t g_rumble_right = 0;
uint8_t g_rumble_left = 0;
pthread_mutex_t g_rumble_mutex = PTHREAD_MUTEX_INITIALIZER;

// Lightbar state (default: blue)
lightbar_state_t g_lightbar_state = {0, 0, 255, 0, 255};
pthread_mutex_t g_lightbar_mutex = PTHREAD_MUTEX_INITIALIZER;

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