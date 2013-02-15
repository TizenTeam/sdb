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

/* connect to sdb, connect to the named service, and return
** a valid fd for interacting with that service upon success
** or a negative number on failure
*/
int sdb_connect(const char *service);
int _sdb_connect(const char *service);

/* connect to sdb, connect to the named service, return 0 if
** the connection succeeded AND the service returned OKAY
*/
int sdb_command(const char *service);

/* connect to sdb, connect to the named service, return
** a malloc'd string of its response upon success or NULL
** on failure.
*/

char *sdb_query(const char *service);

/* Set the preferred transport to connect to.
*/
void sdb_set_transport(transport_type type, const char* serial);

/* Set TCP specifics of the transport to use
*/
void sdb_set_tcp_specifics(int server_port);

/* Return the console port of the currently connected emulator (if any)
 * of -1 if there is no emulator, and -2 if there is more than one.
 * assumes sdb_set_transport() was alled previously...
 */
int  sdb_get_emulator_console_port(void);

/* send commands to the current emulator instance. will fail if there
 * is zero, or more than one emulator connected (or if you use -s <serial>
 * with a <serial> that does not designate an emulator)
 */
int  sdb_send_emulator_command(int  argc, char**  argv);

/* return verbose error string from last operation */
const char *sdb_error(void);

/* read a standard sdb status response (OKAY|FAIL) and
** return 0 in the event of OKAY, -1 in the event of FAIL
** or protocol error
*/
int sdb_status(int fd);

#endif
