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

#define D(...) ((void)0)

static EventHook  _free_hooks;

static EventHook
event_hook_alloc( FH  fh )
{
    EventHook  hook = _free_hooks;
    if (hook != NULL)
        _free_hooks = hook->next;
    else {
        hook = malloc( sizeof(*hook) );
        if (hook == NULL) {
            //_fatal( "could not allocate event hook\n" );
            //TODO:
        }
    }
    hook->next   = NULL;
    hook->fh     = fh;
    hook->wanted = 0;
    hook->ready  = 0;
    hook->h      = INVALID_HANDLE_VALUE;
    hook->aux    = NULL;

    hook->prepare = NULL;
    hook->start   = NULL;
    hook->stop    = NULL;
    hook->check   = NULL;
    hook->peek    = NULL;

    return hook;
}

static void
event_hook_free( EventHook  hook )
{
    hook->fh     = NULL;
    hook->wanted = 0;
    hook->ready  = 0;
    hook->next   = _free_hooks;
    _free_hooks  = hook;
}


static void
event_hook_signal( EventHook  hook )
{
    FH        f   = hook->fh;
    int       fd  = _fh_to_int(f);
    fdevent*  fde = fd_table[ fd - WIN32_FH_BASE ];

    if (fde != NULL && fde->fd == fd) {
        if ((fde->state & FDE_PENDING) == 0) {
            fde->state |= FDE_PENDING;
            fdevent_plist_enqueue( fde );
        }
        fde->events |= hook->wanted;
    }
}

static EventHook*
event_looper_find_p( EventLooper  looper, FH  fh )
{
    EventHook  *pnode = &looper->hooks;
    EventHook   node  = *pnode;
    for (;;) {
        if ( node == NULL || node->fh == fh )
            break;
        pnode = &node->next;
        node  = *pnode;
    }
    return  pnode;
}

static void
event_looper_hook( EventLooper  looper, int  fd, int  events )
{
    FH          f = _fh_from_int(fd);
    EventHook  *pnode;
    EventHook   node;

    if (f == NULL)  /* invalid arg */ {
        D("event_looper_hook: invalid fd=%d\n", fd);
        return;
    }

    pnode = event_looper_find_p( looper, f );
    node  = *pnode;
    if ( node == NULL ) {
        node       = event_hook_alloc( f );
        node->next = *pnode;
        *pnode     = node;
    }

    if ( (node->wanted & events) != events ) {
        /* this should update start/stop/check/peek */
        D("event_looper_hook: call hook for %d (new=%x, old=%x)\n",
           fd, node->wanted, events);
        f->clazz->_fh_hook( f, events & ~node->wanted, node );
        node->wanted |= events;
    } else {
        D("event_looper_hook: ignoring events %x for %d wanted=%x)\n",
           events, fd, node->wanted);
    }
}

static void
event_looper_unhook( EventLooper  looper, int  fd, int  events )
{
    FH          fh    = _fh_from_int(fd);
    EventHook  *pnode = event_looper_find_p( looper, fh );
    EventHook   node  = *pnode;

    if (node != NULL) {
        int  events2 = events & node->wanted;
        if ( events2 == 0 ) {
            D( "event_looper_unhook: events %x not registered for fd %d\n", events, fd );
            return;
        }
        node->wanted &= ~events2;
        if (!node->wanted) {
            *pnode = node->next;
            event_hook_free( node );
        }
    }
}

static EventLooperRec  win32_looper;

static void _fdevent_init(void)
{
    win32_looper.htab_count = 0;
    win32_looper.hooks      = NULL;
}

static void _fdevent_connect(fdevent *fde)
{
    EventLooper  looper = &win32_looper;
    int          events = fde->state & FDE_EVENTMASK;

    if (events != 0)
        event_looper_hook( looper, fde->fd, events );
}

static void _fdevent_disconnect(fdevent *fde)
{
    EventLooper  looper = &win32_looper;
    int          events = fde->state & FDE_EVENTMASK;

    if (events != 0)
        event_looper_unhook( looper, fde->fd, events );
}

