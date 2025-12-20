/*
 * RosettaPad Debug Relay - DS3 Pairing/Feature Reports
 */

#ifndef ROSETTAPAD_DS3_H
#define ROSETTAPAD_DS3_H

#include <stdint.h>
#include <stddef.h>

// =================================================================
// DS3 Feature Reports
// =================================================================
#define DS3_FEATURE_REPORT_SIZE 64
#define DS3_INPUT_REPORT_SIZE   49
#define DS3_BT_INPUT_REPORT_SIZE 50

// Report IDs
#define DS3_REPORT_CAPABILITIES 0x01
#define DS3_REPORT_BT_MAC       0xF2
#define DS3_REPORT_PAIRING      0xF5
#define DS3_REPORT_CALIBRATION  0xF7
#define DS3_REPORT_STATUS       0xF8
#define DS3_REPORT_EF           0xEF

// =================================================================
// Functions
// =================================================================

/**
 * Initialize DS3 emulation layer
 */
void ds3_init(void);

/**
 * Get pointer to a feature report by ID
 */
const uint8_t* ds3_get_feature_report(uint8_t report_id, const char** out_name);

/**
 * Handle SET_REPORT from PS3
 */
void ds3_handle_set_report(uint8_t report_id, const uint8_t* data, size_t len);

/**
 * Save Bluetooth pairing information
 */
void ds3_save_pairing(const char* ps3_addr);

/**
 * Get stored PS3 Bluetooth address
 */
const char* ds3_get_ps3_address(void);

/**
 * Get local Bluetooth address
 */
const char* ds3_get_local_address(void);

/**
 * Update F2 report with local Bluetooth MAC
 */
void ds3_set_local_bt_mac(const uint8_t* mac);

#endif // ROSETTAPAD_DS3_H