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

#ifndef SOCKETS_H_
#define SOCKETS_H_

#include "common_modules.h"

#define SET_SOCKET_STATUS(asocket, _status) ((asocket)->status |= (1 << _status))
#define REMOVE_SOCKET_STATUS(asocket, _status) ((asocket)->status &= ~(1 << _status))
#define HAS_SOCKET_STATUS(asocket, _status) ((asocket)->status & (1 << _status))

extern const unsigned int unsigned_int_bit;
extern const unsigned int remote_con_right_padding;
extern const unsigned int remote_con_flag;
extern unsigned int remote_con_cur_r_id;
extern unsigned int remote_con_cur_l_number;
extern const unsigned int remote_con_l_max; // Ox1111
extern const unsigned int remote_con_r_max;
extern unsigned int remote_con_l_table[16];

typedef enum {
    NOTIFY = 0,
    DEVICE_TRACKER,
    REMOTE_SOCKET,
    REMOTE_CON,
    QEMU_SOCKET
} SOCKET_STATUS;

extern LIST_NODE* local_socket_list;

SDB_SOCKET *find_local_socket(unsigned id);
int local_socket_enqueue(SDB_SOCKET *s, PACKET *p);
void local_socket_ready(SDB_SOCKET *s);
void local_socket_close(SDB_SOCKET *s);
SDB_SOCKET *create_local_socket(int fd);
void connect_to_remote(SDB_SOCKET *s, const char* destination);
int assign_remote_connect_socket_rid (SDB_SOCKET* s);
int device_tracker_send( SDB_SOCKET* local_socket, const char* buffer, int len );
#endif /* SOCKETS_H_ */
