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

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>

#include <stdarg.h>
#include <stddef.h>

#include "utils.h"
#include "utils_backend.h"
#include "fdevent_backend.h"
#include "transport.h"
#include "log.h"

static void alloc_event(SDB_SOCK_HANDLE* h) {
	LOG_INFO("FD(%d), LOCATION(%d)\n", h->handle.fd, current_socket_location);
    HANDLE event = WSACreateEvent();
    socket_event_handle[current_socket_location] = event;
    event_location_to_fd[current_socket_location] = h->handle.fd;

    h->event_location = current_socket_location;
    current_socket_location++;
}

static void free_event(SDB_SOCK_HANDLE* remove_h) {

	LOG_INFO("FD(%d), LOCATION(%d), CUR_SOCKET(%d)\n", remove_h->handle.fd, remove_h->event_location, current_socket_location);

	current_socket_location--;
	int remove_location = remove_h->event_location;
	remove_h->event_location = -1;
	WSACloseEvent(socket_event_handle[remove_location]);
	if(current_socket_location != remove_location) {
		SDB_SOCK_HANDLE* replace_h = (SDB_SOCK_HANDLE*)sdb_handle_map_get(event_location_to_fd[current_socket_location]);
		replace_h->event_location = remove_location;
		socket_event_handle[remove_location] = socket_event_handle[current_socket_location];
		int replace_fd = event_location_to_fd[current_socket_location];
		event_location_to_fd[remove_location] = replace_fd;
	}
}

static int _event_socket_verify(FD_EVENT* fde, WSANETWORKEVENTS* evts) {
    if ((fde->events & FDE_READ) && (evts->lNetworkEvents & (FD_READ | FD_ACCEPT | FD_CLOSE))) {
		return 1;
    }
    if ((fde->events & FDE_WRITE) && (evts->lNetworkEvents & (FD_WRITE | FD_CONNECT | FD_CLOSE))) {
		return 1;
    }
    return 0;
}

static int _socket_wanted_to_flags(int wanted) {
    int flags = 0;
    if (wanted & FDE_READ)
        flags |= FD_READ | FD_ACCEPT | FD_CLOSE;

    if (wanted & FDE_WRITE)
        flags |= FD_WRITE | FD_CONNECT | FD_CLOSE;

    return flags;
}

static int _event_socket_start(FD_EVENT* fde) {
    /* create an event which we're going to wait for */
	int fd = fde->fd;
    long flags = _socket_wanted_to_flags(fde->events);
    SDB_SOCK_HANDLE* __h = (SDB_SOCK_HANDLE*)sdb_handle_map_get(fd);
	LOG_INFO("FD(%d) LOCATION(%d)\n", fd, __h->event_location);
    HANDLE event = socket_event_handle[__h->event_location];

    if (event == INVALID_HANDLE_VALUE) {
        LOG_ERROR( "no event for FD(%d)\n", fd);
        return 0;
    }
	D( "_event_socket_start: hooking FD(%d) for %x (flags %ld)\n", fd, fde->events, flags);
	if (WSAEventSelect(__h->handle.u.socket, event, flags)) {
		LOG_ERROR( "_event_socket_start: WSAEventSelect() for FD(%d) failed, error %d\n", fd, WSAGetLastError());
		exit(1);
		return 0;
	}
    return 1;
}

static void _fdevent_disconnect(FD_EVENT *fde)
{
    int events = fde->events;

    if (events) {
    	SDB_SOCK_HANDLE* h = (SDB_SOCK_HANDLE*)sdb_handle_map_get(fde->fd);
    	if(h == NULL) {
    		LOG_ERROR("FDE of FD(%d) has no socket event handle\n", fde->fd);
    		return;
    	}
    	free_event(h);
    }
}

static void _fdevent_update(FD_EVENT *fde, unsigned events)
{
    unsigned _event = events & FDE_MASK;

    if(fde->events == events) {
        return;
    }
	fde->events = events;

	if(_event == 0) {
	    free_event((SDB_SOCK_HANDLE*)sdb_handle_map_get(fde->fd));
	    return;
	}
	SDB_SOCK_HANDLE* h = (SDB_SOCK_HANDLE*)sdb_handle_map_get(fde->fd);
	if(h == NULL) {
		LOG_ERROR("invalid FD(%d)\n", fde->fd);
		return;
	}

	if(h->event_location == -1) {
		alloc_event(h);
	}
	_event_socket_start(fde);
}

void _fdevent_loop()
{
    do
    {
		if (current_socket_location == 0) {
			D( "fdevent_process: nothing to wait for !!\n" );
			continue;
		}
        LOG_INFO( "sdb_win32: fdevevnt loop for %d events start\n", current_socket_location );
        if (current_socket_location > MAXIMUM_WAIT_OBJECTS) {
            LOG_ERROR("handle count %d exceeds MAXIMUM_WAIT_OBJECTS, aborting!\n", current_socket_location);
            abort();
        }
        int wait_ret = WaitForMultipleObjects( current_socket_location, socket_event_handle, FALSE, INFINITE );

        if(wait_ret == (int)WAIT_FAILED) {
            LOG_ERROR( "sdb_win32: wait failed, error %ld\n", GetLastError() );
            continue;
        }
        else {

        	int _fd = event_location_to_fd[wait_ret];
        	LOG_INFO("wait success. FD(%d), LOCATION(%d)\n", _fd, wait_ret);
        	SDB_HANDLE* _h = sdb_handle_map_get(_fd);
			WSANETWORKEVENTS evts;

			HANDLE event = socket_event_handle[wait_ret];
			if(!WSAEnumNetworkEvents(_h->u.socket, event, &evts)) {
				FD_EVENT*  fde = fdevent_map_get(_fd);
				if(_event_socket_verify(fde, &evts)) {


					if (fde != NULL && fde->fd == _fd) {
						LOG_INFO("FD(%d) start\n", fde->fd);
						fde->func(fde->fd, fde->events, fde->arg);
						LOG_INFO("FD(%d) end\n", fde->fd);
					}
				}
				else {
					LOG_INFO("verify failed\n");
				}


			}
        }
    }
    while (1);
}


const struct fdevent_os_backend fdevent_windows_backend = {
    .fdevent_disconnect = _fdevent_disconnect,
    .fdevent_update = _fdevent_update,
    .fdevent_loop = _fdevent_loop
};


