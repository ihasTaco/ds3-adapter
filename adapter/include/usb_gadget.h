/*
 * RosettaPad - USB Gadget Interface
 * Handles USB FunctionFS setup and communication with PS3
 */

#ifndef ROSETTAPAD_USB_GADGET_H
#define ROSETTAPAD_USB_GADGET_H

#include <stdint.h>

// =================================================================
// USB Gadget Configuration
// =================================================================
#define USB_GADGET_PATH     "/sys/kernel/config/usb_gadget/ds3"
#define USB_FFS_PATH        "/dev/ffs-ds3"
#define USB_UDC_NAME        "3f980000.usb"  // Pi Zero 2W dwc2

// DS3 USB identifiers
#define DS3_USB_VID         0x054C  // Sony
#define DS3_USB_PID         0x0268  // DualShock 3

// Endpoint configuration
#define EP_IN_ADDR          0x81    // Interrupt IN
#define EP_OUT_ADDR         0x02    // Interrupt OUT
#define EP_MAX_PACKET       64
#define EP_INTERVAL         1       // 1ms polling

// =================================================================
// Functions
// =================================================================

/**
 * Initialize USB gadget subsystem
 * Creates ConfigFS gadget structure and mounts FunctionFS
 * @return 0 on success, -1 on failure
 */
int usb_gadget_init(void);

/**
 * Write USB descriptors to ep0
 * Must be called after opening ep0 but before binding UDC
 * @param ep0_fd File descriptor for ep0
 * @return 0 on success, -1 on failure
 */
int usb_gadget_write_descriptors(int ep0_fd);

/**
 * Bind gadget to UDC (makes it visible to host)
 * @return 0 on success, -1 on failure
 */
int usb_gadget_bind(void);

/**
 * Unbind gadget from UDC
 * @return 0 on success, -1 on failure
 */
int usb_gadget_unbind(void);

/**
 * Cleanup USB gadget (unbind, unmount, remove)
 */
void usb_gadget_cleanup(void);

/**
 * USB control endpoint (ep0) handler thread
 * Handles SETUP packets, feature reports, etc.
 * @param arg Unused
 * @return NULL
 */
void* usb_control_thread(void* arg);

/**
 * USB input endpoint (ep1) thread
 * Sends DS3 input reports to PS3
 * @param arg Unused
 * @return NULL
 */
void* usb_input_thread(void* arg);

/**
 * USB output endpoint (ep2) thread  
 * Receives LED/rumble commands from PS3
 * @param arg Unused
 * @return NULL
 */
void* usb_output_thread(void* arg);

/**
 * Open USB endpoint
 * @param endpoint_num 0, 1, or 2
 * @return File descriptor on success, -1 on failure
 */
int usb_open_endpoint(int endpoint_num);

#endif // ROSETTAPAD_USB_GADGET_H