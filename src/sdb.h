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

#ifndef __SDB_H
#define __SDB_H

#include <limits.h>

#include "linkedlist.h"
#include "transport.h"  /* readx(), writex() */
#include "sdb_usb.h"
#include "log.h"
#include "sdb_map.h"

int sdb_main(int is_daemon, int server_port);

unsigned host_to_le32(unsigned n);
int sdb_commandline(int argc, char **argv);

int connection_state(TRANSPORT *t);

extern int SHELL_EXIT_NOTIFY_FD;

void sdb_cleanup(void);

#endif
