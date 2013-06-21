
#ifndef __USB_H
#define __USB_H


#include <limits.h>

// should bo implements for each os type
typedef struct usb_handle usb_handle;
typedef struct sdb_usb_dev_handle sdb_usb_dev_handle;

void    do_lsusb(void);

void    sdb_usb_init();
void    sdb_usb_cleanup();
int     sdb_usb_write(usb_handle *h, const void *data, int len);
int     sdb_usb_read(usb_handle *h, void *data, int len);
int     sdb_usb_close(usb_handle *h);
void    sdb_usb_kick(usb_handle *h);

int     is_sdb_interface(int vendor_id, int usb_class, int usb_subclass, int usb_protocol);
int     is_device_registered(const char *node_path);
void*   usb_poll_thread(void* sleep_msec);
void    kick_disconnected_devices();

#define SDB_INTERFACE_CLASS              0xff
#define SDB_INTERFACE_SUBCLASS           0x20
#define SDB_INTERFACE_PROTOCOL           0x02
// Samsung's USB Vendor ID
#define VENDOR_ID_SAMSUNG                0x04e8

/*
 * Linux usbfs has a limit of one page size for synchronous bulk read/write.
 * 4096 is the most portable maximum we can do for now.
 * Linux usbfs has a limit of 16KB for the URB interface. We use this now
 * to get better performance for USB 2.0 devices.
 */
#define MAX_READ_WRITE  (16 * 1024)

#endif  // __USB_H
