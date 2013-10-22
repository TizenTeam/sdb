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
#ifndef __FDEVENT_I_H
#define __FDEVENT_I_H

#include "fdevent.h"
extern FD_EVENT **fd_table;
extern int fd_table_max;

FD_EVENT *fdevent_plist_dequeue(void);
void fdevent_register(FD_EVENT *fde);
void fdevent_unregister(FD_EVENT *fde);

#endif
