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
#include <libudev.h>
#include <locale.h>
#include <linux/usb/ch9.h>
#include <linux/usbdevice_fs.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "utils.h"
#include "fdevent.h"
#include "strutils.h"
#include "sdb_usb.h"
#include "log.h"
#include "transport.h"

#define   TRACE_TAG  TRACE_USB
#define   URB_TRANSFER_TIMEOUT   0
SDB_MUTEX_DEFINE( usb_lock);

LIST_NODE* usb_list = NULL;

struct usb_handle {
    LIST_NODE* node;

    char unique_node_path[PATH_MAX + 1];
    int node_fd;
    unsigned char end_point[2]; // 0:in, 1:out
    int interface;
};

int register_device(const char* node, const char* serial) {
    int fd;
    unsigned char device_desc[4096];
    unsigned char* desc_current_ptr = NULL;

    if (node == NULL) {
        return -1;
    }
    if (is_device_registered(node)) {
        LOG_DEBUG("already registered device: %s\n", node);
        return -1;
    }
    if ((fd = open(node, O_RDWR)) < 0) {
        LOG_DEBUG("failed to open usb node %s (%s)\n", node, strerror(errno));
        return -1;
    }

    if (read(fd, device_desc, sizeof(device_desc)) < 0) {
        LOG_DEBUG("failed to read usb node %s (%s)\n", node, strerror(errno));
        close(fd);
        return -1;
    }
    desc_current_ptr = device_desc;

    // get device descriptor from head first
    struct usb_device_descriptor* usb_dev =
            (struct usb_device_descriptor*) desc_current_ptr;

    if (USB_DT_DEVICE_SIZE != usb_dev->bLength) {
        LOG_DEBUG("failed to get usb device descriptor\n");
        return -1;
    }

    // move to get device config
    desc_current_ptr += usb_dev->bLength;

    // enumerate all available configuration descriptors
    int i = 0;
    for (i = 0; i < usb_dev->bNumConfigurations; i++) {
        struct usb_config_descriptor* usb_config =
                (struct usb_config_descriptor *) desc_current_ptr;
        if (USB_DT_CONFIG_SIZE != usb_config->bLength) {
            LOG_DEBUG("failed to get usb config descriptor\n");
            break;
        }
        desc_current_ptr += usb_config->bLength;

        unsigned int wTotalLength = usb_config->wTotalLength;
        unsigned int wSumLength = usb_config->bLength;

        if (usb_config->bNumInterfaces < 1) {
            LOG_DEBUG("there is no interfaces\n");
            break;
        }

        while (wSumLength < wTotalLength) {
            int bLength = desc_current_ptr[0];
            int bType = desc_current_ptr[1];

            struct usb_interface_descriptor* usb_interface =
                    (struct usb_interface_descriptor *) desc_current_ptr;

            if (is_sdb_interface(usb_dev->idVendor,
                    usb_interface->bInterfaceClass,
                    usb_interface->bInterfaceSubClass,
                    usb_interface->bInterfaceProtocol)
                    && (USB_DT_INTERFACE_SIZE == bLength
                            && USB_DT_INTERFACE == bType
                            && 2 == usb_interface->bNumEndpoints)) {
                desc_current_ptr += usb_interface->bLength;
                wSumLength += usb_interface->bLength;
                struct usb_endpoint_descriptor *endpoint1 =
                        (struct usb_endpoint_descriptor *) desc_current_ptr;
                desc_current_ptr += endpoint1->bLength;
                wSumLength += endpoint1->bLength;
                struct usb_endpoint_descriptor *endpoint2 =
                        (struct usb_endpoint_descriptor *) desc_current_ptr;
                unsigned char endpoint_in;
                unsigned char endpoint_out;
                unsigned char interface = usb_interface->bInterfaceNumber;
                // TODO: removed!
                {
                    int bConfigurationValue = 2;
                    int n = ioctl(fd, USBDEVFS_RESET);
                    if (n != 0) {
                        LOG_DEBUG("usb reset failed\n");
                    }
                    n = ioctl(fd, USBDEVFS_SETCONFIGURATION,
                            &bConfigurationValue);
                    if (n != 0) {
                        LOG_DEBUG("check kernel is supporting %dth configuration\n", bConfigurationValue);
                    }

                    n = ioctl(fd, USBDEVFS_CLAIMINTERFACE, &interface);
                    if (n != 0) {
                        LOG_DEBUG("usb claim failed\n");
                    }
                }

                // find in/out endpoint address
                if ((endpoint1->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
                        == USB_DIR_IN) {
                    endpoint_in = endpoint1->bEndpointAddress;
                    endpoint_out = endpoint2->bEndpointAddress;
                } else {
                    endpoint_out = endpoint1->bEndpointAddress;
                    endpoint_in = endpoint2->bEndpointAddress;
                }
                // for now i can agree to register usb
                {
                    usb_handle* usb = NULL;
                    usb = calloc(1, sizeof(usb_handle));

                    if (usb == NULL) {
                        break;
                    }
                    usb->node_fd = fd;
                    usb->interface = usb_interface->bInterfaceNumber;
                    usb->end_point[0] = endpoint_in;
                    usb->end_point[1] = endpoint_out;

                    char usb_serial[MAX_SERIAL_NAME] = { 0, };

                    if (serial != NULL) {
                        s_strncpy(usb_serial, serial, sizeof(usb_serial));
                    } else {
                        strcpy(usb_serial, "unknown");
                    }
                    s_strncpy(usb->unique_node_path, node,
                            sizeof(usb->unique_node_path));

                    sdb_mutex_lock(&usb_lock, "usb register locked");
                    usb->node = prepend(&usb_list, usb);
                    LOG_DEBUG("-register new device (in: %04x, out: %04x) from %s\n", usb->end_point[0], usb->end_point[1], node);

                    register_usb_transport(usb, usb_serial);
                    sdb_mutex_unlock(&usb_lock, "usb register unlocked");
                }
                desc_current_ptr += endpoint2->bLength;
                wSumLength += endpoint2->bLength;

            } else {
                wSumLength += usb_interface->bLength;
                desc_current_ptr += usb_interface->bLength;
            }
        }
    }
    return 0;
}

static void usb_plugged(struct udev_device *dev) {
    if (udev_device_get_devnode(dev) != NULL) {
        register_device(udev_device_get_devnode(dev),
                udev_device_get_sysattr_value(dev, "serial"));
    }
}

static void usb_unplugged(struct udev_device *dev) {
    LOG_INFO("check device is removed from the list\n");
}

int usb_register_callback(int msec) {
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev;

    struct udev_monitor *mon;
    int fd;

    // Create the udev object
    udev = udev_new();
    if (!udev) {
        LOG_DEBUG("Can't create udev\n");
        exit(1);
    }

    // Set up a monitor to monitor hidraw devices
    mon = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device");
    udev_monitor_enable_receiving(mon);

    // Get the file descriptor (fd) for the monitor. This fd will get passed to select()
    fd = udev_monitor_get_fd(mon);

    // Create a list of the devices in the 'usb' subsystem.
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_scan_devices(enumerate);

    devices = udev_enumerate_get_list_entry(enumerate);

    LOG_DEBUG("doing lsusb to find tizen devices\n");
    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path;

        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);
        usb_plugged(dev);
        udev_device_unref(dev);

    }
    // Free the enumerator object
    udev_enumerate_unref(enumerate);
    LOG_DEBUG("done lsusb to find tizen devices\n");
    while (1) {
        fd_set fds;
        struct timeval tv;
        int ret;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        ret = select(fd + 1, &fds, NULL, NULL, &tv);

        if (ret > 0 && FD_ISSET(fd, &fds)) {
            dev = udev_monitor_receive_device(mon);
            if (dev) {
                if (!strcmp("add", udev_device_get_action(dev))) {
                    usb_plugged(dev);
                } else {
                    usb_unplugged(dev);
                }
                udev_device_unref(dev);
            } else {
                LOG_DEBUG("failed to get noti from udev monitor\n");
            }
        }
        usleep(msec);
    }
    udev_unref(udev);

    return 0;
}

