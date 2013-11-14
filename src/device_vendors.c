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
#include <stdio.h>
#include "device_vendors.h"
#include "log.h"


VENDOR tizen_device_vendors[] = {
    {"samsung", 0x04e8} //1256
};


void init_device_vendors(void)
{
    LOG_FIXME("should implement later\n");
    vendor_total_cnt = (sizeof(tizen_device_vendors)/sizeof(VENDOR));
}
