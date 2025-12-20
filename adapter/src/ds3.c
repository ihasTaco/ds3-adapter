/*
 * RosettaPad Debug Relay - DS3 Pairing/Feature Reports
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "common.h"
#include "ds3.h"
#include "debug.h"

#ifdef ENABLE_BLUETOOTH
extern void bt_hid_store_ps3_mac(const uint8_t* ps3_mac);
#endif

// =================================================================
// Pairing Configuration
// =================================================================
#define PAIRING_CONFIG_DIR  "/etc/rosettapad"
#define PAIRING_CONFIG_FILE "/etc/rosettapad/pairing.conf"

static char g_local_bt_mac[18] = "00:00:00:00:00:00";
static char g_ps3_bt_mac[18] = "";

// =================================================================
// DS3 Feature Reports
// =================================================================

static uint8_t report_01[DS3_FEATURE_REPORT_SIZE] = {
    0x01, 0x01, 0x04, 0x00, 0x08, 0x0c, 0x01, 0x02, 0x18, 0x18, 0x18, 0x18, 0x09, 0x0a, 0x10, 0x11,
    0x12, 0x13, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x04,
    0x04, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x01, 0x02, 0x07, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t report_f2[DS3_FEATURE_REPORT_SIZE] = {
    0xf2, 0xff, 0xff, 0x00, 0x34, 0xc7, 0x31, 0x25, 0xae, 0x60, 0x00, 0x03, 0x50, 0x81, 0xd8, 0x01,
    0x8a, 0x13, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x04,
    0x04, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x01, 0x02, 0x07, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t report_f5[DS3_FEATURE_REPORT_SIZE] = {
    0xf5, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x03, 0x50, 0x81, 0xd8, 0x01,
    0x8a, 0x13, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x04,
    0x04, 0x04, 0x04, 0x00, 0x00, 0x04, 0x00, 0x01, 0x02, 0x07, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t report_f7[DS3_FEATURE_REPORT_SIZE] = {
    0xf7, 0x02, 0x01, 0x02, 0xcb, 0x01, 0xef, 0xff, 0x14, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t report_f8[DS3_FEATURE_REPORT_SIZE] = {
    0xf8, 0x00, 0x01, 0x00, 0x00, 0x08, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t report_ef[DS3_FEATURE_REPORT_SIZE] = {0xEF};

static uint8_t report_f4[DS3_FEATURE_REPORT_SIZE] = {
    0xF4, 0x42, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// =================================================================
// Internal Functions
// =================================================================

static int parse_mac(const char* str, uint8_t* out) {
    unsigned int bytes[6];
    if (sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
               &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5]) != 6) {
        return -1;
    }
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)bytes[i];
    return 0;
}

static void ds3_read_local_bt_mac(void) {
    FILE* f = fopen("/sys/class/bluetooth/hci0/address", "r");
    if (!f) {
        debug_print(DBG_BT | DBG_WARN, "[DS3] Cannot read BT MAC");
        return;
    }

    char mac_buf[32] = {0};
    if (!fgets(mac_buf, sizeof(mac_buf), f)) {
        fclose(f);
        return;
    }
    fclose(f);

    char* nl = strchr(mac_buf, '\n');
    if (nl) *nl = '\0';

    for (char* p = mac_buf; *p; p++) {
        if (*p >= 'a' && *p <= 'f') *p -= 32;
    }

    uint8_t mac[6];
    if (parse_mac(mac_buf, mac) == 0) {
        report_f2[4] = mac[0];
        report_f2[5] = mac[1];
        report_f2[6] = mac[2];
        report_f2[7] = mac[3];
        report_f2[8] = mac[4];
        report_f2[9] = mac[5];

        strncpy(g_local_bt_mac, mac_buf, sizeof(g_local_bt_mac) - 1);
        debug_print(DBG_BT, "[DS3] F2 MAC: %s", g_local_bt_mac);
    }
}

static void ds3_load_pairing(void) {
    FILE* f = fopen(PAIRING_CONFIG_FILE, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char* eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char* key = line;
        char* value = eq + 1;

        char* nl = strchr(value, '\n');
        if (nl) *nl = '\0';

        if (strcmp(key, "PS3_MAC") == 0) {
            strncpy(g_ps3_bt_mac, value, sizeof(g_ps3_bt_mac) - 1);

            uint8_t mac[6];
            if (parse_mac(g_ps3_bt_mac, mac) == 0) {
                report_f5[2] = mac[0];
                report_f5[3] = mac[1];
                report_f5[4] = mac[2];
                report_f5[5] = mac[3];
                report_f5[6] = mac[4];
                report_f5[7] = mac[5];
            }
        }
    }
    fclose(f);
}

// =================================================================
// Public Functions
// =================================================================

void ds3_init(void) {
    ds3_read_local_bt_mac();
    debug_print(DBG_INIT, "[DS3] Local BT MAC: %s", g_local_bt_mac);

    ds3_load_pairing();
    if (strlen(g_ps3_bt_mac) > 0) {
        debug_print(DBG_INIT, "[DS3] Loaded PS3 pairing: %s", g_ps3_bt_mac);
    }

    debug_print(DBG_INIT, "[DS3] Initialized");
}

const char* ds3_get_ps3_address(void) {
    return strlen(g_ps3_bt_mac) > 0 ? g_ps3_bt_mac : NULL;
}

const char* ds3_get_local_address(void) {
    return g_local_bt_mac;
}

void ds3_set_local_bt_mac(const uint8_t* mac) {
    report_f2[4] = mac[0];
    report_f2[5] = mac[1];
    report_f2[6] = mac[2];
    report_f2[7] = mac[3];
    report_f2[8] = mac[4];
    report_f2[9] = mac[5];

    snprintf(g_local_bt_mac, sizeof(g_local_bt_mac),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    debug_print(DBG_BT, "[DS3] F2 MAC updated: %s", g_local_bt_mac);
}

const uint8_t* ds3_get_feature_report(uint8_t report_id, const char** out_name) {
    const char* name = "UNKNOWN";
    const uint8_t* data = NULL;

    switch (report_id) {
        case DS3_REPORT_CAPABILITIES:
            data = report_01;
            name = "Capabilities";
            break;
        case DS3_REPORT_BT_MAC:
            data = report_f2;
            name = "BT MAC";
            break;
        case DS3_REPORT_PAIRING:
            data = report_f5;
            name = "Pairing";
            break;
        case DS3_REPORT_CALIBRATION:
            data = report_f7;
            name = "Calibration";
            break;
        case DS3_REPORT_STATUS:
            data = report_f8;
            name = "Status";
            break;
        case DS3_REPORT_EF:
            data = report_ef;
            name = "EF Config";
            break;
        case 0xF4:
            data = report_f4;
            name = "Enable (F4)";
            break;
    }

    if (out_name) *out_name = name;
    return data;
}

void ds3_handle_set_report(uint8_t report_id, const uint8_t* data, size_t len) {
    debug_print(DBG_REPORTS, "[DS3] SET_REPORT 0x%02X (%zu bytes)", report_id, len);

    if (report_id == DS3_REPORT_EF) {
        report_ef[0] = 0xEF;
        size_t copy_len = (len > DS3_FEATURE_REPORT_SIZE - 1) ? DS3_FEATURE_REPORT_SIZE - 1 : len;
        memcpy(&report_ef[1], data, copy_len);
    }
    else if (report_id == 0xF4 && len >= 2) {
        if (data[0] == 0x42) {
            debug_print(DBG_HANDSHAKE, "[DS3] PS3 ENABLE: flags=0x%02X", data[1]);
            report_f4[1] = 0x42;
            report_f4[2] = data[1];

#ifdef ENABLE_BLUETOOTH
            extern void bt_hid_set_ps3_enabled(int enabled);
            bt_hid_set_ps3_enabled(1);
#endif
        }
    }
    else if (report_id == DS3_REPORT_PAIRING && len >= 8) {
        char ps3_addr[18];
        snprintf(ps3_addr, sizeof(ps3_addr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 data[2], data[3], data[4], data[5], data[6], data[7]);

        debug_print(DBG_PAIRING, "[DS3] PS3 MAC: %s", ps3_addr);

        report_f5[2] = data[2];
        report_f5[3] = data[3];
        report_f5[4] = data[4];
        report_f5[5] = data[5];
        report_f5[6] = data[6];
        report_f5[7] = data[7];

        ds3_save_pairing(ps3_addr);

#ifdef ENABLE_BLUETOOTH
        bt_hid_store_ps3_mac(&data[2]);
#endif

        g_pairing_complete = 1;
    }
}

void ds3_save_pairing(const char* ps3_addr) {
    mkdir(PAIRING_CONFIG_DIR, 0755);

    FILE* f = fopen(PAIRING_CONFIG_FILE, "w");
    if (!f) {
        debug_print(DBG_WARN, "[DS3] Could not save pairing");
        return;
    }

    fprintf(f, "# RosettaPad Pairing\n");
    fprintf(f, "PS3_MAC=%s\n", ps3_addr);
    fprintf(f, "LOCAL_MAC=%s\n", g_local_bt_mac);

    fclose(f);

    strncpy(g_ps3_bt_mac, ps3_addr, sizeof(g_ps3_bt_mac) - 1);
    debug_print(DBG_PAIRING, "[DS3] Pairing saved");
}