/*
 * RosettaPad Debug Relay - Bluetooth HID Interface for PS3
 */

#ifndef ROSETTAPAD_BT_HID_H
#define ROSETTAPAD_BT_HID_H

#include <stdint.h>

// =================================================================
// Bluetooth HID Constants
// =================================================================

#define L2CAP_PSM_HID_CONTROL   0x0011
#define L2CAP_PSM_HID_INTERRUPT 0x0013

#define BT_CLASS_GAMEPAD        0x002508

#define DS3_BT_INPUT_REPORT_SIZE    50
#define DS3_BT_OUTPUT_REPORT_SIZE   49

// HID transaction types
#define HID_TRANS_HANDSHAKE     0x00
#define HID_TRANS_SET_PROTOCOL  0x70
#define HID_TRANS_GET_PROTOCOL  0x30
#define HID_TRANS_SET_REPORT    0x50
#define HID_TRANS_GET_REPORT    0x40
#define HID_TRANS_DATA          0xA0

// HID report types
#define HID_REPORT_INPUT        0x01
#define HID_REPORT_OUTPUT       0x02
#define HID_REPORT_FEATURE      0x03

// Handshake responses
#define HID_HANDSHAKE_SUCCESS   0x00
#define HID_HANDSHAKE_ERR_INV_REPORT_ID 0x02

// =================================================================
// Bluetooth State
// =================================================================

typedef enum {
    BT_STATE_IDLE,
    BT_STATE_WAITING_FOR_MAC,
    BT_STATE_READY,
    BT_STATE_CONNECTING,
    BT_STATE_CONNECTED,
    BT_STATE_ERROR
} bt_state_t;

#define BT_PAIRING_FILE "/etc/rosettapad/pairing.conf"

// =================================================================
// Relay Configuration
// =================================================================

extern char g_relay_host[256];
extern int g_relay_port;
extern int g_relay_sock;

// =================================================================
// Initialization
// =================================================================

int bt_hid_init(void);
void bt_hid_cleanup(void);

// =================================================================
// Pairing
// =================================================================

void bt_hid_store_ps3_mac(const uint8_t* ps3_mac);
int bt_hid_get_local_mac(uint8_t* out_mac);
int bt_hid_is_paired(void);
int bt_hid_get_ps3_mac(uint8_t* out_mac);
void bt_hid_clear_pairing(void);
int bt_hid_load_pairing(void);
int bt_hid_save_pairing(void);

// =================================================================
// Connection
// =================================================================

int bt_hid_connect(void);
void bt_hid_disconnect(void);
int bt_hid_is_connected(void);
bt_state_t bt_hid_get_state(void);
const char* bt_hid_state_str(void);
void bt_hid_set_ps3_enabled(int enabled);
int bt_hid_is_ps3_enabled(void);

// =================================================================
// Data Transfer
// =================================================================

int bt_hid_send_input_report(const uint8_t* report);
int bt_hid_process_control(void);
int bt_hid_process_interrupt(void);

// =================================================================
// Relay Functions
// =================================================================

int relay_connect(const char* host, int port);
void relay_disconnect(void);
int relay_send(uint8_t channel, const uint8_t* data, size_t len);
int relay_recv(uint8_t* channel, uint8_t* data, size_t* len);

// =================================================================
// Threads
// =================================================================

void* bt_hid_output_thread(void* arg);
void* bt_hid_input_thread(void* arg);

// =================================================================
// Setup
// =================================================================

int bt_hid_set_device_class(void);
int bt_hid_set_device_name(const char* name);
int bt_hid_set_discoverable(int enable);

// =================================================================
// Utility
// =================================================================

void bt_hid_mac_to_str(const uint8_t* mac, char* out);
int bt_hid_str_to_mac(const char* str, uint8_t* out);

#endif // ROSETTAPAD_BT_HID_H