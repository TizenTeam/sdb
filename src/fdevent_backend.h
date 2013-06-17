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
#ifndef __FDEVENT_I_H
#define __FDEVENT_I_H

#include "fdevent.h"

extern fdevent list_pending;
extern fdevent **fd_table;
extern int fd_table_max;

void fdevent_plist_enqueue(fdevent *node);
void fdevent_plist_remove(fdevent *node);
fdevent *fdevent_plist_dequeue(void);
void fdevent_register(fdevent *fde);
void fdevent_unregister(fdevent *fde);

void _fatal(const char *fn, const char *fmt, ...);
#define FATAL(x...) _fatal(__FUNCTION__, x)

#if DEBUG
void dump_fde(fdevent *fde, const char *info)
{
    fprintf(stderr,"FDE #%03d %c%c%c %s\n", fde->fd,
            fde->state & FDE_READ ? 'R' : ' ',
            fde->state & FDE_WRITE ? 'W' : ' ',
            fde->state & FDE_ERROR ? 'E' : ' ',
            info);
}
#else
#define dump_fde(fde, info) do { } while(0)
#endif

struct fdevent_os_backend {
    // human-readable name
    const char *name;
    void (*fdevent_loop)(void);
    void (*fdevent_init)(void);
    void (*fdevent_connect)(fdevent *fde);
    void (*fdevent_disconnect)(fdevent *fde);
    void (*fdevent_update)(fdevent *fde, unsigned events);
};

extern const struct fdevent_os_backend* fdevent_backend;
extern const struct fdevent_os_backend fdevent_unix_backend;
extern const struct fdevent_os_backend fdevent_windows_backend;

#endif