static void _fdevent_update(fdevent *fde, unsigned events)
{
    EventLooper  looper  = &win32_looper;
    unsigned     events0 = fde->state & FDE_EVENTMASK;

    if (events != events0) {
        int  removes = events0 & ~events;
        int  adds    = events  & ~events0;
        if (removes) {
            D("fdevent_update: remove %x from %d\n", removes, fde->fd);
            event_looper_unhook( looper, fde->fd, removes );
        }
        if (adds) {
            D("fdevent_update: add %x to %d\n", adds, fde->fd);
            event_looper_hook  ( looper, fde->fd, adds );
        }
    }
}

static void fdevent_process()
{
    EventLooper  looper = &win32_looper;
    EventHook    hook;
    int          gotone = 0;

    /* if we have at least one ready hook, execute it/them */
    for (hook = looper->hooks; hook; hook = hook->next) {
        hook->ready = 0;
        if (hook->prepare) {
            hook->prepare(hook);
            if (hook->ready != 0) {
                event_hook_signal( hook );
                gotone = 1;
            }
        }
    }

    /* nothing's ready yet, so wait for something to happen */
    if (!gotone)
    {
        looper->htab_count = 0;

        for (hook = looper->hooks; hook; hook = hook->next)
        {
            if (hook->start && !hook->start(hook)) {
                D( "fdevent_process: error when starting a hook\n" );
                return;
            }
            if (hook->h != INVALID_HANDLE_VALUE) {
                int  nn;

                for (nn = 0; nn < looper->htab_count; nn++)
                {
                    if ( looper->htab[nn] == hook->h )
                        goto DontAdd;
                }
                looper->htab[ looper->htab_count++ ] = hook->h;
            DontAdd:
                ;
            }
        }

        if (looper->htab_count == 0) {
            D( "fdevent_process: nothing to wait for !!\n" );
            return;
        }

        do
        {
            int   wait_ret;

            D( "sdb_win32: waiting for %d events\n", looper->htab_count );
            if (looper->htab_count > MAXIMUM_WAIT_OBJECTS) {
                D("handle count %d exceeds MAXIMUM_WAIT_OBJECTS, aborting!\n", looper->htab_count);
                abort();
            }
            wait_ret = WaitForMultipleObjects( looper->htab_count, looper->htab, FALSE, INFINITE );
            if (wait_ret == (int)WAIT_FAILED) {
                D( "sdb_win32: wait failed, error %ld\n", GetLastError() );
            } else {
                D( "sdb_win32: got one (index %d)\n", wait_ret );

                /* according to Cygwin, some objects like consoles wake up on "inappropriate" events
                 * like mouse movements. we need to filter these with the "check" function
                 */
                if ((unsigned)wait_ret < (unsigned)looper->htab_count)
                {
                    for (hook = looper->hooks; hook; hook = hook->next)
                    {
                        if ( looper->htab[wait_ret] == hook->h       &&
                         (!hook->check || hook->check(hook)) )
                        {
                            D( "sdb_win32: signaling %s for %x\n", hook->fh->name, hook->ready );
                            event_hook_signal( hook );
                            gotone = 1;
                            break;
                        }
                    }
                }
            }
        }
        while (!gotone);

        for (hook = looper->hooks; hook; hook = hook->next) {
            if (hook->stop)
                hook->stop( hook );
        }
    }

    for (hook = looper->hooks; hook; hook = hook->next) {
        if (hook->peek && hook->peek(hook))
                event_hook_signal( hook );
    }
}

void _fdevent_loop()
{
    fdevent *fde;

    for(;;) {
#if DEBUG
        fprintf(stderr,"--- ---- waiting for events\n");
#endif
        fdevent_process();

        while((fde = fdevent_plist_dequeue())) {
            unsigned events = fde->events;
            fde->events = 0;
            fde->state &= (~FDE_PENDING);
            dump_fde(fde, "callback");
            fde->func(fde->fd, events, fde->arg);
        }
    }
}


const struct fdevent_os_backend fdevent_windows_backend = {
    .name = "windows fdevent",
    .fdevent_init = _fdevent_init,
    .fdevent_connect = _fdevent_connect,
    .fdevent_disconnect = _fdevent_disconnect,
    .fdevent_update = _fdevent_update,
    .fdevent_loop = _fdevent_loop
};


