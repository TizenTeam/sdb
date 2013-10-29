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
#include "fdevent.h"
#include "log.h"

#ifndef OS_WINDOWS
int max_select = 0;
const struct fdevent_os_backend* fdevent_backend = &fdevent_unix_backend;
#else
const struct fdevent_os_backend* fdevent_backend = &fdevent_windows_backend;
#endif

#define TRACE_TAG TRACE_SDB

MAP event_map;

#if defined(OS_WINDOWS)
MAP sdb_handle_map;
HANDLE socket_event_handle[MAXIMUM_WAIT_OBJECTS];
int event_location_to_fd[MAXIMUM_WAIT_OBJECTS];
int current_socket_location = 0;
#endif

void fdevent_register(FD_EVENT *fde)
{
	int fd = fde->fd;

    if(fd < 0) {
        LOG_FATAL("bogus negative fd (%d)\n", fd);
    }

    if(fd > 32000) {
        LOG_FATAL("bogus huuuuge FD(%d)\n", fd);
    }
    else {
    	fdevent_map_put(fd, fde);
    }
}

void fdevent_unregister(FD_EVENT *fde)
{
    int fd = fde->fd;

    if((fd < 0)) {
        LOG_FATAL("fdevent out of range FD(%d)\n", fd);
    }
    else if(fdevent_map_get(fd) != fde) {
        LOG_FATAL("fd event out of sync");
    }
    else {
        fdevent_map_remove(fd);
        sdb_close(fde->fd);
    }
}

void fdevent_install(FD_EVENT *fde, int fd, fd_func func, void *arg)
{
    D("FD(%d)\n", fd);
    memset(fde, 0, sizeof(FD_EVENT));
    fde->fd = fd;
    fde->func = func;
    fde->arg = arg;
    fde->events = 0;

#ifndef OS_WINDOWS
    fcntl(fd, F_SETFL, O_NONBLOCK);
    if(fd >= max_select) {
        max_select = fd + 1;
    }
#endif
    fdevent_register(fde);
}

void fdevent_remove(FD_EVENT *fde)
{
    fdevent_backend->fdevent_disconnect(fde);
    fdevent_unregister(fde);
    fde->events = 0;
}

FD_EVENT* fdevent_map_get(int _key) {
	MAP_KEY key;
	key.key_int = _key;
	return map_get(&event_map, key);
}

void fdevent_map_put(int _key, FD_EVENT* _value) {
	MAP_KEY key;
	key.key_int = _key;
	map_put(&event_map, key, _value);
}

void fdevent_map_remove(int _key) {
	MAP_KEY key;
	key.key_int = _key;
	map_remove(&event_map, key);
}
