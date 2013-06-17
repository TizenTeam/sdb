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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <fcntl.h>

#include <stdarg.h>
#include <stddef.h>

#include "fdevent_backend.h"
#include "transport.h"
#include "utils.h"

#if defined(OS_LINUX)
const struct fdevent_os_backend* fdevent_backend = &fdevent_unix_backend;
#elif defined(OS_DARWIN)
const struct fdevent_os_backend* fdevent_backend = &fdevent_unix_backend;
#elif defined(OS_WINDOWS)
const struct fdevent_os_backend* fdevent_backend = &fdevent_windows_backend;
#define  WIN32_FH_BASE    100
#else
#error "unsupported OS"
#endif

fdevent list_pending = {
    .next = &list_pending,
    .prev = &list_pending,
};

fdevent **fd_table = 0;
int fd_table_max = 0;

void _fatal(const char *fn, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s:", fn);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    abort();
}

void fdevent_register(fdevent *fde)
{
#if defined(OS_WINDOWS)
    int  fd = fde->fd - WIN32_FH_BASE;
#else
    int  fd = fde->fd;
#endif

    if(fd < 0) {
        FATAL("bogus negative fd (%d)\n", fde->fd);
    }

    if(fd >= fd_table_max) {
        int oldmax = fd_table_max;
        if(fde->fd > 32000) {
            FATAL("bogus huuuuge fd (%d)\n", fde->fd);
        }
        if(fd_table_max == 0) {
            fdevent_backend->fdevent_init();
            fd_table_max = 256;
        }
        while(fd_table_max <= fd) {
            fd_table_max *= 2;
        }
        fd_table = realloc(fd_table, sizeof(fdevent*) * fd_table_max);
        if(fd_table == 0) {
            FATAL("could not expand fd_table to %d entries\n", fd_table_max);
        }
        memset(fd_table + oldmax, 0, sizeof(int) * (fd_table_max - oldmax));
    }

    fd_table[fd] = fde;
}

void fdevent_unregister(fdevent *fde)
{
#if defined(OS_WINDOWS)
    int  fd = fde->fd - WIN32_FH_BASE;
#else
    int  fd = fde->fd;
#endif
    if((fd < 0) || (fd >= fd_table_max)) {
        FATAL("fd out of range (%d)\n", fde->fd);
    }

    if(fd_table[fd] != fde) {
        FATAL("fd_table out of sync");
    }

    fd_table[fd] = 0;

    if(!(fde->state & FDE_DONT_CLOSE)) {
        dump_fde(fde, "close");
        sdb_close(fde->fd);
    }
}

void fdevent_plist_enqueue(fdevent *node)
{
    fdevent *list = &list_pending;

    node->next = list;
    node->prev = list->prev;
    node->prev->next = node;
    list->prev = node;
}

void fdevent_plist_remove(fdevent *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = 0;
    node->prev = 0;
}

fdevent *fdevent_plist_dequeue(void)
{
    fdevent *list = &list_pending;
    fdevent *node = list->next;

    if(node == list) return 0;

    list->next = node->next;
    list->next->prev = list;
    node->next = 0;
    node->prev = 0;

    return node;
}

fdevent *fdevent_create(int fd, fd_func func, void *arg)
{
    fdevent *fde = (fdevent*) malloc(sizeof(fdevent));
    if(fde == 0) return 0;
    fdevent_install(fde, fd, func, arg);
    fde->state |= FDE_CREATED;
    return fde;
}

void fdevent_destroy(fdevent *fde)
{
    if(fde == 0) return;
    if(!(fde->state & FDE_CREATED)) {
        FATAL("fde %p not created by fdevent_create()\n", fde);
    }
    fdevent_remove(fde);
}

void fdevent_install(fdevent *fde, int fd, fd_func func, void *arg)
{
    memset(fde, 0, sizeof(fdevent));
    fde->state = FDE_ACTIVE;
    fde->fd = fd;
    fde->force_eof = 0;
    fde->func = func;
    fde->arg = arg;

#ifndef OS_WINDOWS
    fcntl(fd, F_SETFL, O_NONBLOCK);
#endif
    fdevent_register(fde);
    dump_fde(fde, "connect");
    fdevent_backend->fdevent_connect(fde);
    fde->state |= FDE_ACTIVE;
}

void fdevent_remove(fdevent *fde)
{
    if(fde->state & FDE_PENDING) {
        fdevent_plist_remove(fde);
    }

    if(fde->state & FDE_ACTIVE) {
        fdevent_backend->fdevent_disconnect(fde);
        dump_fde(fde, "disconnect");
        fdevent_unregister(fde);
    }

    fde->state = 0;
    fde->events = 0;
}


void fdevent_set(fdevent *fde, unsigned events)
{
    events &= FDE_EVENTMASK;

    if((fde->state & FDE_EVENTMASK) == events) return;

    if(fde->state & FDE_ACTIVE) {
        fdevent_backend->fdevent_update(fde, events);
        dump_fde(fde, "update");
    }

    fde->state = (fde->state & FDE_STATEMASK) | events;

    if(fde->state & FDE_PENDING) {
            /* if we're pending, make sure
            ** we don't signal an event that
            ** is no longer wanted.
            */
        fde->events &= (~events);
        if(fde->events == 0) {
            fdevent_plist_remove(fde);
            fde->state &= (~FDE_PENDING);
        }
    }
}

void fdevent_add(fdevent *fde, unsigned events) {
    fdevent_set(fde, (fde->state & FDE_EVENTMASK) | (events & FDE_EVENTMASK));
}

void fdevent_del(fdevent *fde, unsigned events) {
    fdevent_set(fde, (fde->state & FDE_EVENTMASK) & (~(events & FDE_EVENTMASK)));
}

void fdevent_loop()
{
    fdevent_backend->fdevent_loop();
}
