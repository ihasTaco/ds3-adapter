/*
 * RosettaPad Debug Relay - Bluetooth HID Interface for PS3
 * Pi A: Connects to PS3 via BT, relays all traffic to/from Pi B via TCP
 * 
 * Data flow:
 *   PS3 --BT--> Pi A --TCP--> Pi B --BT--> DS3
 *   PS3 <--BT-- Pi A <--TCP-- Pi B <--BT-- DS3
 * 
 * Pi B does all the logging.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "common.h"
#include "ds3.h"
#include "bt_hid.h"
#include "debug.h"

// =================================================================
// Relay Configuration
// =================================================================

char g_relay_host[256] = "";
int g_relay_port = 5555;
int g_relay_sock = -1;
static pthread_mutex_t g_relay_send_mutex = PTHREAD_MUTEX_INITIALIZER;

// =================================================================
// Internal State
// =================================================================

typedef struct {
    bt_state_t state;

    int control_sock;
    int interrupt_sock;

    uint8_t ps3_mac[6];
    uint8_t local_mac[6];
    int has_ps3_mac;

    int hci_dev_id;
    int hci_sock;

    int handshake_complete;
    int ps3_enabled;

    pthread_mutex_t mutex;

} bt_internal_state_t;

static bt_internal_state_t bt_state = {
    .state = BT_STATE_IDLE,
    .control_sock = -1,
    .interrupt_sock = -1,
    .has_ps3_mac = 0,
    .hci_dev_id = -1,
    .hci_sock = -1,
    .handshake_complete = 0,
    .ps3_enabled = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER
};

// =================================================================
// Utility Functions
// =================================================================

void bt_hid_mac_to_str(const uint8_t* mac, char* out) {
    sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int bt_hid_str_to_mac(const char* str, uint8_t* out) {
    unsigned int tmp[6];
    if (sscanf(str, "%02X:%02X:%02X:%02X:%02X:%02X",
               &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4], &tmp[5]) != 6) {
        return -1;
    }
    for (int i = 0; i < 6; i++) {
        out[i] = (uint8_t)tmp[i];
    }
    return 0;
}

// =================================================================
// Relay Functions (TCP to Pi B)
// =================================================================

int relay_connect(const char* host, int port) {
    struct sockaddr_in addr;
    int sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        debug_print(DBG_ERROR, "[Relay] socket() failed: %s", strerror(errno));
        return -1;
    }

    // Disable Nagle for low latency
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        debug_print(DBG_ERROR, "[Relay] Invalid address: %s", host);
        close(sock);
        return -1;
    }

    debug_print(DBG_INFO, "[Relay] Connecting to %s:%d...", host, port);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        debug_print(DBG_ERROR, "[Relay] connect() failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    debug_print(DBG_INFO, "[Relay] Connected to Pi B");
    g_relay_sock = sock;
    return 0;
}

void relay_disconnect(void) {
    if (g_relay_sock >= 0) {
        close(g_relay_sock);
        g_relay_sock = -1;
    }
}

// Protocol: [channel:1][length:2][payload:N]
int relay_send(uint8_t channel, const uint8_t* data, size_t len) {
    if (g_relay_sock < 0) return -1;

    pthread_mutex_lock(&g_relay_send_mutex);

    uint8_t header[3];
    header[0] = channel;
    header[1] = (len >> 8) & 0xFF;
    header[2] = len & 0xFF;

    ssize_t sent = send(g_relay_sock, header, 3, MSG_NOSIGNAL);
    if (sent != 3) {
        debug_print(DBG_ERROR, "[Relay] Failed to send header");
        pthread_mutex_unlock(&g_relay_send_mutex);
        return -1;
    }

    if (len > 0) {
        sent = send(g_relay_sock, data, len, MSG_NOSIGNAL);
        if (sent != (ssize_t)len) {
            debug_print(DBG_ERROR, "[Relay] Failed to send payload");
            pthread_mutex_unlock(&g_relay_send_mutex);
            return -1;
        }
    }

    pthread_mutex_unlock(&g_relay_send_mutex);
    return 0;
}

int relay_recv(uint8_t* channel, uint8_t* data, size_t* len) {
    if (g_relay_sock < 0) return -1;

    uint8_t header[3];
    ssize_t n = recv(g_relay_sock, header, 3, MSG_WAITALL);
    if (n != 3) {
        if (n == 0) {
            debug_print(DBG_INFO, "[Relay] Connection closed");
        } else if (n > 0) {
            debug_print(DBG_ERROR, "[Relay] Partial header recv");
        } else {
            debug_print(DBG_ERROR, "[Relay] recv failed: %s", strerror(errno));
        }
        return -1;
    }

    *channel = header[0];
    *len = (header[1] << 8) | header[2];

    if (*len > 0 && *len <= 256) {
        n = recv(g_relay_sock, data, *len, MSG_WAITALL);
        if (n != (ssize_t)*len) {
            debug_print(DBG_ERROR, "[Relay] Failed to recv payload");
            return -1;
        }
    }

    return 0;
}

// =================================================================
// Initialization
// =================================================================

int bt_hid_init(void) {
    debug_print(DBG_INIT, "[BT] Initializing...");

    pthread_mutex_lock(&bt_state.mutex);

    bt_state.hci_dev_id = hci_get_route(NULL);
    if (bt_state.hci_dev_id < 0) {
        debug_print(DBG_ERROR, "[BT] No Bluetooth adapter found");
        pthread_mutex_unlock(&bt_state.mutex);
        return -1;
    }

    bt_state.hci_sock = hci_open_dev(bt_state.hci_dev_id);
    if (bt_state.hci_sock < 0) {
        debug_print(DBG_ERROR, "[BT] Failed to open HCI: %s", strerror(errno));
        pthread_mutex_unlock(&bt_state.mutex);
        return -1;
    }

    bdaddr_t local_bdaddr;
    if (hci_read_bd_addr(bt_state.hci_sock, &local_bdaddr, 1000) < 0) {
        debug_print(DBG_ERROR, "[BT] Failed to read local address");
        hci_close_dev(bt_state.hci_sock);
        pthread_mutex_unlock(&bt_state.mutex);
        return -1;
    }

    for (int i = 0; i < 6; i++) {
        bt_state.local_mac[i] = local_bdaddr.b[5 - i];
    }

    char mac_str[18];
    bt_hid_mac_to_str(bt_state.local_mac, mac_str);
    debug_print(DBG_INIT, "[BT] Local MAC: %s", mac_str);

    if (bt_hid_load_pairing() == 0) {
        bt_state.state = BT_STATE_READY;
        bt_hid_mac_to_str(bt_state.ps3_mac, mac_str);
        debug_print(DBG_INIT, "[BT] Loaded PS3 MAC: %s", mac_str);
    } else {
        bt_state.state = BT_STATE_WAITING_FOR_MAC;
        debug_print(DBG_INIT, "[BT] No pairing - need USB pairing first");
    }

    pthread_mutex_unlock(&bt_state.mutex);

    bt_hid_set_device_class();
    bt_hid_set_device_name("PLAYSTATION(R)3 Controller");

    debug_print(DBG_INIT, "[BT] Initialized");
    return 0;
}

void bt_hid_cleanup(void) {
    debug_print(DBG_BT, "[BT] Cleaning up...");

    bt_hid_disconnect();
    relay_disconnect();

    pthread_mutex_lock(&bt_state.mutex);

    if (bt_state.hci_sock >= 0) {
        hci_close_dev(bt_state.hci_sock);
        bt_state.hci_sock = -1;
    }

    bt_state.state = BT_STATE_IDLE;

    pthread_mutex_unlock(&bt_state.mutex);
}

// =================================================================
// Pairing Functions
// =================================================================

void bt_hid_store_ps3_mac(const uint8_t* ps3_mac) {
    pthread_mutex_lock(&bt_state.mutex);

    memcpy(bt_state.ps3_mac, ps3_mac, 6);
    bt_state.has_ps3_mac = 1;

    char mac_str[18];
    bt_hid_mac_to_str(ps3_mac, mac_str);
    debug_print(DBG_PAIRING, "[BT] Stored PS3 MAC: %s", mac_str);

    bt_hid_save_pairing();

    if (bt_state.state == BT_STATE_WAITING_FOR_MAC) {
        bt_state.state = BT_STATE_READY;
    }

    pthread_mutex_unlock(&bt_state.mutex);
}

int bt_hid_get_local_mac(uint8_t* out_mac) {
    pthread_mutex_lock(&bt_state.mutex);
    memcpy(out_mac, bt_state.local_mac, 6);
    pthread_mutex_unlock(&bt_state.mutex);
    return 0;
}

int bt_hid_is_paired(void) {
    pthread_mutex_lock(&bt_state.mutex);
    int paired = bt_state.has_ps3_mac;
    pthread_mutex_unlock(&bt_state.mutex);
    return paired;
}

int bt_hid_get_ps3_mac(uint8_t* out_mac) {
    pthread_mutex_lock(&bt_state.mutex);
    if (!bt_state.has_ps3_mac) {
        pthread_mutex_unlock(&bt_state.mutex);
        return -1;
    }
    memcpy(out_mac, bt_state.ps3_mac, 6);
    pthread_mutex_unlock(&bt_state.mutex);
    return 0;
}

void bt_hid_clear_pairing(void) {
    pthread_mutex_lock(&bt_state.mutex);
    memset(bt_state.ps3_mac, 0, 6);
    bt_state.has_ps3_mac = 0;
    bt_state.state = BT_STATE_WAITING_FOR_MAC;
    pthread_mutex_unlock(&bt_state.mutex);

    unlink(BT_PAIRING_FILE);
    debug_print(DBG_PAIRING, "[BT] Cleared pairing");
}

int bt_hid_load_pairing(void) {
    FILE* f = fopen(BT_PAIRING_FILE, "r");
    if (!f) return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PS3_MAC=", 8) == 0) {
            char* mac_str = line + 8;
            char* nl = strchr(mac_str, '\n');
            if (nl) *nl = '\0';

            if (bt_hid_str_to_mac(mac_str, bt_state.ps3_mac) == 0) {
                bt_state.has_ps3_mac = 1;
                fclose(f);
                return 0;
            }
        }
    }

    fclose(f);
    return -1;
}

int bt_hid_save_pairing(void) {
    mkdir("/etc/rosettapad", 0755);

    FILE* f = fopen(BT_PAIRING_FILE, "w");
    if (!f) {
        debug_print(DBG_ERROR, "[BT] Failed to save pairing");
        return -1;
    }

    char mac_str[18];
    bt_hid_mac_to_str(bt_state.ps3_mac, mac_str);
    fprintf(f, "PS3_MAC=%s\n", mac_str);

    bt_hid_mac_to_str(bt_state.local_mac, mac_str);
    fprintf(f, "LOCAL_MAC=%s\n", mac_str);

    fclose(f);
    debug_print(DBG_PAIRING, "[BT] Saved pairing");
    return 0;
}

// =================================================================
// Connection Functions
// =================================================================

#define L2CAP_CONNECT_TIMEOUT_SEC 10

static int l2cap_connect_psm(const uint8_t* dest_mac, uint16_t psm) {
    struct sockaddr_l2 addr;
    int sock;

    sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (sock < 0) {
        debug_print(DBG_ERROR, "[BT] socket() failed: %s", strerror(errno));
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.l2_family = AF_BLUETOOTH;
    bacpy(&addr.l2_bdaddr, BDADDR_ANY);
    addr.l2_psm = 0;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        debug_print(DBG_ERROR, "[BT] bind() failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    bdaddr_t dest_bdaddr;
    for (int i = 0; i < 6; i++) {
        dest_bdaddr.b[i] = dest_mac[5 - i];
    }

    memset(&addr, 0, sizeof(addr));
    addr.l2_family = AF_BLUETOOTH;
    bacpy(&addr.l2_bdaddr, &dest_bdaddr);
    addr.l2_psm = htobs(psm);

    debug_print(DBG_BT, "[BT] Connecting to PSM 0x%04X...", psm);

    int ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    if (ret < 0 && (errno == EINPROGRESS || errno == EAGAIN)) {
        struct pollfd pfd = { .fd = sock, .events = POLLOUT };
        ret = poll(&pfd, 1, L2CAP_CONNECT_TIMEOUT_SEC * 1000);

        if (ret == 0) {
            debug_print(DBG_ERROR, "[BT] Connection timeout");
            close(sock);
            return -1;
        }

        if (ret < 0) {
            debug_print(DBG_ERROR, "[BT] poll() failed: %s", strerror(errno));
            close(sock);
            return -1;
        }

        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);

        if (error != 0) {
            debug_print(DBG_ERROR, "[BT] connect() failed: %s", strerror(error));
            close(sock);
            return -1;
        }
    } else if (ret < 0) {
        debug_print(DBG_ERROR, "[BT] connect() failed: %s", strerror(errno));
        close(sock);
        return -1;
    }

    // Set back to blocking for simpler relay logic
    fcntl(sock, F_SETFL, flags);

    debug_print(DBG_BT, "[BT] Connected to PSM 0x%04X", psm);
    return sock;
}

int bt_hid_connect(void) {
    pthread_mutex_lock(&bt_state.mutex);

    if (!bt_state.has_ps3_mac) {
        debug_print(DBG_ERROR, "[BT] No PS3 MAC - pair via USB first");
        pthread_mutex_unlock(&bt_state.mutex);
        return -1;
    }

    if (bt_state.state == BT_STATE_CONNECTED) {
        pthread_mutex_unlock(&bt_state.mutex);
        return 0;
    }

    char mac_str[18];
    bt_hid_mac_to_str(bt_state.ps3_mac, mac_str);
    debug_print(DBG_BT, "[BT] Connecting to PS3 at %s...", mac_str);

    bt_state.state = BT_STATE_CONNECTING;
    pthread_mutex_unlock(&bt_state.mutex);

    int ctrl = l2cap_connect_psm(bt_state.ps3_mac, L2CAP_PSM_HID_CONTROL);
    if (ctrl < 0) {
        pthread_mutex_lock(&bt_state.mutex);
        bt_state.state = BT_STATE_ERROR;
        pthread_mutex_unlock(&bt_state.mutex);
        return -1;
    }

    usleep(100000);

    int intr = l2cap_connect_psm(bt_state.ps3_mac, L2CAP_PSM_HID_INTERRUPT);
    if (intr < 0) {
        close(ctrl);
        pthread_mutex_lock(&bt_state.mutex);
        bt_state.state = BT_STATE_ERROR;
        pthread_mutex_unlock(&bt_state.mutex);
        return -1;
    }

    pthread_mutex_lock(&bt_state.mutex);
    bt_state.control_sock = ctrl;
    bt_state.interrupt_sock = intr;
    bt_state.state = BT_STATE_CONNECTED;
    bt_state.handshake_complete = 0;
    bt_state.ps3_enabled = 0;
    pthread_mutex_unlock(&bt_state.mutex);

    debug_print(DBG_BT, "[BT] Connected to PS3!");
    return 0;
}

void bt_hid_disconnect(void) {
    pthread_mutex_lock(&bt_state.mutex);

    if (bt_state.interrupt_sock >= 0) {
        close(bt_state.interrupt_sock);
        bt_state.interrupt_sock = -1;
    }

    if (bt_state.control_sock >= 0) {
        close(bt_state.control_sock);
        bt_state.control_sock = -1;
    }

    if (bt_state.state == BT_STATE_CONNECTED || bt_state.state == BT_STATE_CONNECTING) {
        bt_state.state = bt_state.has_ps3_mac ? BT_STATE_READY : BT_STATE_WAITING_FOR_MAC;
    }

    pthread_mutex_unlock(&bt_state.mutex);
    debug_print(DBG_BT, "[BT] Disconnected");
}

int bt_hid_is_connected(void) {
    pthread_mutex_lock(&bt_state.mutex);
    int connected = (bt_state.state == BT_STATE_CONNECTED);
    pthread_mutex_unlock(&bt_state.mutex);
    return connected;
}

bt_state_t bt_hid_get_state(void) {
    pthread_mutex_lock(&bt_state.mutex);
    bt_state_t state = bt_state.state;
    pthread_mutex_unlock(&bt_state.mutex);
    return state;
}

const char* bt_hid_state_str(void) {
    switch (bt_hid_get_state()) {
        case BT_STATE_IDLE:            return "IDLE";
        case BT_STATE_WAITING_FOR_MAC: return "WAITING_FOR_USB_PAIRING";
        case BT_STATE_READY:           return "READY";
        case BT_STATE_CONNECTING:      return "CONNECTING";
        case BT_STATE_CONNECTED:       return "CONNECTED";
        case BT_STATE_ERROR:           return "ERROR";
        default:                       return "UNKNOWN";
    }
}

void bt_hid_set_ps3_enabled(int enabled) {
    pthread_mutex_lock(&bt_state.mutex);
    bt_state.ps3_enabled = enabled;
    pthread_mutex_unlock(&bt_state.mutex);
}

int bt_hid_is_ps3_enabled(void) {
    pthread_mutex_lock(&bt_state.mutex);
    int enabled = bt_state.ps3_enabled;
    pthread_mutex_unlock(&bt_state.mutex);
    return enabled;
}

// =================================================================
// Data Transfer (not used in relay, but needed for header compat)
// =================================================================

int bt_hid_send_input_report(const uint8_t* report) {
    pthread_mutex_lock(&bt_state.mutex);

    if (bt_state.state != BT_STATE_CONNECTED || bt_state.interrupt_sock < 0) {
        pthread_mutex_unlock(&bt_state.mutex);
        return -1;
    }

    int sock = bt_state.interrupt_sock;
    pthread_mutex_unlock(&bt_state.mutex);

    ssize_t sent = send(sock, report, DS3_BT_INPUT_REPORT_SIZE, MSG_NOSIGNAL);
    if (sent < 0) {
        debug_print(DBG_ERROR, "[BT] Send failed: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int bt_hid_process_control(void) {
    return 0;
}

int bt_hid_process_interrupt(void) {
    return 0;
}

// =================================================================
// Thread Functions - Relay Mode
// =================================================================

/*
 * Output thread: Receives data FROM Pi B (DS3's responses) and sends TO PS3
 */
