/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "utils.h"
#include "fdevent.h"
#include "libusb/usb.h"

#define   TRACE_TAG  TRACE_USB
#include "sdb.h"
#include "strutils.h"
#include "sdb_usb.h"

SDB_MUTEX_DEFINE( usb_lock );

struct usb_handle
{
    usb_handle *prev;
    usb_handle *next;

    usb_dev_handle *usb_handle;
    char unique_node_path[PATH_MAX+1];
    unsigned char end_point[2]; // 0:in, 1:out
    int interface;
    unsigned zero_mask;

};

static usb_handle handle_list = {
    .prev = &handle_list,
    .next = &handle_list,
};

int is_device_registered(const char *unique_node_path)
{
    usb_handle *usb;
    int r = 0;
    sdb_mutex_lock(&usb_lock);
    for(usb = handle_list.next; usb != &handle_list; usb = usb->next){
        if(!strcmp(usb->unique_node_path, unique_node_path)) {
            // set mark flag to indicate this device is still alive
            usb->zero_mask = 1;
            r = 1;
            break;
        }
    }
    sdb_mutex_unlock(&usb_lock);
    return r;
}

void register_device(struct usb_device *dev)
{
    usb_dev_handle *udev;
    int ret, i;

    char usb_path[PATH_MAX+1];

    snprintf(usb_path, sizeof(usb_path), "/dev/bus/usb/%s/%s", dev->bus->dirname, dev->filename);

    if (is_device_registered(usb_path)) {
        //D("skip to register device: %s\n", usb_path);
        return;
    }

    if (!dev->config) {
        D("couldn't retrieve descriptors\n");
        return;
    }

    struct usb_interface_descriptor *altsetting = NULL;
    ret = 0;
    for (i = 0; i < dev->descriptor.bNumConfigurations; i++) {
        struct usb_config_descriptor *config = &dev->config[i];
        int interface_index = 0;
        for (interface_index = 0; interface_index < config->bNumInterfaces; interface_index++) {
            struct usb_interface *interface = &config->interface[interface_index];
            int altsetting_index = 0;
            for (altsetting_index = 0; altsetting_index < interface->num_altsetting; altsetting_index++) {
                altsetting = &interface->altsetting[altsetting_index];

                if (is_sdb_interface(altsetting->bInterfaceClass, altsetting->bInterfaceSubClass, altsetting->bInterfaceProtocol)) {
                    ret = 1;
                    break;
                }

            }
        }
    }
    if (ret == 0) {
        //D("fail to get sdb interface descriptor\n");
        return;
    }
    if (altsetting->bNumEndpoints !=2) {
        D("the number of endpoint should be two\n");
        return;
    }

    usb_handle* usb = NULL;
    usb = calloc(1, sizeof(usb_handle));

    if (usb == NULL) {
        return;
    }

    int sdb_configuration = 2;
    const struct usb_endpoint_descriptor *ep1 = &altsetting->endpoint[0];
    const struct usb_endpoint_descriptor *ep2 = &altsetting->endpoint[1];

    usb->interface = altsetting->bInterfaceNumber;
    // find out which  endpoint is in or out
    if (ep1->bEndpointAddress & USB_ENDPOINT_DIR_MASK) {

        usb->end_point[0] = ep1->bEndpointAddress;
        usb->end_point[1] = ep2->bEndpointAddress;
    } else {
        usb->end_point[0] = ep2->bEndpointAddress;
        usb->end_point[1] = ep1->bEndpointAddress;
    }

    if (altsetting->bInterfaceProtocol == 0x01) {
        usb->zero_mask = ep1->wMaxPacketSize - 1;
    }

    udev = usb_open(dev);
    if (udev == NULL) {
        D("unable to open %s\n", usb_path);
        free(usb);
        return;
    }
    usb->usb_handle = udev;
    char serial[256];

    if (dev->descriptor.iSerialNumber) {
        ret = usb_get_string_simple(udev, dev->descriptor.iSerialNumber, serial, sizeof(serial));
        if (ret < 1) {
            D("error to get serial name: %d(%s)\n", ret, strerror(errno));
            strcpy(serial, "unknown");
        }
    }

    s_strncpy(usb->unique_node_path, usb_path, sizeof(usb->unique_node_path));

    /*
    // detach any kernel driver
    if ((ret = libusb_kernel_driver_active(dev_handle, 0))) {
        if ((ret = libusb_detach_kernel_driver(dev_handle, 0))) {
            D ("detach!\n");
        }
    }*/

    ret = usb_reset(udev);
    ret = usb_set_configuration(udev, sdb_configuration);
    // claim the device
    ret = usb_claim_interface(udev, usb->interface);
    D("claim %d: %d(%s)\n", ret, errno, strerror(errno));

    sdb_mutex_lock(&usb_lock);
    usb->next = &handle_list;
    usb->prev = handle_list.prev;
    usb->prev->next = usb;
    usb->next->prev = usb;

    D("-register new device (in: %04x, out: %04x) from %s\n",usb->end_point[0], usb->end_point[1], usb_path);

    register_usb_transport(usb, serial, 1);
    sdb_mutex_unlock(&usb_lock);
}


