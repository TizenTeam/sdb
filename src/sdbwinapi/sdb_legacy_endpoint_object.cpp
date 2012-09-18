/*
 * Copyright (C) 2009 The Android Open Source Project
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
  This file consists of implementation of class SdbLegacyEndpointObject that
  encapsulates a handle opened to an endpoint on our device controlled by
  a custom (legacy) USB driver.
*/

#include "stdafx.h"
#include "sdb_api_legacy.h"
#include "sdb_legacy_endpoint_object.h"
#include "sdb_legacy_io_completion.h"
#include "sdb_helper_routines.h"

SdbLegacyEndpointObject::SdbLegacyEndpointObject(
    SdbLegacyInterfaceObject* parent_interf,
    UCHAR endpoint_id,
    UCHAR endpoint_index)
    : SdbEndpointObject(parent_interf, endpoint_id, endpoint_index),
      usb_handle_(INVALID_HANDLE_VALUE) {
}

SdbLegacyEndpointObject::~SdbLegacyEndpointObject() {
  if (INVALID_HANDLE_VALUE != usb_handle_) {
    ::CloseHandle(usb_handle_);
  }
}

SDBAPIHANDLE SdbLegacyEndpointObject::CommonAsyncReadWrite(
    bool is_read,
    void* buffer,
    ULONG bytes_to_transfer,
    ULONG* bytes_transferred,
    HANDLE event_handle,
    ULONG time_out) {
  if (NULL != bytes_transferred) {
    *bytes_transferred = 0;
  }

  if (!IsOpened()) {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }

  bool is_ioctl_write = is_read ? false : (0 != time_out);

  // Create completion i/o object
  SdbLegacyIOCompletion* sdb_io_completion = NULL;

  try {
    sdb_io_completion = new SdbLegacyIOCompletion(this,
                                                  bytes_to_transfer,
                                                  event_handle,
                                                  is_ioctl_write);
  } catch (... ) {
    // We don't expect exceptions other than OOM thrown here.
    SetLastError(ERROR_OUTOFMEMORY);
    return NULL;
  }

  // Create a handle for it
  SDBAPIHANDLE ret = sdb_io_completion->CreateHandle();
  ULONG transferred = 0;
  if (NULL != ret) {
    BOOL res = TRUE;
    if (0 == time_out) {
      // Go the read / write file way
      res = is_read ? ReadFile(usb_handle(),
                               buffer,
                               bytes_to_transfer,
                               &transferred,
                               sdb_io_completion->overlapped()) :
                      WriteFile(usb_handle(),
                                buffer,
                                bytes_to_transfer,
                                &transferred,
                                sdb_io_completion->overlapped());
    } else {
      // Go IOCTL way
      SdbBulkTransfer transfer_param;
      transfer_param.time_out = time_out;
      transfer_param.transfer_size = is_read ? 0 : bytes_to_transfer;
      transfer_param.SetWriteBuffer(is_read ? NULL : buffer);

      res = DeviceIoControl(usb_handle(),
        is_read ? SDB_IOCTL_BULK_READ : SDB_IOCTL_BULK_WRITE,
        &transfer_param, sizeof(transfer_param),
        is_read ? buffer : sdb_io_completion->transferred_bytes_ptr(),
        is_read ? bytes_to_transfer : sizeof(ULONG),
        &transferred,
        sdb_io_completion->overlapped());
    }

    if (NULL != bytes_transferred) {
      *bytes_transferred = transferred;
    }

    ULONG error = GetLastError();
    if (!res && (ERROR_IO_PENDING != error)) {
      // I/O failed immediatelly. We need to close i/o completion object
      // before we return NULL to the caller.
      sdb_io_completion->CloseHandle();
      ret = NULL;
      SetLastError(error);
    }
  }

  // Offseting 'new'
  sdb_io_completion->Release();

  return ret;
}

bool SdbLegacyEndpointObject::CommonSyncReadWrite(bool is_read,
                                                  void* buffer,
                                                  ULONG bytes_to_transfer,
                                                  ULONG* bytes_transferred,
                                                  ULONG time_out) {
  if (NULL != bytes_transferred) {
    *bytes_transferred = 0;
  }

  if (!IsOpened()) {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }

  bool is_ioctl_write = is_read ? false : (0 != time_out);

  // This is synchronous I/O. Since we always open I/O items for
  // overlapped I/O we're obligated to always provide OVERLAPPED
  // structure to read / write routines. Prepare it now.
  OVERLAPPED overlapped;
  ZeroMemory(&overlapped, sizeof(overlapped));

  BOOL ret = TRUE;
  ULONG ioctl_write_transferred = 0;
  if (0 == time_out) {
    // Go the read / write file way
    ret = is_read ?
      ReadFile(usb_handle(), buffer, bytes_to_transfer, bytes_transferred, &overlapped) :
      WriteFile(usb_handle(), buffer, bytes_to_transfer, bytes_transferred, &overlapped);
  } else {
    // Go IOCTL way
    SdbBulkTransfer transfer_param;
    transfer_param.time_out = time_out;
    transfer_param.transfer_size = is_read ? 0 : bytes_to_transfer;
    transfer_param.SetWriteBuffer(is_read ? NULL : buffer);

    ULONG tmp;
    ret = DeviceIoControl(usb_handle(),
      is_read ? SDB_IOCTL_BULK_READ : SDB_IOCTL_BULK_WRITE,
      &transfer_param, sizeof(transfer_param),
      is_read ? buffer : &ioctl_write_transferred,
      is_read ? bytes_to_transfer : sizeof(ULONG),
      &tmp,
      &overlapped);
  }

  // Lets see the result
  if (!ret && (ERROR_IO_PENDING != GetLastError())) {
    // I/O failed.
    return false;
  }

  // Lets wait till I/O completes
  ULONG transferred = 0;
  ret = GetOverlappedResult(usb_handle(), &overlapped, &transferred, TRUE);
  if (ret && (NULL != bytes_transferred)) {
    *bytes_transferred = is_ioctl_write ? ioctl_write_transferred :
                                          transferred;
  }

  return ret ? true : false;
}

SDBAPIHANDLE SdbLegacyEndpointObject::CreateHandle(
    const wchar_t* item_path,
    SdbOpenAccessType access_type,
    SdbOpenSharingMode share_mode) {
  // Convert access / share parameters into CreateFile - compatible
  ULONG desired_access;
  ULONG desired_sharing;

  if (!GetSDKComplientParam(access_type, share_mode,
                            &desired_access, &desired_sharing)) {
    return NULL;
  }

  // Open USB handle
  usb_handle_ = CreateFile(item_path,
                           desired_access,
                           share_mode,
                           NULL,
                           OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED,  // Always overlapped!
                           NULL);
  if (INVALID_HANDLE_VALUE == usb_handle_) {
    return NULL;
  }

  // Create SDB handle
  SDBAPIHANDLE ret = SdbObjectHandle::CreateHandle();

  if (NULL == ret) {
    // If creation of SDB handle failed we have to close USB handle too.
    ULONG error = GetLastError();
    ::CloseHandle(usb_handle());
    usb_handle_ = INVALID_HANDLE_VALUE;
    SetLastError(error);
  }

  return ret;
}

bool SdbLegacyEndpointObject::CloseHandle() {
  if (INVALID_HANDLE_VALUE != usb_handle_) {
    ::CloseHandle(usb_handle_);
    usb_handle_ = INVALID_HANDLE_VALUE;
  }

  return SdbEndpointObject::CloseHandle();
}