void* bt_hid_output_thread(void* arg) {
    (void)arg;

    debug_print(DBG_INIT, "[BT] Output thread started (Pi B -> PS3)");

    while (g_running) {
        if (!bt_hid_is_connected() || g_relay_sock < 0) {
            usleep(100000);
            continue;
        }

        uint8_t channel;
        uint8_t data[256];
        size_t len;

        if (relay_recv(&channel, data, &len) != 0) {
            debug_print(DBG_ERROR, "[BT] Relay recv failed, disconnecting");
            g_running = 0;
            break;
        }

        if (len == 0) continue;

        pthread_mutex_lock(&bt_state.mutex);
        int ctrl_sock = bt_state.control_sock;
        int intr_sock = bt_state.interrupt_sock;
        pthread_mutex_unlock(&bt_state.mutex);

        if (channel == L2CAP_PSM_HID_CONTROL && ctrl_sock >= 0) {
            send(ctrl_sock, data, len, MSG_NOSIGNAL);
        } else if (channel == L2CAP_PSM_HID_INTERRUPT && intr_sock >= 0) {
            send(intr_sock, data, len, MSG_NOSIGNAL);
        }
    }

    debug_print(DBG_BT, "[BT] Output thread exiting");
    return NULL;
}

/*
 * Input thread: Receives data FROM PS3 and sends TO Pi B
 */
