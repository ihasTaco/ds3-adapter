/*
 * RosettaPad Debug Relay - Common definitions and shared state
 */

#ifndef ROSETTAPAD_COMMON_H
#define ROSETTAPAD_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#include "debug.h"

// =================================================================
// Global State
// =================================================================
extern volatile int g_running;
extern volatile int g_usb_enabled;
extern volatile int g_mode_switching;
extern volatile int g_pairing_complete;

// File descriptors
extern int g_ep0_fd;
extern int g_ep1_fd;
extern int g_ep2_fd;

// =================================================================
// DS3 Report (for relay)
// =================================================================
#define DS3_REPORT_SIZE 49

extern uint8_t g_ds3_report[DS3_REPORT_SIZE];
extern pthread_mutex_t g_report_mutex;

// =================================================================
// Debug Utilities
// =================================================================
void print_hex(const char* label, const uint8_t* data, size_t len);

#endif // ROSETTAPAD_COMMON_H