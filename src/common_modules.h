/*
* SDB - Smart Development Bridge
*
* Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
*
* Contact:
* Ho Namkoong <ho.namkoong@samsung.com>
* Yoonki Park <yoonki.park@samsung.com>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* Contributors:
* - S-Core Co., Ltd
*
*/

#ifndef COMMON_MODULES_H_
#define COMMON_MODULES_H_

#include "linkedlist.h"
#include "fdevent.h"
#include "sdb_usb.h"

#define MAX_PAYLOAD 4096
#define CHUNK_SIZE (64*1024)
#define DEFAULT_SDB_QEMU_PORT 26097
#define DEFAULT_SDB_PORT 26099

#define A_VERSION 0x0100000
#define SDB_VERSION_MAJOR   2       // increments upon significant architectural changes or the achievement of important milestones
#define SDB_VERSION_MINOR   2       // progress is made within a major version
#define SDB_VERSION_PATCH   53      // increments for small sets of changes
#define SDB_VERSION_MAX_LENGTH  128

extern MAP hex_map;

typedef struct message MESSAGE;
struct message {
    unsigned command;       /* command identifier constant      */
    unsigned arg0;          /* first argument                   */
    unsigned arg1;          /* second argument                  */
    unsigned data_length;   /* length of payload (0 is allowed) */
    unsigned data_check;    /* checksum of data payload         */
    unsigned magic;         /* command ^ 0xffffffff             */
};

typedef struct packet PACKET;
struct packet
{
    LIST_NODE* node;

    unsigned len;
    void *ptr;

    MESSAGE msg;
    unsigned char data[MAX_PAYLOAD];
};

typedef enum transport_type {
        kTransportUsb,
        kTransportLocal,
        kTransportAny,
        kTransportConnect,
        //TODO REMOTE_DEVICE_CONNECT
        //kTransportRemoteDevCon
} transport_type;

typedef struct transport TRANSPORT;
struct transport
{
    LIST_NODE* node;
    //TODO REMOTE_DEVICE_CONNECT
    //list for remote sockets which wait for CNXN
    //LIST_NODE* remote_cnxn_socket;

    int (*read_from_remote)(TRANSPORT* t, void* data, int len);
    int (*write_to_remote)(PACKET *p, TRANSPORT *t);
    void (*close)(TRANSPORT *t);
    void (*kick)(TRANSPORT *t);

    int connection_state;

    //for checking emulator suspended mode
    int suspended;
    transport_type type;

    usb_handle *usb;
    int sfd;

    char *serial;
    char host[20];
    int sdb_port;
    char *device_name;

    int          kicked;
    unsigned req;
    unsigned res;
};

typedef enum listener_type {
        serverListener,
        qemuListener,
        forwardListener
} LISTENER_TYPE;

typedef struct listener LISTENER;
struct listener
{
    LIST_NODE* node;

    FD_EVENT fde;
    int fd;

    int local_port;
    int connect_port;
    TRANSPORT *transport;
    LISTENER_TYPE type;
};

typedef struct t_packet T_PACKET;
struct t_packet {
    TRANSPORT* t;
    PACKET* p;
};

typedef struct socket SDB_SOCKET;
struct socket {
    int status;
    LIST_NODE* node;

    unsigned local_id;
    unsigned remote_id;

    int    closing;
    FD_EVENT fde;
    int fd;

    LIST_NODE* pkt_list;
    TRANSPORT *transport;

    PACKET* read_packet;
};

int readx(int fd, void *ptr, size_t len);
int writex(int fd, const void *ptr, size_t len);
int notify_qemu(char* host, int port, char* serial);

#endif /* SDB_TYPES_H_ */
