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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#ifndef OS_WINDOWS
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#endif

#define  TRACE_TAG  TRACE_TRANSPORT
#include "strutils.h"
#include "log.h"
#include "common_modules.h"
#include "fdevent.h"
#include "utils.h"
#include "transport.h"

/* we keep a list of opened transports. The atransport struct knows to which
 * local transport it is connected. The list is used to detect when we're
 * trying to connect twice to a given local transport.
 */

int current_local_transports = 0;

static int get_devicename_from_shdmem(int port, char *device_name);

static int remote_read(TRANSPORT* t, void* data, int len) {
    return readx(t->sfd, data, len);
}

static int remote_write(PACKET *p, TRANSPORT *t) {
    dump_packet(t->serial, "remote_write_local", p);
    if(writex(t->sfd, &p->msg, sizeof(MESSAGE) + p->msg.data_length)) {
        LOG_ERROR("remote local: write terminated\n");
        return -1;
    }
    return 0;
}

static int notify_sensord(int sdb_port) {

    int  fd = -1;
    int  sensord_port = sdb_port + 2;

    fd = sdb_host_connect("127.0.0.1", sensord_port, SOCK_DGRAM);

    if (fd < 0) {
        LOG_ERROR("failed to create socket to localhost(%d)\n", sensord_port);
        return -1;
    }

    char request[16];
    snprintf(request, sizeof request, "2\n");

    // send to sensord with udp
    if (sdb_write(fd, request, strlen(request)) < 0) {
        LOG_ERROR("could not send sensord request\n");
    }

    sdb_close(fd);
    return 0;
}

int local_connect(int sdb_port, const char *device_name) {

    char buf[64];

    // in case of windows, it takes a long time to connect localhost compare to linux
#if defined(OS_WINDOWS)
    char devname[DEVICENAME_MAX]={0,};
    if (get_devicename_from_shdmem(sdb_port, devname) == -1) {
        return -1;
    }
#endif

    char* host = "127.0.0.1";
    int fd = sdb_host_connect(host, sdb_port, SOCK_STREAM);

    if (fd >= 0) {
        D("connected on remote on fd '%d', port '%d'\n", fd, sdb_port);
        close_on_exec(fd);
        disable_tcp_nagle(fd);


        snprintf(buf, sizeof buf, "%s%d", LOCAL_CLIENT_PREFIX, sdb_port);

        if(notify_qemu(host, sdb_port, buf)) {
            return -1;
        }

        register_socket_transport(fd, buf, host, sdb_port, kTransportLocal, device_name);

        // noti to sensord port to enable shell context menu on
        notify_sensord(sdb_port);
        return 0;
    }
    D("failed to connect on port '%d'\n", sdb_port);
    return -1;
}

static int get_devicename_from_shdmem(int port, char *device_name)
{
    char *vms = NULL;
#ifndef OS_WINDOWS
    int shm_id;
    void *shared_memory = (void *)0;

    shm_id = shmget( (key_t)port-1, 0, 0);
    if (shm_id == -1) {
        D("failed to get shm from key:(%d)\n", port-1);
        return -1;
    }

    shared_memory = shmat(shm_id, (void *)0, SHM_RDONLY);

    if (shared_memory == (void *)-1)
    {
        D("faild to get shdmem key (%d) : %s\n", port, strerror(errno));
        return -1;
    }
    vms = strstr((char*)shared_memory, VMS_PATH);
    if (vms != NULL) {
        s_strncpy(device_name, vms+strlen(VMS_PATH), DEVICENAME_MAX);
    } else {
        D("failed to get vm name from(%s)\n", shared_memory);
        return -1;
    }

#else /* _WIN32*/
    HANDLE hMapFile;
    char s_port[5];
    char* pBuf;

    sprintf(s_port, "%d", port-1);
    hMapFile = OpenFileMapping(FILE_MAP_READ, TRUE, s_port);

    if(hMapFile == NULL) {
        D("faild to get shdmem key from port (%d) : (%ld)\n", port, GetLastError() );
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
    if (vms != NULL) {
        s_strncpy(device_name, vms+strlen(VMS_PATH), DEVICENAME_MAX);
    } else {
        D("failed to get vm name from(%s)\n", pBuf);
        CloseHandle(hMapFile);
        return -1;
    }
    CloseHandle(hMapFile);
#endif
    // vms path should be: ~/tizen-sdk-data/emulator-vms/vms/{name}/emulimg-em1.~~
    vms = strtok(device_name, OS_PATH_SEPARATOR_STR);
    if (vms != NULL) {
        s_strncpy(device_name, vms, DEVICENAME_MAX);
    } else {
        D("failed to get vm name from(%s)\n", device_name);
        return -1;
    }
    D("init device name %s on port %d\n", device_name, port);

    return 0;
}

static void remote_kick(TRANSPORT *t)
{
    int fd = t->sfd;

//In Unix, another socket fd can be created while transport thread still uses it
#ifndef OS_WINDOWS
    t->sfd = -1;
#endif
    sdb_shutdown(fd);
    sdb_transport_close(fd);

    --current_local_transports;
}

static void remote_close(TRANSPORT *t)
{
    //nothing to close
    D("close remote socket. T(%s), device name: '%s'\n", t->serial, t->device_name);
}

void register_socket_transport(int s, const char *serial, char* host, int port, transport_type ttype, const char *device_name)
{
    if(current_local_transports >= SDB_LOCAL_TRANSPORT_MAX) {
        D("Too many emulators\n");
        sdb_close(s);
        return;
    }

    TRANSPORT *t = calloc(1, sizeof(TRANSPORT));
    char buff[32];

    if (!serial) {
        snprintf(buff, sizeof buff, "T-%p", t);
        serial = buff;
    }
    D("transport: %s init'ing for socket %d, on port %d (%s)\n", serial, s, port, device_name);
    TRANSPORT* old_t = acquire_one_transport(kTransportAny, serial, NULL);
    if(old_t != NULL) {
        D("old transport '%s' is found. Unregister it\n", old_t->serial);
        kick_transport(old_t);
    }

    t->kick = remote_kick;
    t->close = remote_close;
    t->read_from_remote = remote_read;
    t->write_to_remote = remote_write;
    t->sfd = s;
    t->connection_state = CS_OFFLINE;
    t->node = NULL;
    t->req = 0;
    t->res = 0;
    t->sdb_port = port;
    t->suspended = 0;
    t->type = ttype;
    //TODO REMOTE_DEVICE_CONNECT
//    t->remote_cnxn_socket = NULL;

    if(host) {
        snprintf(t->host, 20, "%s", host);
    }
    else {
        snprintf(t->host, 20, "%s", "127.0.0.1");
    }
    if(serial) {
        t->serial = strdup(serial);
    }

    if (device_name) {
        t->device_name = strdup(device_name);
    } else {
        // device_name could be null when sdb server was forked before qemu has sent the connect message.
        t->device_name = (char*) malloc(DEVICENAME_MAX+1);
        if (get_devicename_from_shdmem(port, t->device_name) == -1) {
            s_strncpy(t->device_name, DEFAULT_DEVICENAME, DEVICENAME_MAX);
        }
    }

    ++current_local_transports;
    register_transport(t);
}
