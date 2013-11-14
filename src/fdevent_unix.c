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

#include <sys/ioctl.h>

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
#include "log.h"

// This socket is used when a subproc shell service exists.
// It wakes up the fdevent_loop() and cause the correct handling
// of the shell's pseudo-tty master. I.e. force close it.
int SHELL_EXIT_NOTIFY_FD = -1;

#include <sys/select.h>

#define FD_CLR_ALL(fde) \
        FD_CLR(fde->fd, &fds_read); \
        FD_CLR(fde->fd, &fds_write);

static fd_set fds_write;
static fd_set fds_read;

static void _fdevent_disconnect(FD_EVENT *fde)
{
    FD_CLR_ALL(fde);
    //selectn should be reset
    int fd = fde->fd;
    if(fd  >= max_select -1) {
        int i = fd - 1;
        while(i > -1) {
            if(fdevent_map_get(i) !=  NULL) {
                max_select = i+1;
                break;
            }
            --i;
        }
    }
}

static void _fdevent_update(FD_EVENT *fde, unsigned events)
{
    if(fde->events == events) {
        return;
    }

    int fd = fde->fd;
    events = events & FDE_MASK;
    fde->events = events;

    switch (events ) {
        case FDE_READ:
            FD_SET(fd, &fds_read);
            FD_CLR(fd, &fds_write);
            break;
        case FDE_WRITE:
            FD_SET(fd, &fds_write);
            FD_CLR(fd, &fds_read);
            break;
        case 0:
            FD_CLR(fd, &fds_read);
            FD_CLR(fd, &fds_write);
            break;
        default:
            FD_SET(fd, &fds_read);
            FD_SET(fd, &fds_write);
            break;
    }
}

static void _fdevent_loop()
{

    LIST_NODE* event_list = NULL;

    while(1) {

        fd_set rfd, wfd;

        memcpy(&rfd, &fds_read, sizeof(fd_set));
        memcpy(&wfd, &fds_write, sizeof(fd_set));

        LOG_INFO("before select function, max_select %d\n", max_select);
        int n = select(max_select, &rfd, &wfd, NULL, NULL);
        LOG_INFO("%d events happens\n", n);

        if(n < 0) {
            LOG_ERROR("fatal error happens in select loop errno %d, strerr %s\n", errno, strerror(errno));
            FD_ZERO(&wfd);
            FD_ZERO(&rfd);
            continue;
        }
        if(n == 0) {
            LOG_ERROR("select returns 0\n");
            continue;
        }

        int i = 0;
        while( i < max_select) {
            unsigned events = 0;

            if(FD_ISSET(i, &rfd)) {
                events |= FDE_READ;
            }
            if(FD_ISSET(i, &wfd)) {
                events |= FDE_WRITE;
            }

            if(events) {
                LOG_INFO("FD(%d) got events=%04x\n", i, events);
                FD_EVENT* fde = fdevent_map_get(i);
                if(fde == NULL) {
                    LOG_INFO("fdevent FD(%d) may be already closed\n", i);
                    i++;
                    continue;
                }

                fde->node = prepend(&event_list, fde);
                events |= fde->events;
            }
            i++;
        }

        while(event_list != NULL) {
            FD_EVENT* fde = event_list->data;
            remove_first(&event_list, no_free);
            LOG_INFO("FD(%d) start!\n", fde->fd);
            fde->func(fde->fd, fde->events, fde->arg);
            LOG_INFO("FD(%d) end!\n", fde->fd);
        }
    }
}

const struct fdevent_os_backend fdevent_unix_backend = {
    .fdevent_disconnect = _fdevent_disconnect,
    .fdevent_update = _fdevent_update,
    .fdevent_loop = _fdevent_loop
};
