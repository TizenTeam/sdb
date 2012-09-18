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
#include <string.h>
#include <errno.h>

#include "sysdeps.h"
#include <sys/types.h>

#ifndef HAVE_WIN32_IPC
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#endif

#define  TRACE_TAG  TRACE_TRANSPORT
#include "sdb.h"

#ifdef __ppc__
#define H4(x)	(((x) & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | (((x) & 0x000000FF) << 24)
static inline void fix_endians(apacket *p)
{
    p->msg.command     = H4(p->msg.command);
    p->msg.arg0        = H4(p->msg.arg0);
    p->msg.arg1        = H4(p->msg.arg1);
    p->msg.data_length = H4(p->msg.data_length);
    p->msg.data_check  = H4(p->msg.data_check);
    p->msg.magic       = H4(p->msg.magic);
}
#else
#define fix_endians(p) do {} while (0)
#endif

#if SDB_HOST
/* we keep a list of opened transports. The atransport struct knows to which
 * local transport it is connected. The list is used to detect when we're
 * trying to connect twice to a given local transport.
 */
#define  SDB_LOCAL_TRANSPORT_MAX  16

SDB_MUTEX_DEFINE( local_transports_lock );

static atransport*  local_transports[ SDB_LOCAL_TRANSPORT_MAX ];
#endif /* SDB_HOST */

static int remote_read(apacket *p, atransport *t)
{
    if(readx(t->sfd, &p->msg, sizeof(amessage))){
        D("remote local: read terminated (message)\n");
        return -1;
    }

    fix_endians(p);

#if 0 && defined __ppc__
    D("read remote packet: %04x arg0=%0x arg1=%0x data_length=%0x data_check=%0x magic=%0x\n",
      p->msg.command, p->msg.arg0, p->msg.arg1, p->msg.data_length, p->msg.data_check, p->msg.magic);
#endif
    if(check_header(p)) {
        D("bad header: terminated (data)\n");
        return -1;
    }

    if(readx(t->sfd, p->data, p->msg.data_length)){
        D("remote local: terminated (data)\n");
        return -1;
    }

    if(check_data(p)) {
        D("bad data: terminated (data)\n");
        return -1;
    }

    return 0;
}

static int remote_write(apacket *p, atransport *t)
{
    int   length = p->msg.data_length;

    fix_endians(p);

#if 0 && defined __ppc__
    D("write remote packet: %04x arg0=%0x arg1=%0x data_length=%0x data_check=%0x magic=%0x\n",
      p->msg.command, p->msg.arg0, p->msg.arg1, p->msg.data_length, p->msg.data_check, p->msg.magic);
#endif
    if(writex(t->sfd, &p->msg, sizeof(amessage) + length)) {
        D("remote local: write terminated\n");
        return -1;
    }

    return 0;
}


int local_connect(int port, const char *device_name) {
    return local_connect_arbitrary_ports(port-1, port, device_name);
}

int local_connect_arbitrary_ports(int console_port, int sdb_port, const char *device_name)
{
    char buf[64];
    int  fd = -1;

#if SDB_HOST
    const char *host = getenv("SDBHOST");
    if (host) {
        fd = socket_network_client(host, sdb_port, SOCK_STREAM);
    }
#endif
    if (fd < 0) {
        fd = socket_loopback_client(sdb_port, SOCK_STREAM);
    }

    if (fd >= 0) {
        D("client: connected on remote on fd %d\n", fd);
        close_on_exec(fd);
        disable_tcp_nagle(fd);
        snprintf(buf, sizeof buf, "%s%d", LOCAL_CLIENT_PREFIX, console_port);
        register_socket_transport(fd, buf, sdb_port, 1, device_name);
        return 0;
    }
    return -1;
}

