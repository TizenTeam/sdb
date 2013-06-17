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

#define D(...) ((void)0)
// This socket is used when a subproc shell service exists.
// It wakes up the fdevent_loop() and cause the correct handling
// of the shell's pseudo-tty master. I.e. force close it.
int SHELL_EXIT_NOTIFY_FD = -1;

#include <sys/select.h>


static fd_set read_fds;
static fd_set write_fds;
static fd_set error_fds;

static int select_n = 0;

static void _fdevent_init(void)
{
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&error_fds);
}

static void _fdevent_connect(fdevent *fde)
{
    if(fde->fd >= select_n) {
        select_n = fde->fd + 1;
    }
}

static void _fdevent_disconnect(fdevent *fde)
{
    int i, n;

    FD_CLR(fde->fd, &read_fds);
    FD_CLR(fde->fd, &write_fds);
    FD_CLR(fde->fd, &error_fds);

    for(n = 0, i = 0; i < select_n; i++) {
        if(fd_table[i] != 0) n = i;
    }
    select_n = n + 1;
}

static void _fdevent_update(fdevent *fde, unsigned events)
{
    if(events & FDE_READ) {
        FD_SET(fde->fd, &read_fds);
    } else {
        FD_CLR(fde->fd, &read_fds);
    }
    if(events & FDE_WRITE) {
        FD_SET(fde->fd, &write_fds);
    } else {
        FD_CLR(fde->fd, &write_fds);
    }
    if(events & FDE_ERROR) {
        FD_SET(fde->fd, &error_fds);
    } else {
        FD_CLR(fde->fd, &error_fds);
    }

    fde->state = (fde->state & FDE_STATEMASK) | events;
}

/* Looks at fd_table[] for bad FDs and sets bit in fds.
** Returns the number of bad FDs.
*/
static int fdevent_fd_check(fd_set *fds)
{
    int i, n = 0;
    fdevent *fde;

    for(i = 0; i < select_n; i++) {
        fde = fd_table[i];
        if(fde == 0) continue;
        if(fcntl(i, F_GETFL, NULL) < 0) {
            FD_SET(i, fds);
            n++;
            // fde->state |= FDE_DONT_CLOSE;

        }
    }
    return n;
}

#if !DEBUG
static inline void dump_all_fds(const char *extra_msg) {}
#else
static void dump_all_fds(const char *extra_msg)
{
int i;
    fdevent *fde;
    // per fd: 4 digits (but really: log10(FD_SETSIZE)), 1 staus, 1 blank
    char msg_buff[FD_SETSIZE*6 + 1], *pb=msg_buff;
    size_t max_chars = FD_SETSIZE * 6 + 1;
    int printed_out;
#define SAFE_SPRINTF(...)                                                    \
    do {                                                                     \
        printed_out = snprintf(pb, max_chars, __VA_ARGS__);                  \
        if (printed_out <= 0) {                                              \
            D("... snprintf failed.\n");                                     \
            return;                                                          \
        }                                                                    \
        if (max_chars < (unsigned int)printed_out) {                         \
            D("... snprintf out of space.\n");                               \
            return;                                                          \
        }                                                                    \
        pb += printed_out;                                                   \
        max_chars -= printed_out;                                            \
    } while(0)

    for(i = 0; i < select_n; i++) {
        fde = fd_table[i];
        SAFE_SPRINTF("%d", i);
        if(fde == 0) {
            SAFE_SPRINTF("? ");
            continue;
        }
        if(fcntl(i, F_GETFL, NULL) < 0) {
            SAFE_SPRINTF("b");
        }
        SAFE_SPRINTF(" ");
    }
    D("%s fd_table[]->fd = {%s}\n", extra_msg, msg_buff);
}
#endif