void* bt_hid_input_thread(void* arg) {
    (void)arg;

    debug_print(DBG_INIT, "[BT] Input thread started (PS3 -> Pi B)");

    struct pollfd fds[2];

    while (g_running) {
        if (!bt_hid_is_connected()) {
            usleep(100000);
            continue;
        }

        pthread_mutex_lock(&bt_state.mutex);
        fds[0].fd = bt_state.control_sock;
        fds[0].events = POLLIN;
        fds[1].fd = bt_state.interrupt_sock;
        fds[1].events = POLLIN;
        pthread_mutex_unlock(&bt_state.mutex);

        int ret = poll(fds, 2, 100);

        if (ret < 0) {
            if (errno == EINTR) continue;
            debug_print(DBG_ERROR, "[BT] poll error: %s", strerror(errno));
            break;
        }

        if (ret == 0) continue;

        // Control channel: PS3 -> Pi B
        if (fds[0].revents & POLLIN) {
            uint8_t buf[256];
            ssize_t len = recv(fds[0].fd, buf, sizeof(buf), 0);

            if (len <= 0) {
                debug_print(DBG_BT, "[BT] Control channel closed");
                bt_hid_disconnect();
                continue;
            }

            relay_send(L2CAP_PSM_HID_CONTROL, buf, len);
        }

        if (fds[0].revents & (POLLERR | POLLHUP)) {
            bt_hid_disconnect();
            continue;
        }

        // Interrupt channel: PS3 -> Pi B
        if (fds[1].revents & POLLIN) {
            uint8_t buf[256];
            ssize_t len = recv(fds[1].fd, buf, sizeof(buf), 0);

            if (len <= 0) {
                debug_print(DBG_BT, "[BT] Interrupt channel closed");
                bt_hid_disconnect();
                continue;
            }

            relay_send(L2CAP_PSM_HID_INTERRUPT, buf, len);
        }

        if (fds[1].revents & (POLLERR | POLLHUP)) {
            bt_hid_disconnect();
            continue;
        }
    }

    debug_print(DBG_BT, "[BT] Input thread exiting");
    return NULL;
}

// =================================================================
// Setup Functions
// =================================================================

int bt_hid_set_device_class(void) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "hciconfig hci%d class 0x%06X 2>/dev/null",
             bt_state.hci_dev_id, BT_CLASS_GAMEPAD);
    return system(cmd) == 0 ? 0 : -1;
}

int bt_hid_set_device_name(const char* name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "hciconfig hci%d name '%s' 2>/dev/null",
             bt_state.hci_dev_id, name);
    return system(cmd) == 0 ? 0 : -1;
}

int bt_hid_set_discoverable(int enable) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "hciconfig hci%d %s 2>/dev/null",
             bt_state.hci_dev_id, enable ? "piscan" : "noscan");
    return system(cmd) == 0 ? 0 : -1;
}