#if SDB_HOST /* tizen specific */
int get_devicename(int port, char *device_name)
{
    int fd;
    char buffer[MAX_PAYLOAD];
    char *tok = NULL;
    int found = 0;

    fd = unix_open(DEVICEMAP_FILENAME, O_RDONLY);
    if (fd > 0) {
        for(;;) {
            if(read_line(fd, buffer, MAX_PAYLOAD) < 0)
                break;
            tok = strtok(buffer, DEVICEMAP_SEPARATOR);
            if (tok != NULL) {
                strncpy(device_name, tok, DEVICENAME_MAX);
                tok = strtok(NULL, DEVICEMAP_SEPARATOR);
                if (tok != NULL) {
                    if (port == atoi(tok)) {
                        found = 1;
                        break;
                    }
                }
            }
        }
        sdb_close(fd);
    }
    if (found != 1)
        strncpy(device_name, DEFAULT_DEVICENAME, DEVICENAME_MAX);
    D("init device name %s on port %d\n", device_name, port);

    return found;
}

int get_devicename_from_shdmem(int port, char *device_name)
{
    char *vms = NULL;
#ifndef HAVE_WIN32_IPC
    int shm_id;
    void *shared_memory = (void *)0;

    shm_id = shmget( (key_t)port-1, 0, 0);
    if (shm_id == -1)
        return -1;

    shared_memory = shmat(shm_id, (void *)0, SHM_RDONLY);

    if (shared_memory == (void *)-1)
    {
        D("faild to get shdmem key (%d) : %s\n", port, strerror(errno));
        return -1;
    }

    vms = strstr((char*)shared_memory, VMS_PATH);
    if (vms != NULL)
        strncpy(device_name, vms+strlen(VMS_PATH), DEVICENAME_MAX);
    else
        strncpy(device_name, DEFAULT_DEVICENAME, DEVICENAME_MAX);

#else /* _WIN32*/
    HANDLE hMapFile;
    char s_port[5];
    char* pBuf;

    sprintf(s_port, "%d", port-1);
    hMapFile = OpenFileMapping(FILE_MAP_READ, TRUE, s_port);

    if(hMapFile == NULL) {
        D("faild to get shdmem key (%ld) : %s\n", port, GetLastError() );
        return -1;
    }
    pBuf = (char*)MapViewOfFile(hMapFile,
                            FILE_MAP_READ,
                            0,
                            0,
                            50);
    if (pBuf == NULL) {
        D("Could not map view of file (%ld)\n", GetLastError());
        CloseHandle(hMapFile);
        return -1;
    }

    vms = strstr((char*)pBuf, VMS_PATH);
    if (vms != NULL)
        strncpy(device_name, vms+strlen(VMS_PATH), DEVICENAME_MAX);
    else
        strncpy(device_name, DEFAULT_DEVICENAME, DEVICENAME_MAX);
    CloseHandle(hMapFile);
#endif
    D("init device name %s on port %d\n", device_name, port);

    return 0;
}

int read_line(const int fd, char* ptr, size_t maxlen)
{
    unsigned int n = 0;
    char c[2];
    int rc;

    while(n != maxlen) {
        if((rc = sdb_read(fd, c, 1)) != 1)
            return -1; // eof or read err

        if(*c == '\n') {
            ptr[n] = 0;
            return n;
        }
        ptr[n++] = *c;
    }
    return -1; // no space
}
#endif

static void *client_socket_thread(void *x)
{
#if SDB_HOST
//	for (;;){
    int port = DEFAULT_SDB_LOCAL_TRANSPORT_PORT;
    int count = SDB_LOCAL_TRANSPORT_MAX;

    D("transport: client_socket_thread() starting\n");

    /* try to connect to any number of running emulator instances     */
    /* this is only done when SDB starts up. later, each new emulator */
    /* will send a message to SDB to indicate that is is starting up  */
    for (; count > 0; count--, port += 10 ) {
        (void) local_connect(port, NULL);
    }

//		sdb_sleep_ms(1000);
//	}
#endif
    return 0;
}

