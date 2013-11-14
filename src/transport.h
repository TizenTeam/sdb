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

#ifndef __TRANSPORT_H
#define __TRANSPORT_H

#include "common_modules.h"

#define  LOCAL_CLIENT_PREFIX  "emulator-"

#define A_CNXN 0x4e584e43
#define A_OPEN 0x4e45504f
#define A_OKAY 0x59414b4f
#define A_CLSE 0x45534c43
#define A_WRTE 0x45545257
#define A_TCLS 0x534C4354
#define A_STAT 0x54415453

#define CS_NONE      -10000
#define CS_ANY       -1
#define CS_OFFLINE    0
#define CS_BOOTLOADER 1
#define CS_DEVICE     2
#define CS_HOST       3
#define CS_RECOVERY   4
#define CS_SIDELOAD   6
#define CS_WAITCNXN   7
#define CS_PWLOCK     10

#define DEFAULT_SDB_LOCAL_TRANSPORT_PORT 26101
#define  SDB_LOCAL_TRANSPORT_MAX  15

extern LIST_NODE* transport_list;
extern int current_local_transports;

#ifdef _WIN32 /* FIXME : move to sysdeps.h later */
int asprintf( char **, char *, ... );
int vasprintf( char **, char *, va_list );
#endif

void wakeup_select_func(int _fd, unsigned ev, void *data);
void dump_packet(const char* name, const char* func, PACKET* p);
void send_packet(PACKET *p, TRANSPORT *t);
const char *connection_state_name(TRANSPORT *t);
PACKET *get_apacket(void);
void put_apacket(void *p);
void register_socket_transport(int s, const char *serial, int port, int local, const char *device_name);
void register_usb_transport(usb_handle *usb, const char *serial);
int register_device_con_transport(int s, const char *serial);
void send_cmd(unsigned arg0, unsigned arg1, unsigned cmd, char* data, TRANSPORT* t);
void close_usb_devices();
int list_transports_msg(char*  buffer, size_t  bufferlen);
int list_transports(char *buf, size_t  bufsize);
int local_connect(int  port, const char *device_name);
TRANSPORT *acquire_one_transport(transport_type ttype, const char* serial, char **error_out);
void kick_transport( TRANSPORT*  t );
void register_transport(TRANSPORT *t);
#endif   /* __TRANSPORT_H */