void do_lsusb()
{
    struct usb_bus *bus;

    usb_init();

    usb_find_busses();
    usb_find_devices();

    for (bus = usb_busses; bus; bus = bus->next) {
        struct usb_device *dev;

        for (dev = bus->devices; dev; dev = dev->next) {
            register_device(dev);
        }

    }
    //D("finished loop\n");
}


void* usb_poll_thread(void* sleep_msec)
{
    D("created usb detecting thread\n");
    int  mseconds = (int) sleep_msec;

    while (1) {
        do_lsusb();
        //kick_disconnected_devices();
        sdb_sleep_ms(mseconds);
    }
    return NULL;
}


void sdb_usb_init(void)
{
    sdb_thread_t tid;

    if(sdb_thread_create(&tid, usb_poll_thread, (void*)1000)){
        fatal_errno("cannot create input thread");
    }
}

void sdb_usb_cleanup()
{
    close_usb_devices();
}

int sdb_usb_write(usb_handle *h, const void *_data, int len)
{
    char *data = (char*) _data;
    int n = 0;
    int need_zero = 0;

    D("+sdb_usb_write\n");

    if(h->zero_mask) {
            /* if we need 0-markers and our transfer
            ** is an even multiple of the packet size,
            ** we make note of it
            */
        if(!(len & h->zero_mask)) {
            need_zero = 1;
        }
    }

    while(len > 0) {
        int xfer = (len > MAX_READ_WRITE) ? MAX_READ_WRITE : len;

        n = usb_bulk_write(h->usb_handle,  h->end_point[1], data, xfer, 0);
        if(n != xfer) {
            D("fail to usb write: n = %d, errno = %d (%s)\n", n, errno, strerror(errno));
            return -1;
        }

        len -= xfer;
        data += xfer;
    }

    if(need_zero){
        n = usb_bulk_write(h->usb_handle,  h->end_point[1], (char *)_data, 0, 0);
        if (n < 0) {
            D("fail to to usb write for 0 len from %p\n", h->usb_handle);
        }
        return n;
    }
    D("-usb_write\n");
    return 0;
}

int sdb_usb_read(usb_handle *h, void *_data, int len)
{
    char *data = (char*) _data;
    int n;

    D("+sdb_usb_read\n");
    while(len > 0) {
        int xfer = (len > MAX_READ_WRITE) ? MAX_READ_WRITE : len;

        n = usb_bulk_read(h->usb_handle, h->end_point[0], data, xfer, 0);

        if(n != xfer) {
            if((errno == ETIMEDOUT)) {
                D("usb bulk read timeout\n");
                if(n > 0){
                    data += n;
                    len -= n;
                }
                continue;
            }
            D("fail to usb read: n = %d, errno = %d (%s)\n", n, errno, strerror(errno));
            return -1;
        }

        len -= xfer;
        data += xfer;
    }

    D("-sdb_usb_read\n");
    return 0;
}

void sdb_usb_kick(usb_handle *h)
{
    D("+kicking\n");
    D("-kicking\n");
}

int sdb_usb_close(usb_handle *h)
{
    D("+usb close\n");
    sdb_mutex_lock(&usb_lock);
    h->next->prev = h->prev;
    h->prev->next = h->next;
    h->prev = 0;
    h->next = 0;

    usb_release_interface(h->usb_handle, h->interface);
    usb_close(h->usb_handle);
    sdb_mutex_unlock(&usb_lock);

    if (h != NULL) {
        free(h);
        h = NULL;
    }
    D("-usb close\n");
    return 0;
}

int is_sdb_interface(int usb_class, int usb_subclass, int usb_protocol)
{
    if (usb_class == SDB_INTERFACE_CLASS && usb_subclass == SDB_INTERFACE_SUBCLASS
            && usb_protocol == SDB_INTERFACE_PROTOCOL) {
        return 1;
    }

    return 0;
}

#ifdef _USE_UDEV_

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>