static void *server_socket_thread(void * arg)
{
    int serverfd, fd;
    struct sockaddr addr;
    socklen_t alen;
    int port = (int)arg;

    D("transport: server_socket_thread() starting\n");
    serverfd = -1;
    for(;;) {
        if(serverfd == -1) {
            serverfd = socket_inaddr_any_server(port, SOCK_STREAM);
            if(serverfd < 0) {
                D("server: cannot bind socket yet\n");
                sdb_sleep_ms(1000);
                continue;
            }
            close_on_exec(serverfd);
        }

        alen = sizeof(addr);
        D("server: trying to get new connection from %d\n", port);
        fd = sdb_socket_accept(serverfd, &addr, &alen);
        if(fd >= 0) {
            D("server: new connection on fd %d\n", fd);
            close_on_exec(fd);
            disable_tcp_nagle(fd);
            register_socket_transport(fd, "host", port, 1, NULL);
        }
    }
    D("transport: server_socket_thread() exiting\n");
    return 0;
}

void local_init(int port)
{
    sdb_thread_t thr;
    void* (*func)(void *);

    if(HOST) {
        func = client_socket_thread;
    } else {
        func = server_socket_thread;
    }

    D("transport: local %s init\n", HOST ? "client" : "server");

    if(sdb_thread_create(&thr, func, (void *)port)) {
        fatal_errno("cannot create local socket %s thread",
                    HOST ? "client" : "server");
    }
}

static void remote_kick(atransport *t)
{
    int fd = t->sfd;
    t->sfd = -1;
    sdb_shutdown(fd);
    sdb_close(fd);

#if SDB_HOST
    if(HOST) {
        int  nn;
        sdb_mutex_lock( &local_transports_lock );
        for (nn = 0; nn < SDB_LOCAL_TRANSPORT_MAX; nn++) {
            if (local_transports[nn] == t) {
                local_transports[nn] = NULL;
                break;
            }
        }
        sdb_mutex_unlock( &local_transports_lock );
    }
#endif
}

static void remote_close(atransport *t)
{
    sdb_close(t->fd);
}


#if SDB_HOST
/* Only call this function if you already hold local_transports_lock. */
atransport* find_emulator_transport_by_sdb_port_locked(int sdb_port)
{
    int i;
    for (i = 0; i < SDB_LOCAL_TRANSPORT_MAX; i++) {
        if (local_transports[i] && local_transports[i]->sdb_port == sdb_port) {
            return local_transports[i];
        }
    }
    return NULL;
}

atransport* find_emulator_transport_by_sdb_port(int sdb_port)
{
    sdb_mutex_lock( &local_transports_lock );
    atransport* result = find_emulator_transport_by_sdb_port_locked(sdb_port);
    sdb_mutex_unlock( &local_transports_lock );
    return result;
}

/* Only call this function if you already hold local_transports_lock. */
int get_available_local_transport_index_locked()
{
    int i;
    for (i = 0; i < SDB_LOCAL_TRANSPORT_MAX; i++) {
        if (local_transports[i] == NULL) {
            return i;
        }
    }
    return -1;
}

int get_available_local_transport_index()
{
    sdb_mutex_lock( &local_transports_lock );
    int result = get_available_local_transport_index_locked();
    sdb_mutex_unlock( &local_transports_lock );
    return result;
}
#endif

int init_socket_transport(atransport *t, int s, int sdb_port, int local)
{
    int  fail = 0;

    t->kick = remote_kick;
    t->close = remote_close;
    t->read_from_remote = remote_read;
    t->write_to_remote = remote_write;
    t->sfd = s;
    t->sync_token = 1;
    t->connection_state = CS_OFFLINE;
    t->type = kTransportLocal;
    t->sdb_port = 0;

#if SDB_HOST
    if (HOST && local) {
        sdb_mutex_lock( &local_transports_lock );
        {
            t->sdb_port = sdb_port;
            atransport* existing_transport =
                    find_emulator_transport_by_sdb_port_locked(sdb_port);
            int index = get_available_local_transport_index_locked();
            if (existing_transport != NULL) {
                D("local transport for port %d already registered (%p)?\n",
                sdb_port, existing_transport);
                fail = -1;
            } else if (index < 0) {
                // Too many emulators.
                D("cannot register more emulators. Maximum is %d\n",
                        SDB_LOCAL_TRANSPORT_MAX);
                fail = -1;
            } else {
                local_transports[index] = t;
            }
       }
       sdb_mutex_unlock( &local_transports_lock );
    }
#endif
    return fail;
}
