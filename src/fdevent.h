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

#ifndef __FDEVENT_H
#define __FDEVENT_H

#include <stdint.h>  /* for int64_t */
#include "linkedlist.h"
#include "sdb_map.h"

/* events that may be observed */
#define FDE_READ              1
#define FDE_WRITE             2

#define FDE_MASK              3

typedef struct fd_event FD_EVENT;
typedef void (*fd_func)(int fd, unsigned events, void *userdata);

struct fd_event
{
    LIST_NODE* node;

    int fd;
    unsigned events;

    fd_func func;
    void *arg;
};

int fdevent_wakeup_send;
int fdevent_wakeup_recv;

FD_EVENT fdevent_wakeup_fde;

/* Initialize an fdevent object that was externally allocated
*/
void fdevent_install(FD_EVENT *fde, int fd, fd_func func, void *arg);

/* Uninitialize an fdevent object that was initialized by
** fdevent_install()
*/
void fdevent_remove(FD_EVENT *item);

#define EVENT_MAP_SIZE 256

extern MAP event_map;

FD_EVENT* fdevent_map_get(int key);
void fdevent_map_put(int key, FD_EVENT* value);
void fdevent_map_remove(int key);

/* Change which events should cause notifications
*/

#define FDEVENT_SET(fde, events)    \
        fdevent_backend->fdevent_update(fde, events)

#define FDEVENT_ADD(fde, _events)    \
        fdevent_backend->fdevent_update(fde, (fde)->events | _events)

#define FDEVENT_DEL(fde, _events)    \
        fdevent_backend->fdevent_update(fde, (fde)->events & ~_events)

#define FDEVENT_LOOP()  \
        fdevent_backend->fdevent_loop()

struct fdevent_os_backend {
    void (*fdevent_loop)(void);
    void (*fdevent_disconnect)(FD_EVENT *fde);
    void (*fdevent_update)(FD_EVENT *fde, unsigned events);
};

extern const struct fdevent_os_backend* fdevent_backend;
#ifndef OS_WINDOWS
extern int max_select;
extern const struct fdevent_os_backend fdevent_unix_backend;
#else
extern const struct fdevent_os_backend fdevent_windows_backend;
#endif

#if defined(OS_WINDOWS)
#include <windows.h>

#define  WIN32_MAX_FHS    128
extern MAP sdb_handle_map;
extern HANDLE socket_event_handle[];
extern int event_location_to_fd[];
extern int current_socket_location;
#endif

#endif
