/*
 * RosettaPad Debug Relay - USB Gadget Interface
 */

#ifndef ROSETTAPAD_USB_GADGET_H
#define ROSETTAPAD_USB_GADGET_H

#include <stdint.h>

// =================================================================
// USB Gadget Configuration
// =================================================================
#define USB_GADGET_PATH     "/sys/kernel/config/usb_gadget/ds3"
#define USB_FFS_PATH        "/dev/ffs-ds3"
#define USB_UDC_NAME        "3f980000.usb"

#define DS3_USB_VID         0x054C
#define DS3_USB_PID         0x0268

#define EP_IN_ADDR          0x81
#define EP_OUT_ADDR         0x02
#define EP_MAX_PACKET       64
#define EP_INTERVAL         1

// =================================================================
// Functions
// =================================================================

int usb_gadget_init(void);
int usb_gadget_write_descriptors(int ep0_fd);
int usb_gadget_bind(void);
int usb_gadget_unbind(void);
void usb_gadget_cleanup(void);

void* usb_control_thread(void* arg);
void* usb_input_thread(void* arg);
void* usb_output_thread(void* arg);

int usb_open_endpoint(int endpoint_num);

#endif // ROSETTAPAD_USB_GADGET_H