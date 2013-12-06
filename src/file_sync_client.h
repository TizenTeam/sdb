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
* See the License for the specific language governing permissions andㄴ
* limitations under the License.
*
* Contributors:
* - S-Core Co., Ltd
*
*/

#ifndef FILE_SYNC_CLIENT_H_
#define FILE_SYNC_CLIENT_H_

#include "file_sync_functions.h"

void create_copy_info(COPY_INFO** info, char* srcp, char* dstp);
int do_sync_copy(char* srcp, char* dstp, FILE_FUNC* srcF, FILE_FUNC* dstF, int is_utf8);

#endif /* FILE_SYNC_CLIENT_H_ */
