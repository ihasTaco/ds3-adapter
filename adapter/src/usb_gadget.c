/*
 * RosettaPad Debug Relay - USB Gadget Interface
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/usb/functionfs.h>
#include <linux/usb/ch9.h>

#include "common.h"
#include "ds3.h"
#include "usb_gadget.h"
#include "debug.h"

// =================================================================
// USB Descriptors
// =================================================================

static const struct {
    struct usb_functionfs_descs_head_v2 header;
    __le32 fs_count;
    __le32 hs_count;
    struct {
        struct usb_interface_descriptor intf;
        struct usb_endpoint_descriptor_no_audio ep_in;
        struct usb_endpoint_descriptor_no_audio ep_out;
    } __attribute__((packed)) fs_descs, hs_descs;
} __attribute__((packed)) usb_descriptors = {
    .header = {
        .magic = FUNCTIONFS_DESCRIPTORS_MAGIC_V2,
        .flags = FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC,
        .length = sizeof(usb_descriptors),
    },
    .fs_count = 3,
    .hs_count = 3,
    .fs_descs = {
        .intf = {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DT_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = USB_CLASS_HID,
            .bInterfaceSubClass = 0,
            .bInterfaceProtocol = 0,
            .iInterface = 1,
        },
        .ep_in = {
            .bLength = sizeof(struct usb_endpoint_descriptor_no_audio),
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = EP_IN_ADDR,
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = EP_MAX_PACKET,
            .bInterval = EP_INTERVAL,
        },
        .ep_out = {
            .bLength = sizeof(struct usb_endpoint_descriptor_no_audio),
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = EP_OUT_ADDR,
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = EP_MAX_PACKET,
            .bInterval = EP_INTERVAL,
        },
    },
    .hs_descs = {
        .intf = {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DT_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = USB_CLASS_HID,
            .bInterfaceSubClass = 0,
            .bInterfaceProtocol = 0,
            .iInterface = 1,
        },
        .ep_in = {
            .bLength = sizeof(struct usb_endpoint_descriptor_no_audio),
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = EP_IN_ADDR,
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = EP_MAX_PACKET,
            .bInterval = EP_INTERVAL,
        },
        .ep_out = {
            .bLength = sizeof(struct usb_endpoint_descriptor_no_audio),
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = EP_OUT_ADDR,
            .bmAttributes = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize = EP_MAX_PACKET,
            .bInterval = EP_INTERVAL,
        },
    },
};

static const struct {
    struct usb_functionfs_strings_head header;
    struct {
        __le16 code;
        const char str1[10];
    } __attribute__((packed)) lang0;
} __attribute__((packed)) usb_strings = {
    .header = {
        .magic = FUNCTIONFS_STRINGS_MAGIC,
        .length = sizeof(usb_strings),
        .str_count = 1,
        .lang_count = 1,
    },
    .lang0 = {
        .code = 0x0409,
        .str1 = "DS3 Input",
    },
};

// =================================================================
// Gadget Setup
// =================================================================

int usb_gadget_init(void) {
    debug_print(DBG_INIT, "[USB] Initializing...");

    system("modprobe libcomposite 2>/dev/null");
    system("modprobe usb_f_fs 2>/dev/null");

    if (access(USB_GADGET_PATH, F_OK) != 0) {
        debug_print(DBG_INIT, "[USB] Creating gadget...");

        system("mkdir -p " USB_GADGET_PATH);

        char cmd[256];
        snprintf(cmd, sizeof(cmd), "echo 0x%04x > %s/idVendor", DS3_USB_VID, USB_GADGET_PATH);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "echo 0x%04x > %s/idProduct", DS3_USB_PID, USB_GADGET_PATH);
        system(cmd);

        system("echo 0x0100 > " USB_GADGET_PATH "/bcdDevice");
        system("echo 0x0200 > " USB_GADGET_PATH "/bcdUSB");

        system("mkdir -p " USB_GADGET_PATH "/strings/0x409");
        system("echo '123456' > " USB_GADGET_PATH "/strings/0x409/serialnumber");
        system("echo 'Sony' > " USB_GADGET_PATH "/strings/0x409/manufacturer");
        system("echo 'PLAYSTATION(R)3 Controller' > " USB_GADGET_PATH "/strings/0x409/product");

        system("mkdir -p " USB_GADGET_PATH "/configs/c.1/strings/0x409");
        system("echo 'DS3 Config' > " USB_GADGET_PATH "/configs/c.1/strings/0x409/configuration");
        system("echo 500 > " USB_GADGET_PATH "/configs/c.1/MaxPower");

        system("mkdir -p " USB_GADGET_PATH "/functions/ffs.usb0");
        system("ln -sf " USB_GADGET_PATH "/functions/ffs.usb0 " USB_GADGET_PATH "/configs/c.1/ 2>/dev/null");
    }

    system("mkdir -p " USB_FFS_PATH);
    system("umount " USB_FFS_PATH " 2>/dev/null");
    system("mount -t functionfs usb0 " USB_FFS_PATH);

    debug_print(DBG_INIT, "[USB] Initialized");
    return 0;
}

int usb_gadget_write_descriptors(int ep0_fd) {
    ssize_t written;

    written = write(ep0_fd, &usb_descriptors, sizeof(usb_descriptors));
    if (written != sizeof(usb_descriptors)) {
        debug_print(DBG_ERROR, "[USB] Failed to write descriptors");
        return -1;
    }

    written = write(ep0_fd, &usb_strings, sizeof(usb_strings));
    if (written != sizeof(usb_strings)) {
        debug_print(DBG_ERROR, "[USB] Failed to write strings");
        return -1;
    }

    debug_print(DBG_INIT, "[USB] Descriptors written");
    return 0;
}

int usb_gadget_bind(void) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "echo '%s' > %s/UDC", USB_UDC_NAME, USB_GADGET_PATH);
    int ret = system(cmd);
    if (ret == 0) {
        debug_print(DBG_INIT, "[USB] Bound to UDC");
    }
    return ret == 0 ? 0 : -1;
}

int usb_gadget_unbind(void) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "echo '' > %s/UDC", USB_GADGET_PATH);
    system(cmd);
    debug_print(DBG_USB, "[USB] Unbound");
    return 0;
}

void usb_gadget_cleanup(void) {
    usb_gadget_unbind();
}

int usb_open_endpoint(int endpoint_num) {
    char path[64];
    snprintf(path, sizeof(path), USB_FFS_PATH "/ep%d", endpoint_num);

    int fd = open(path, O_RDWR);
    if (fd < 0) {
        debug_print(DBG_ERROR, "[USB] Failed to open %s: %s", path, strerror(errno));
    }
    return fd;
}

// =================================================================
// Thread Functions
// =================================================================

void* usb_control_thread(void* arg) {
    (void)arg;

    debug_print(DBG_INIT, "[USB] Control thread started");

    while (g_running) {
        struct usb_functionfs_event event;

        if (read(g_ep0_fd, &event, sizeof(event)) < 0) {
            if (errno == EINTR) continue;
            debug_print(DBG_ERROR, "[USB] read ep0 failed: %s", strerror(errno));
            break;
        }

        switch (event.type) {
            case FUNCTIONFS_SETUP: {
                uint8_t bRequest = event.u.setup.bRequest;
                uint16_t wValue = event.u.setup.wValue;
                uint16_t wLength = event.u.setup.wLength;
                uint8_t report_id = wValue & 0xFF;

                debug_print(DBG_USB, "[USB] SETUP: bReq=0x%02X wValue=0x%04X wLen=%d",
                            bRequest, wValue, wLength);

                if (bRequest == 0x0A) {
                    // SET_IDLE
                    read(g_ep0_fd, NULL, 0);
                }
                else if (bRequest == 0x01) {
                    // GET_REPORT
                    const char* name = NULL;
                    const uint8_t* data = ds3_get_feature_report(report_id, &name);

                    if (data) {
                        size_t send_len = (DS3_FEATURE_REPORT_SIZE < wLength) ?
                                          DS3_FEATURE_REPORT_SIZE : wLength;
                        debug_print(DBG_REPORTS, "[USB] GET_REPORT 0x%02X (%s)", report_id, name);
                        write(g_ep0_fd, data, send_len);
                    } else {
                        read(g_ep0_fd, NULL, 0);
                    }
                }
                else if (bRequest == 0x09) {
                    // SET_REPORT
                    uint8_t buf[64] = {0};
                    ssize_t r = 0;

                    if (wLength > 0) {
                        r = read(g_ep0_fd, buf, wLength < 64 ? wLength : 64);
                        if (r > 0) {
                            ds3_handle_set_report(report_id, buf, r);
                        }
                    }
                    write(g_ep0_fd, NULL, 0);
                }
                else {
                    read(g_ep0_fd, NULL, 0);
                }
                break;
            }

            case FUNCTIONFS_ENABLE:
                debug_print(DBG_INFO, "[USB] ENABLED - PS3 connected");
                g_usb_enabled = 1;
                break;

            case FUNCTIONFS_DISABLE:
                debug_print(DBG_INFO, "[USB] DISABLED - PS3 disconnected");
                g_usb_enabled = 0;
                break;

            case FUNCTIONFS_UNBIND:
                debug_print(DBG_USB, "[USB] UNBIND");
                if (!g_mode_switching) {
                    g_running = 0;
                }
                break;

            default:
                break;
        }
    }

    return NULL;
}

void* usb_input_thread(void* arg) {
    (void)arg;

    g_ep1_fd = usb_open_endpoint(1);
    if (g_ep1_fd < 0) {
        debug_print(DBG_ERROR, "[USB] Failed to open ep1");
        return NULL;
    }

    debug_print(DBG_INIT, "[USB] Input thread started");

    // For relay mode, we don't send input reports via USB
    // USB is only used for pairing
    while (g_running) {
        usleep(100000);
    }

    return NULL;
}

void* usb_output_thread(void* arg) {
    (void)arg;

    g_ep2_fd = usb_open_endpoint(2);
    if (g_ep2_fd < 0) {
        debug_print(DBG_ERROR, "[USB] Failed to open ep2");
        return NULL;
    }

    debug_print(DBG_INIT, "[USB] Output thread started");

    uint8_t buf[EP_MAX_PACKET];

    while (g_running) {
        ssize_t n = read(g_ep2_fd, buf, sizeof(buf));

        if (n <= 0) {
            if (errno == EAGAIN) {
                usleep(1000);
                continue;
            }
            continue;
        }

        // Log any output from PS3 during USB mode
        debug_print(DBG_USB, "[USB] Output report (%zd bytes)", n);
    }

    return NULL;
}