int udev_notify(void)
{
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev;

    struct udev_monitor *mon;
    int fd;

    /* Create the udev object */
    udev = udev_new();
    if (!udev) {
        printf("Can't create udev\n");
        exit(1);
    }

    /* This section sets up a monitor which will report events when
       devices attached to the system change.  Events include "add",
       "remove", "change", "online", and "offline".

       This section sets up and starts the monitoring. Events are
       polled for (and delivered) later in the file.

       It is important that the monitor be set up before the call to
       udev_enumerate_scan_devices() so that events (and devices) are
       not missed.  For example, if enumeration happened first, there
       would be no event generated for a device which was attached after
       enumeration but before monitoring began.

       Note that a filter is added so that we only get events for
       "hidraw" devices. */

    /* Set up a monitor to monitor hidraw devices */
    mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device");
    udev_monitor_enable_receiving(mon);
    /* Get the file descriptor (fd) for the monitor.
       This fd will get passed to select() */
    fd = udev_monitor_get_fd(mon);


    /* Create a list of the devices in the 'hidraw' subsystem. */
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_scan_devices(enumerate);


    devices = udev_enumerate_get_list_entry(enumerate);
    /* For each item enumerated, print out its information.
       udev_list_entry_foreach is a macro which expands to
       a loop. The loop will be executed for each member in
       devices, setting dev_list_entry to a list entry
       which contains the device's path in /sys. */
    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path;

        /* Get the filename of the /sys entry for the device
           and create a udev_device object (dev) representing it */
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);

        /* usb_device_get_devnode() returns the path to the device node
           itself in /dev. */
        printf("Device Node Path: %s\n", udev_device_get_devnode(dev));

        /* The device pointed to by dev contains information about
           the hidraw device. In order to get information about the
           USB device, get the parent device with the
           subsystem/devtype pair of "usb"/"usb_device". This will
           be several levels up the tree, but the function will find
           it.*/
        /*dev = udev_device_get_parent_with_subsystem_devtype(
               dev,
               "usb",
               "usb_device");
        if (!dev) {
            printf("Unable to find parent usb device.");
            exit(1);
        }*/

        /* From here, we can call get_sysattr_value() for each file
           in the device's /sys entry. The strings passed into these
           functions (idProduct, idVendor, serial, etc.) correspond
           directly to the files in the /sys directory which
           represents the USB device. Note that USB strings are
           Unicode, UCS2 encoded, but the strings returned from
           udev_device_get_sysattr_value() are UTF-8 encoded. */
        printf("  VID/PID: %s %s\n",
                udev_device_get_sysattr_value(dev,"idVendor"),
                udev_device_get_sysattr_value(dev, "idProduct"));
        printf("  %s\n  %s\n",
                udev_device_get_sysattr_value(dev,"manufacturer"),
                udev_device_get_sysattr_value(dev,"product"));
        printf("  serial: %s\n",
                 udev_device_get_sysattr_value(dev, "serial"));
        udev_device_unref(dev);
    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);

    /* Begin polling for udev events. Events occur when devices
       attached to the system are added, removed, or change state.
       udev_monitor_receive_device() will return a device
       object representing the device which changed and what type of
       change occured.

       The select() system call is used to ensure that the call to
       udev_monitor_receive_device() will not block.

       The monitor was set up earler in this file, and monitoring is
       already underway.

       This section will run continuously, calling usleep() at the end
       of each pass. This is to demonstrate how to use a udev_monitor
       in a non-blocking way. */
    while (1) {
        /* Set up the call to select(). In this case, select() will
           only operate on a single file descriptor, the one
           associated with our udev_monitor. Note that the timeval
           object is set to 0, which will cause select() to not
           block. */
        fd_set fds;
        struct timeval tv;
        int ret;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        ret = select(fd+1, &fds, NULL, NULL, &tv);

        /* Check if our file descriptor has received data. */
        if (ret > 0 && FD_ISSET(fd, &fds)) {
            printf("\nselect() says there should be data\n");

            /* Make the call to receive the device.
               select() ensured that this will not block. */
            dev = udev_monitor_receive_device(mon);
            if (dev) {
                printf("Got Device\n");
                printf("   Node: %s\n", udev_device_get_devnode(dev));
                printf("   Subsystem: %s\n", udev_device_get_subsystem(dev));
                printf("   Devtype: %s\n", udev_device_get_devtype(dev));

                printf("   Action: %s\n", udev_device_get_action(dev));
                udev_device_unref(dev);
            }
            else {
                printf("No Device from receive_device(). An error occured.\n");
            }
        }
        usleep(250*1000);
        printf(".");
        fflush(stdout);
    }


    udev_unref(udev);

    return 0;
}

#endif
