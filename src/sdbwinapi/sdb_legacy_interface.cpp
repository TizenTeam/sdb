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
  This file consists of implementation of class SdbLegacyInterfaceObject
  that encapsulates an interface on our USB device that is accessible
*/

#include "stdafx.h"
#include "sdb_api_legacy.h"
#include "sdb_legacy_interface.h"
#include "sdb_legacy_endpoint_object.h"

SdbLegacyInterfaceObject::SdbLegacyInterfaceObject(const wchar_t* interf_name)
    : SdbInterfaceObject(interf_name),
      def_read_endpoint_(0xFF),
      read_endpoint_id_(0xFF),
      def_write_endpoint_(0xFF),
      write_endpoint_id_(0xFF) {
}

SdbLegacyInterfaceObject::~SdbLegacyInterfaceObject() {
}

SDBAPIHANDLE SdbLegacyInterfaceObject::CreateHandle() {
  // Open USB device for this intefface
  HANDLE usb_device_handle = CreateFile(interface_name().c_str(),
                                        GENERIC_READ | GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL,
                                        OPEN_EXISTING,
                                        0,
                                        NULL);
  if (INVALID_HANDLE_VALUE == usb_device_handle) {
    return NULL;
  }

  // Now, we ensured that our usb device / interface is up and running.
  // Lets collect device, interface and pipe information
  bool ok = true;
  if (!CacheUsbDeviceDescriptor(usb_device_handle) ||
      !CacheUsbConfigurationDescriptor(usb_device_handle) ||
      !CacheUsbInterfaceDescriptor(usb_device_handle)) {
    ok = false;
  }

  // Preserve error accross handle close
  ULONG error = ok ? NO_ERROR : GetLastError();

  ::CloseHandle(usb_device_handle);

  if (NO_ERROR != error) {
    SetLastError(error);
  }

  if (!ok) {
    return false;
  }

  // Save indexes and IDs for bulk read / write endpoints. We will use them to
  // convert SDB_QUERY_BULK_WRITE_ENDPOINT_INDEX and
  // SDB_QUERY_BULK_READ_ENDPOINT_INDEX into actual endpoint indexes and IDs.
  for (UCHAR endpoint = 0; endpoint < usb_interface_descriptor_.bNumEndpoints;
       endpoint++) {
    // Get endpoint information
    SdbEndpointInformation pipe_info;
    if (!GetEndpointInformation(endpoint, &pipe_info)) {
      return false;
    }

    if (SdbEndpointTypeBulk == pipe_info.endpoint_type) {
      // This is a bulk endpoint. Cache its index and ID.
      if (0 != (pipe_info.endpoint_address & USB_ENDPOINT_DIRECTION_MASK)) {
        // Use this endpoint as default bulk read endpoint
        ATLASSERT(0xFF == def_read_endpoint_);
        def_read_endpoint_ = endpoint;
        read_endpoint_id_ = pipe_info.endpoint_address;
      } else {
        // Use this endpoint as default bulk write endpoint
        ATLASSERT(0xFF == def_write_endpoint_);
        def_write_endpoint_ = endpoint;
        write_endpoint_id_ = pipe_info.endpoint_address;
      }
    }
  }

  return SdbObjectHandle::CreateHandle();
}

bool SdbLegacyInterfaceObject::GetSerialNumber(void* buffer,
                                               unsigned long* buffer_char_size,
                                               bool ansi) {
  if (!IsOpened()) {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }

  // Open USB device for this intefface
  HANDLE usb_device_handle = CreateFile(interface_name().c_str(),
                                        GENERIC_READ,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL,
                                        OPEN_EXISTING,
                                        0,
                                        NULL);
  if (INVALID_HANDLE_VALUE == usb_device_handle) {
    return NULL;
  }

  WCHAR serial_number[512];

  // Send IOCTL
  DWORD ret_bytes = 0;
  BOOL ret = DeviceIoControl(usb_device_handle,
                             SDB_IOCTL_GET_SERIAL_NUMBER,
                             NULL, 0,
                             serial_number, sizeof(serial_number),
                             &ret_bytes,
                             NULL);

  // Preserve error accross CloseHandle
  ULONG error = ret ? NO_ERROR : GetLastError();

  ::CloseHandle(usb_device_handle);

  if (NO_ERROR != error) {
    SetLastError(error);
    return false;
  }

  unsigned long str_len =
    static_cast<unsigned long>(wcslen(serial_number) + 1);

  if ((NULL == buffer) || (*buffer_char_size < str_len)) {
    *buffer_char_size = str_len;
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return false;
  }

  if (!ansi) {
    // If user asked for wide char name just return it
    wcscpy(reinterpret_cast<wchar_t*>(buffer), serial_number);
    return true;
  }

  // We need to convert name from wide char to ansi string
  int res = WideCharToMultiByte(CP_ACP,
                                0,
                                serial_number,
                                static_cast<int>(str_len),
                                reinterpret_cast<PSTR>(buffer),
                                static_cast<int>(*buffer_char_size),
                                NULL,
                                NULL);
  return (res != 0);
}

