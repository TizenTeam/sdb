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
/* the list of mutexes used by sdb */
/* #ifndef __MUTEX_LIST_H
 * Do not use an include-guard. This file is included once to declare the locks
 * and once in win32 to actually do the runtime initialization.
 */
#ifndef SDB_MUTEX
#error SDB_MUTEX not defined when including this file
#endif
SDB_MUTEX(dns_lock)
SDB_MUTEX(socket_list_lock)
SDB_MUTEX(transport_lock)
#if SDB_HOST
SDB_MUTEX(local_transports_lock)
#endif
SDB_MUTEX(usb_lock)

// Sadly logging to /data/sdb/sdb-... is not thread safe.
//  After modifying sdb.h::D() to count invocations:
//   DEBUG(jpa):0:Handling main()
//   DEBUG(jpa):1:[ usb_init - starting thread ]
// (Oopsies, no :2:, and matching message is also gone.)
//   DEBUG(jpa):3:[ usb_thread - opening device ]
//   DEBUG(jpa):4:jdwp control socket started (10)
SDB_MUTEX(D_lock)

#undef SDB_MUTEX
