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

#ifndef _SDB_CLIENT_H_
#define _SDB_CLIENT_H_

#include "sdb.h"
#include "sdb_constants.h"

/* connect to sdb, connect to the named service, and return
** a valid fd for interacting with that service upon success
** or a negative number on failure
*/
int sdb_connect(const char *service, void** ext_args);
int _sdb_connect(const char *service, void** ext_args);

/* connect to sdb, connect to the named service, return 0 if
** the connection succeeded AND the service returned OKAY
*/
int sdb_command(const char *service, void** ext_args);

/* connect to sdb, connect to the named service, return
** a malloc'd string of its response upon success or NULL
** on failure.
*/

char *sdb_query(const char *service, void** ext_args);

/* return verbose error string from last operation */
const char *sdb_error(void);

/* read a standard sdb status response (OKAY|FAIL) and
** return 0 in the event of OKAY, -1 in the event of FAIL
** or protocol error
*/
int __inline__ read_msg_size(int fd);
int __inline__ write_msg_size(int fd, int size);

void get_host_prefix(char* prefix, int size, transport_type ttype, const char* serial, HOST_TYPE host_type);
#endif
