/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include "sysdeps.h"

#define   TRACE_TAG  TRACE_USB
#include "sdb.h"


struct usb_handle
{
    int fd;
    sdb_cond_t notify;
    sdb_mutex_t lock;
};

void usb_cleanup()
{
    // nothing to do here
}

static void *usb_open_thread(void *x)
{
    struct usb_handle *usb = (struct usb_handle *)x;
    int fd;

    while (1) {
        // wait until the USB device needs opening
        sdb_mutex_lock(&usb->lock);
        while (usb->fd != -1) {
            sdb_cond_wait(&usb->notify, &usb->lock);
        }
        sdb_mutex_unlock(&usb->lock);

        D("[ usb_thread - opening device ]\n");
        do {
            /* XXX use inotify? */
            fd = unix_open("/dev/samsung_sdb", O_RDWR);
#if 0
            if (fd < 0) {
                // to support older kernels
                fd = unix_open("/dev/android", O_RDWR);
            }
#endif
            if (fd < 0) {
                sdb_sleep_ms(1000);
            }
        } while (fd < 0);
        D("[ opening device succeeded ]\n");

        close_on_exec(fd);
        usb->fd = fd;

        D("[ usb_thread - registering device ]\n");
        register_usb_transport(usb, 0, 1);
    }

    // never gets here
    return 0;
}

int usb_write(usb_handle *h, const void *data, int len)
{
    int n;

    D("[ write %d ]\n", len);
    n = sdb_write(h->fd, data, len);
    if(n != len) {
        D("ERROR: n = %d, errno = %d (%s)\n",
            n, errno, strerror(errno));
        return -1;
    }
    D("[ done ]\n");
    return 0;
}

int usb_read(usb_handle *h, void *data, int len)
{
    int n;

    D("[ read %d ]\n", len);
    n = sdb_read(h->fd, data, len);
    if(n != len) {
        D("ERROR: n = %d, errno = %d (%s)\n",
            n, errno, strerror(errno));
        return -1;
    }
    return 0;
}

void usb_init()
{
    usb_handle *h;
    sdb_thread_t tid;
#if 0 //eric
    int fd;
#endif
    h = calloc(1, sizeof(usb_handle));
    h->fd = -1;
    sdb_cond_init(&h->notify, 0);
    sdb_mutex_init(&h->lock, 0);

    // Open the file /dev/android_sdb_enable to trigger 
    // the enabling of the sdb USB function in the kernel.
    // We never touch this file again - just leave it open
    // indefinitely so the kernel will know when we are running
    // and when we are not.
#if 0 //eric
    fd = unix_open("/dev/android_sdb_enable", O_RDWR);
    if (fd < 0) {
       D("failed to open /dev/android_sdb_enable\n");
    } else {
        close_on_exec(fd);
    }
#endif
    D("[ usb_init - starting thread ]\n");
    if(sdb_thread_create(&tid, usb_open_thread, h)){
        fatal_errno("cannot create usb thread");
    }
}

void usb_kick(usb_handle *h)
{
    D("usb_kick\n");
    sdb_mutex_lock(&h->lock);
    sdb_close(h->fd);
    h->fd = -1;

    // notify usb_open_thread that we are disconnected
    sdb_cond_signal(&h->notify);
    sdb_mutex_unlock(&h->lock);
}

int usb_close(usb_handle *h)
{
    // nothing to do here
    return 0;
}
