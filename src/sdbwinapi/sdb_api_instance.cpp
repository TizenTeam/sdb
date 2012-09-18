/*
 * Copyright (C) 2006 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \file
  This file consists of implementation of class SdbApiInstance that is a main
  API object representing a device interface that is in the interest of
  the API client. All device (interface) related operations go through this
  class first.
*/

#include "stdafx.h"
#include "sdb_api_instance.h"
#include "sdb_helper_routines.h"

/// Map that holds all instances of this object
SdbApiInstanceMap sdb_app_instance_map;
ULONG_PTR sdb_app_instance_id = 0;
CComAutoCriticalSection sdb_app_instance_map_locker;

SdbApiInstance::SdbApiInstance()
    : ref_count_(1) {
  // Generate inteface handle
  sdb_app_instance_map_locker.Lock();
  sdb_app_instance_id++;
  sdb_app_instance_map_locker.Unlock();
  instance_handle_ =
    reinterpret_cast<SDBAPIINSTANCEHANDLE>(sdb_app_instance_id);
}

SdbApiInstance::~SdbApiInstance() {
}

void SdbApiInstance::LastReferenceReleased() {
}