int is_device_registered(const char *unique_node_path) {
    int r = 0;
    sdb_mutex_lock(&usb_lock, "usb registering locked");

    LIST_NODE* curptr = usb_list;
    while (curptr != NULL) {
        usb_handle *usb = curptr->data;
        if (!strcmp(usb->unique_node_path, unique_node_path)) {
            r = 1;
            break;
        }
        curptr = curptr->next_ptr;
    }

    sdb_mutex_unlock(&usb_lock, "usb registering unlocked");
    return r;
}

void* usb_callback_thread(void* sleep_msec) {
    LOG_DEBUG("created usb callback thread\n");
    int mseconds = (int) sleep_msec;

    usb_register_callback(mseconds);

    return NULL;
}

void sdb_usb_init(void) {
    sdb_thread_t tid;

    if (sdb_thread_create(&tid, usb_callback_thread, (void*) (250 * 1000))) {
        LOG_FATAL("cannot create input thread\n");
    }
}

void sdb_usb_cleanup() {
    close_usb_devices();
}

#define URB_USERCONTEXT_COOKIE      ((void *)0x1)

static int usb_urb_transfer(usb_handle *h, int ep, char *bytes, int size,
        int timeout) {
    struct usbdevfs_urb urb;
    int bytesdone = 0, requested;
    struct timeval tv, tv_ref, tv_now;
    struct usbdevfs_urb *context;
    int ret, waiting;

    struct timeval tv_cur;
    /*
     * HACK: The use of urb.usercontext is a hack to get threaded applications
     * sort of working again. Threaded support is still not recommended, but
     * this should allow applications to work in the common cases. Basically,
     * if we get the completion for an URB we're not waiting for, then we update
     * the usercontext pointer to 1 for the other threads URB and it will see
     * the change after it wakes up from the the timeout. Ugly, but it works.
     */

    /*
     * Get actual time, and add the timeout value. The result is the absolute
     * time where we have to quit waiting for an message.
     */
    if (gettimeofday(&tv_cur, NULL) != 0) {
        LOG_DEBUG("failed to read clock\n");
        return -1;
    }
    tv_cur.tv_sec = tv_cur.tv_sec + timeout / 1000;
    tv_cur.tv_usec = tv_cur.tv_usec + (timeout % 1000) * 1000;

    if (tv_cur.tv_usec > 1000000) {
        tv_cur.tv_usec -= 1000000;
        tv_cur.tv_sec++;
    }

    do {
        fd_set writefds;

        requested = size - bytesdone;
        if (requested > MAX_READ_WRITE) {
            requested = MAX_READ_WRITE;
            LOG_DEBUG("requested bytes over than %d\n", MAX_READ_WRITE);
        }

        urb.type = USBDEVFS_URB_TYPE_BULK;
        urb.endpoint = ep;
        urb.flags = 0;
        urb.buffer = bytes + bytesdone;
        urb.buffer_length = requested;
        urb.signr = 0;
        urb.actual_length = 0;
        urb.number_of_packets = 0; /* don't do isochronous yet */
        urb.usercontext = NULL;

        ret = ioctl(h->node_fd, USBDEVFS_SUBMITURB, &urb);
        if (ret < 0) {
            LOG_DEBUG("failed to submit urb: %s\n", strerror(errno));
            return -1;
        }

        FD_ZERO(&writefds);
        FD_SET(h->node_fd, &writefds);

        restart: waiting = 1;
        context = NULL;
        for (;;) {
            ret = ioctl(h->node_fd, USBDEVFS_REAPURBNDELAY, &context);
            int saved_errno = errno;
            if (!urb.usercontext && (ret == -1) && waiting) {
                // continue but,
                if (saved_errno == ENODEV) {
                    LOG_DEBUG("device may be unplugged: %s\n", strerror(saved_errno));
                    break;
                }
            } else {
                break;
            }

            tv.tv_sec = 0;
            tv.tv_usec = 1000; // 1 msec

            select(h->node_fd + 1, NULL, &writefds, NULL, &tv); //sub second wait

            if (timeout) {
                /* compare with actual time, as the select timeout is not that precise */
                gettimeofday(&tv_now, NULL);

                if ((tv_now.tv_sec > tv_cur.tv_sec)
                        || ((tv_now.tv_sec == tv_cur.tv_sec)
                                && (tv_now.tv_usec >= tv_ref.tv_usec))) {
                    waiting = 0;
                }
            }
        }

        if (context && context != &urb) {
            context->usercontext = URB_USERCONTEXT_COOKIE;
            /* We need to restart since we got a successful URB, but not ours */
            goto restart;
        }

        /*
         * If there was an error, that wasn't EAGAIN (no completion), then
         * something happened during the reaping and we should return that
         * error now
         */
        if (ret < 0 && !urb.usercontext && errno != EAGAIN)
            LOG_DEBUG("error reaping URB: %s\n", strerror(errno));

        bytesdone += urb.actual_length;
    } while ((ret == 0 || urb.usercontext) && bytesdone < size
            && urb.actual_length == requested);

    /* If the URB didn't complete in success or error, then let's unlink it */
    if (ret < 0 && !urb.usercontext) {
        int rc;
        if (!waiting)
            rc = -ETIMEDOUT;
        else
            rc = urb.status;

        ret = ioctl(h->node_fd, USBDEVFS_DISCARDURB, &urb);
        if (ret < 0 && errno != EINVAL)
            LOG_DEBUG("error discarding URB: %s\n", strerror(errno));

        /*
         * When the URB is unlinked, it gets moved to the completed list and
         * then we need to reap it or else the next time we call this function,
         * we'll get the previous completion and exit early
         */
        ret = ioctl(h->node_fd, USBDEVFS_REAPURB, &context);
        if (ret < 0 && errno != EINVAL)
            LOG_DEBUG("error reaping URB: %s\n", strerror(errno));

        return rc;
    }

    return bytesdone;
}

