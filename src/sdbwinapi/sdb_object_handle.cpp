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
  This file consists of implementation of a class SdbObjectHandle that
  encapsulates an internal API object that is visible to the outside
  of the API through a handle.
*/

#include "stdafx.h"
#include "sdb_api.h"
#include "sdb_object_handle.h"

/// Global SDBAPIHANDLE -> SdbObjectHandle* map
SdbObjectHandleMap      the_map;

/// Locker for the SdbObjectHandleMap instance
CComAutoCriticalSection the_map_locker;

/// Next sdb handle value generator
ULONG_PTR               next_sdb_handle_value = 0;

SdbObjectHandle::SdbObjectHandle(SdbObjectType obj_type)
    : sdb_handle_(NULL),
      object_type_(obj_type),
      ref_count_(1) {
  ATLASSERT(obj_type < SdbObjectTypeMax);
}

SdbObjectHandle::~SdbObjectHandle() {
  ATLASSERT(0 == ref_count_);
  ATLASSERT(NULL == sdb_handle_);
}

LONG SdbObjectHandle::AddRef() {
  ATLASSERT(ref_count_ > 0);
  return InterlockedIncrement(&ref_count_);
}

LONG SdbObjectHandle::Release() {
  ATLASSERT(ref_count_ > 0);
  LONG ret = InterlockedDecrement(&ref_count_);
  ATLASSERT(ret >= 0);
  if (0 == ret) {
    LastReferenceReleased();
    delete this;
  }
  return ret;
}

SDBAPIHANDLE SdbObjectHandle::CreateHandle() {
  SDBAPIHANDLE ret = NULL;

  // We have to hold this lock while we're dealing with the handle
  // and the table
  the_map_locker.Lock();
  
  ATLASSERT(!IsOpened());

  if (!IsOpened()) {
    try {
      // Generate next handle value
      next_sdb_handle_value++;
      ret = reinterpret_cast<SDBAPIHANDLE>(next_sdb_handle_value);

      // Add ourselves to the map
      the_map[ret] = this;

      // Save handle, addref and return
      sdb_handle_ = ret;
      AddRef();
    } catch (...) {
      ret = NULL;
      SetLastError(ERROR_OUTOFMEMORY);
    }
  } else {
    // Signaling that this object is already opened
    SetLastError(ERROR_GEN_FAILURE);
  }

  the_map_locker.Unlock();

  return ret;
}

bool SdbObjectHandle::CloseHandle() {
  bool ret = false;

  // Addref just in case that last reference to this object is being
  // held in the map
  AddRef();

  the_map_locker.Lock();
  
  ATLASSERT(IsOpened());

  if (IsOpened()) {
    try {
      // Look us up in the map.
      SdbObjectHandleMap::iterator found = the_map.find(sdb_handle());
      ATLASSERT((found != the_map.end()) && (this == found->second));

      if ((found != the_map.end()) && (this == found->second)) {
        // Remove ourselves from the map, close and release the object
        the_map.erase(found);
        sdb_handle_ = NULL;
        Release();
        ret = true;
      } else {
        SetLastError(ERROR_INVALID_HANDLE);
      }
    } catch (...) {
      ret = false;
      SetLastError(ERROR_OUTOFMEMORY);
    }
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
  }

  the_map_locker.Unlock();

  Release();

  return ret;
}

bool SdbObjectHandle::IsObjectOfType(SdbObjectType obj_type) const {
  return (obj_type == object_type());
}

void SdbObjectHandle::LastReferenceReleased() {
  ATLASSERT(!IsOpened());
}

SdbObjectHandle* SdbObjectHandle::Lookup(SDBAPIHANDLE sdb_hndl) {
  SdbObjectHandle* ret = NULL;

  the_map_locker.Lock();

  try {
    // Look us up in the map.
    SdbObjectHandleMap::iterator found = the_map.find(sdb_hndl);
    if (found != the_map.end()) {
      ret = found->second;
      ret->AddRef();
    }
  } catch (...) {
    SetLastError(ERROR_OUTOFMEMORY);
  }

  the_map_locker.Unlock();

  return ret;
}