bool SdbLegacyInterfaceObject::GetEndpointInformation(
    UCHAR endpoint_index,
    SdbEndpointInformation* info) {
  // Open USB device for this intefface
  HANDLE usb_device_handle = CreateFile(interface_name().c_str(),
                                        GENERIC_READ,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL,
                                        OPEN_EXISTING,
                                        0,
                                        NULL);
  if (INVALID_HANDLE_VALUE == usb_device_handle) {
    return NULL;
  }

  // Init ICTL param
  SdbQueryEndpointInformation param;
  param.endpoint_index = endpoint_index;

  // Send IOCTL
  DWORD ret_bytes = 0;
  BOOL ret = DeviceIoControl(usb_device_handle,
                             SDB_IOCTL_GET_ENDPOINT_INFORMATION,
                             &param, sizeof(param),
                             info, sizeof(SdbEndpointInformation),
                             &ret_bytes,
                             NULL);
  ATLASSERT(!ret || (sizeof(SdbEndpointInformation) == ret_bytes));

  // Preserve error accross CloseHandle
  ULONG error = ret ? NO_ERROR : GetLastError();

  ::CloseHandle(usb_device_handle);

  if (NO_ERROR != error) {
    SetLastError(error);
  }

  return ret ? true : false;
}

SDBAPIHANDLE SdbLegacyInterfaceObject::OpenEndpoint(
    UCHAR endpoint_index,
    SdbOpenAccessType access_type,
    SdbOpenSharingMode sharing_mode) {
  // Convert index into name and ID.
  std::wstring endpoint_name;
  UCHAR endpoint_id;

  try {
    if ((SDB_QUERY_BULK_READ_ENDPOINT_INDEX == endpoint_index) ||
        (def_read_endpoint_ == endpoint_index)) {
      endpoint_name = DEVICE_BULK_READ_PIPE_NAME;
      endpoint_id = read_endpoint_id_;
      endpoint_index = def_read_endpoint_;
    } else if ((SDB_QUERY_BULK_WRITE_ENDPOINT_INDEX == endpoint_index) ||
               (def_write_endpoint_ == endpoint_index)) {
      endpoint_name = DEVICE_BULK_WRITE_PIPE_NAME;
      endpoint_id = write_endpoint_id_;
      endpoint_index = def_write_endpoint_;
    } else {
      SetLastError(ERROR_INVALID_PARAMETER);
      return false;
    }
  } catch (...) {
    // We don't expect exceptions other than OOM thrown here.
    SetLastError(ERROR_OUTOFMEMORY);
    return NULL;
  }

  return OpenEndpoint(endpoint_name.c_str(), endpoint_id, endpoint_index,
                      access_type, sharing_mode);
}

SDBAPIHANDLE SdbLegacyInterfaceObject::OpenEndpoint(
    const wchar_t* endpoint_name,
    UCHAR endpoint_id,
    UCHAR endpoint_index,
    SdbOpenAccessType access_type,
    SdbOpenSharingMode sharing_mode) {
  if (!IsOpened()) {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }

  SdbLegacyEndpointObject* sdb_endpoint = NULL;

  try {
    sdb_endpoint =
        new SdbLegacyEndpointObject(this, endpoint_id, endpoint_index);
  } catch (...) {
    // We don't expect exceptions other than OOM thrown here.
    SetLastError(ERROR_OUTOFMEMORY);
    return NULL;
  }

  // Build full path to the object
  std::wstring endpoint_path = interface_name();
  endpoint_path += L"\\";
  endpoint_path += endpoint_name;

  SDBAPIHANDLE ret = sdb_endpoint->CreateHandle(endpoint_path.c_str(),
                                                access_type,
                                                sharing_mode);

  sdb_endpoint->Release();

  return ret;
}

bool SdbLegacyInterfaceObject::CacheUsbDeviceDescriptor(
    HANDLE usb_device_handle) {
  DWORD ret_bytes = 0;
  BOOL ret = DeviceIoControl(usb_device_handle,
                             SDB_IOCTL_GET_USB_DEVICE_DESCRIPTOR,
                             NULL, 0,
                             &usb_device_descriptor_,
                             sizeof(usb_device_descriptor_),
                             &ret_bytes,
                             NULL);
  ATLASSERT(!ret || (sizeof(USB_DEVICE_DESCRIPTOR) == ret_bytes));

  return ret ? true : false;
}

bool SdbLegacyInterfaceObject::CacheUsbConfigurationDescriptor(
    HANDLE usb_device_handle) {
  DWORD ret_bytes = 0;
  BOOL ret = DeviceIoControl(usb_device_handle,
                             SDB_IOCTL_GET_USB_CONFIGURATION_DESCRIPTOR,
                             NULL, 0,
                             &usb_config_descriptor_,
                             sizeof(usb_config_descriptor_),
                             &ret_bytes,
                             NULL);
  ATLASSERT(!ret || (sizeof(USB_CONFIGURATION_DESCRIPTOR) == ret_bytes));

  return ret ? true : false;
}

bool SdbLegacyInterfaceObject::CacheUsbInterfaceDescriptor(
    HANDLE usb_device_handle) {
  DWORD ret_bytes = 0;
  BOOL ret = DeviceIoControl(usb_device_handle,
                             SDB_IOCTL_GET_USB_INTERFACE_DESCRIPTOR,
                             NULL, 0,
                             &usb_interface_descriptor_,
                             sizeof(usb_interface_descriptor_),
                             &ret_bytes,
                             NULL);
  ATLASSERT(!ret || (sizeof(USB_INTERFACE_DESCRIPTOR) == ret_bytes));

  return ret ? true : false;
}
