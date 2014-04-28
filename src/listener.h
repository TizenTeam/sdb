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

#ifndef LISTENER_H_
#define LISTENER_H_

#include "common_modules.h"
extern LIST_NODE* listener_list;

int install_listener(int local_port, int connect_port, TRANSPORT* transport, LISTENER_TYPE ltype);
//int remove_listener(int local_port, int connect_port, TRANSPORT* transport);
int remove_listener(int local_port);
void  free_listener(void* data);

#endif /* LISTENER_H_ */