static int _fdevent_process()
{
    int i, n;
    fdevent *fde;
    unsigned events;
    fd_set rfd, wfd, efd;

    memcpy(&rfd, &read_fds, sizeof(fd_set));
    memcpy(&wfd, &write_fds, sizeof(fd_set));
    memcpy(&efd, &error_fds, sizeof(fd_set));

    dump_all_fds("pre select()");

    n = select(select_n, &rfd, &wfd, &efd, NULL);
    int saved_errno = errno;
    D("select() returned n=%d, errno=%d\n", n, n<0?saved_errno:0);

    dump_all_fds("post select()");

    if(n < 0) {
        switch(saved_errno) {
        case EINTR: return -1;
        case EBADF:
            // Can't trust the FD sets after an error.
            FD_ZERO(&wfd);
            FD_ZERO(&efd);
            FD_ZERO(&rfd);
            break;
        default:
            D("Unexpected select() error=%d\n", saved_errno);
            return 0;
        }
    }
    if(n <= 0) {
        // We fake a read, as the rest of the code assumes
        // that errors will be detected at that point.
        n = fdevent_fd_check(&rfd);
    }

    for(i = 0; (i < select_n) && (n > 0); i++) {
        events = 0;
        if(FD_ISSET(i, &rfd)) { events |= FDE_READ; n--; }
        if(FD_ISSET(i, &wfd)) { events |= FDE_WRITE; n--; }
        if(FD_ISSET(i, &efd)) { events |= FDE_ERROR; n--; }

        if(events) {
            fde = fd_table[i];
            if(fde == 0)
              FATAL("missing fde for fd %d\n", i);

            fde->events |= events;

            D("got events fde->fd=%d events=%04x, state=%04x\n",
                fde->fd, fde->events, fde->state);
            if(fde->state & FDE_PENDING) continue;
            fde->state |= FDE_PENDING;
            fdevent_plist_enqueue(fde);
        }
    }

    return 0;
}

static void fdevent_call_fdfunc(fdevent* fde)
{
    unsigned events = fde->events;
    fde->events = 0;
    if(!(fde->state & FDE_PENDING)) return;
    fde->state &= (~FDE_PENDING);
    dump_fde(fde, "callback");
    fde->func(fde->fd, events, fde->arg);
}

static void fdevent_subproc_event_func(int fd, unsigned ev, void *userdata)
{

    D("subproc handling on fd=%d ev=%04x\n", fd, ev);

    // Hook oneself back into the fde's suitable for select() on read.
    if((fd < 0) || (fd >= fd_table_max)) {
        FATAL("fd %d out of range for fd_table \n", fd);
    }
    fdevent *fde = fd_table[fd];
    fdevent_add(fde, FDE_READ);

    if(ev & FDE_READ){
      int subproc_fd;

      if(readx(fd, &subproc_fd, sizeof(subproc_fd))) {
          FATAL("Failed to read the subproc's fd from fd=%d\n", fd);
      }
      if((subproc_fd < 0) || (subproc_fd >= fd_table_max)) {
          D("subproc_fd %d out of range 0, fd_table_max=%d\n",
            subproc_fd, fd_table_max);
          return;
      }
      fdevent *subproc_fde = fd_table[subproc_fd];
      if(!subproc_fde) {
          D("subproc_fd %d cleared from fd_table\n", subproc_fd);
          return;
      }
      if(subproc_fde->fd != subproc_fd) {
          // Already reallocated?
          D("subproc_fd %d != fd_table[].fd %d\n", subproc_fd, subproc_fde->fd);
          return;
      }

      subproc_fde->force_eof = 1;

      int rcount = 0;
      ioctl(subproc_fd, FIONREAD, &rcount);
      D("subproc with fd=%d  has rcount=%d err=%d\n",
        subproc_fd, rcount, errno);

      if(rcount) {
        // If there is data left, it will show up in the select().
        // This works because there is no other thread reading that
        // data when in this fd_func().
        return;
      }

      D("subproc_fde.state=%04x\n", subproc_fde->state);
      subproc_fde->events |= FDE_READ;
      if(subproc_fde->state & FDE_PENDING) {
        return;
      }
      subproc_fde->state |= FDE_PENDING;
      fdevent_call_fdfunc(subproc_fde);
    }
}

static void fdevent_subproc_setup()
{
    int s[2];

    if(sdb_socketpair(s)) {
        FATAL("cannot create shell-exit socket-pair\n");
    }
    SHELL_EXIT_NOTIFY_FD = s[0];
    fdevent *fde;
    fde = fdevent_create(s[1], fdevent_subproc_event_func, NULL);
    if(!fde)
      FATAL("cannot create fdevent for shell-exit handler\n");
    fdevent_add(fde, FDE_READ);
}

static void _fdevent_loop()
{
    fdevent *fde;
    fdevent_subproc_setup();

    for(;;) {
        D("--- ---- waiting for events\n");

        if (_fdevent_process() < 0) {
            return;
        }

        while((fde = fdevent_plist_dequeue())) {
            fdevent_call_fdfunc(fde);
        }
    }
}

const struct fdevent_os_backend fdevent_unix_backend = {
    .name = "unix fdevent",
    .fdevent_init = _fdevent_init,
    .fdevent_connect = _fdevent_connect,
    .fdevent_disconnect = _fdevent_disconnect,
    .fdevent_update = _fdevent_update,
    .fdevent_loop = _fdevent_loop
};
