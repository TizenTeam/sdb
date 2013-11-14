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

#define  TRACE_TAG   TRACE_SDB

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

#include "utils.h"
#include "fdevent.h"
#include "sdb.h"
#include "commandline.h"
#include "sdb_constants.h"
#include "listener.h"

#if SDB_TRACE
SDB_MUTEX_DEFINE( D_lock );
#endif

MAP hex_map;

static void local_init(int port);
static void init_wakeup_select_func();

#ifdef OS_WINDOWS
static BOOL WINAPI ctrlc_handler(DWORD type)
{
    exit(STATUS_CONTROL_C_EXIT);
    return TRUE;
}
#endif

void sdb_cleanup(void)
{
    sdb_usb_cleanup();
}

static void init_map() {
    initialize_map(&event_map, EVENT_MAP_SIZE, NULL, NULL, no_free);
    initialize_map(&hex_map, 16, NULL, NULL, no_free);
    MAP_KEY key;
    key.key_int = (int)'0';
    map_put(&hex_map, key, 0);
    key.key_int = (int)'1';
    map_put(&hex_map, key, (void*)('1' - '0'));
    key.key_int = (int)'2';
    map_put(&hex_map, key, (void*)('2' - '0'));
    key.key_int = (int)'3';
    map_put(&hex_map, key, (void*)('3' - '0'));
    key.key_int = (int)'4';
    map_put(&hex_map, key, (void*)('4' - '0'));
    key.key_int = (int)'5';
    map_put(&hex_map, key, (void*)('5' - '0'));
    key.key_int = (int)'6';
    map_put(&hex_map, key, (void*)('6' - '0'));
    key.key_int = (int)'7';
    map_put(&hex_map, key, (void*)('7' - '0'));
    key.key_int = (int)'8';
    map_put(&hex_map, key, (void*)('8' - '0'));
    key.key_int = (int)'9';
    map_put(&hex_map, key, (void*)('9' - '0'));
    key.key_int = (int)'a';
    map_put(&hex_map, key, (void*)10);
    key.key_int = (int)'b';
    map_put(&hex_map, key, (void*)(10 + 'b' - 'a'));
    key.key_int = (int)'c';
    map_put(&hex_map, key, (void*)(10 + 'c' - 'a'));
    key.key_int = (int)'d';
    map_put(&hex_map, key, (void*)(10 + 'd' - 'a'));
    key.key_int = (int)'e';
    map_put(&hex_map, key, (void*)(10 + 'e' - 'a'));
    key.key_int = (int)'f';
    map_put(&hex_map, key, (void*)(10 + 'f' - 'a'));
    key.key_int = (int)'A';
    map_put(&hex_map, key, (void*)10);
    key.key_int = (int)'B';
    map_put(&hex_map, key, (void*)(10 + 'B' - 'A'));
    key.key_int = (int)'C';
    map_put(&hex_map, key, (void*)(10 + 'C' - 'A'));
    key.key_int = (int)'D';
    map_put(&hex_map, key, (void*)(10 + 'D' - 'A'));
    key.key_int = (int)'E';
    map_put(&hex_map, key, (void*)(10 + 'E' - 'A'));
    key.key_int = (int)'F';
    map_put(&hex_map, key, (void*)(10 + 'F' - 'A'));

#if defined(OS_WINDOWS)
    initialize_map(&sdb_handle_map, WIN32_MAX_FHS / 2, NULL, NULL, no_free);
#endif
}

/* Constructs a local name of form tcp:port.
 * target_str points to the target string, it's content will be overwritten.
 * target_size is the capacity of the target string.
 * server_port is the port number to use for the local name.
 */
void build_local_name(char* target_str, size_t target_size, int server_port)
{
  snprintf(target_str, target_size, "tcp:%d", server_port);
}

int sdb_main(int is_daemon, int server_port)
{
#ifdef OS_WINDOWS
    SetConsoleCtrlHandler( ctrlc_handler, TRUE );
#else
    // No SIGCHLD. Let the service subproc handle its children.
    signal(SIGPIPE, SIG_IGN);
#endif

    init_wakeup_select_func();

    sdb_usb_init();
    local_init(DEFAULT_SDB_LOCAL_TRANSPORT_PORT);

    char local_name[30];
    build_local_name(local_name, sizeof(local_name), server_port);
    if(install_listener(local_name, "*smartsocket*", NULL)) {
        _exit(1);
    }
    if (is_daemon) {
        start_logging();
    }
    LOG_INFO("Event loop starting\n");
    FDEVENT_LOOP();

    atexit(sdb_cleanup);
    return 0;
}

static void local_init(int port)
{
    if(port < 1024) {
        port = DEFAULT_SDB_LOCAL_TRANSPORT_PORT;
    }
    int  count = SDB_LOCAL_TRANSPORT_MAX;

    LOG_INFO("try to connect to emulator instances when booting sdb server up\n");
    for ( ; count > 0; count--, port += 10 ) {
        (void) local_connect(port, NULL);
    }
}


void init_wakeup_select_func() {
    D("initialize select wakeup func\n");
    int socket_pair[2];

    if(sdb_socketpair(socket_pair)){
        D("cannot open select wakeup socketpair\n");
        return;
    }
    fdevent_wakeup_send = socket_pair[0];
    fdevent_wakeup_recv = socket_pair[1];

    fdevent_install(&fdevent_wakeup_fde,
                    fdevent_wakeup_recv,
                    wakeup_select_func,
                    0);

    FDEVENT_SET(&fdevent_wakeup_fde, FDE_READ);
}

int main(int argc, char **argv)
{
    log_init();
    sdb_sysdeps_init();
    init_map();

    return process_cmdline(argc - 1, argv + 1);
}