int sdb_usb_write(usb_handle *h, const void *_data, int len) {
    char *data = (char*) _data;
    int n = 0;

    LOG_DEBUG("+sdb_usb_write\n");

    while (len > 0) {
        int xfer = (len > MAX_READ_WRITE) ? MAX_READ_WRITE : len;

        n = usb_urb_transfer(h, h->end_point[1], data, xfer, URB_TRANSFER_TIMEOUT);
        if (n != xfer) {
            LOG_DEBUG("fail to usb write: n = %d, errno = %d (%s)\n", n, errno, strerror(errno));
            return -1;
        }

        len -= xfer;
        data += xfer;
    }

    LOG_DEBUG("-usb_write\n");

    return 0;
}

int sdb_usb_read(usb_handle *h, void *_data, int len) {
    char *data = (char*) _data;
    int n;

    LOG_DEBUG("+sdb_usb_read\n");

    while (len > 0) {
        int xfer = (len > MAX_READ_WRITE) ? MAX_READ_WRITE : len;

        n = usb_urb_transfer(h, h->end_point[0], data, xfer, URB_TRANSFER_TIMEOUT);
        if (n != xfer) {
            if ((errno == ETIMEDOUT)) {
                LOG_DEBUG("usb bulk read timeout\n");
                if (n > 0) {
                    data += n;
                    len -= n;
                }
                continue;
            }
            LOG_DEBUG("fail to usb read: n = %d, errno = %d (%s)\n", n, errno, strerror(errno));
            return -1;
        }

        len -= xfer;
        data += xfer;
    }

    LOG_DEBUG("-sdb_usb_read\n");

    return 0;
}

void sdb_usb_kick(usb_handle *h) {
    LOG_DEBUG("+kicking\n");
    LOG_DEBUG("-kicking\n");
}

int sdb_usb_close(usb_handle *h) {
    LOG_DEBUG("+usb close\n");

    if (h != NULL) {
        sdb_mutex_lock(&usb_lock, "usb close locked");
        remove_node(&usb_list, h->node, no_free);
        sdb_close(h->node_fd);
        free(h);
        h = NULL;
        sdb_mutex_unlock(&usb_lock, "usb close unlocked");
    }
    LOG_DEBUG("-usb close\n");
    return 0;
}

