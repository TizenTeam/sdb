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
  This file consists of implementation of rotines that are exported
  from this DLL.
*/

#include "stdafx.h"
#include "sdb_api.h"
#include "sdb_object_handle.h"
#include "sdb_interface_enum.h"
#include "sdb_interface.h"
#include "sdb_legacy_interface.h"
#include "sdb_endpoint_object.h"
#include "sdb_io_completion.h"
#include "sdb_helper_routines.h"
#include "sdb_winusb_api.h"

/** \brief Points to InstantiateWinUsbInterface exported from SdbWinUsbApi.dll.

  This variable is initialized with the actual address in DllMain routine for
  this DLL on DLL_PROCESS_ATTACH event.
  @see PFN_INSTWINUSBINTERFACE for more information.
*/
PFN_INSTSDBWINUSBINTERFACE InstantiateSdbWinUsbInterface = NULL;

SDBAPIHANDLE __cdecl SdbEnumInterfaces(GUID class_id,
                               bool exclude_not_present,
                               bool exclude_removed,
                               bool active_only) {
  SdbInterfaceEnumObject* enum_obj = NULL;
  SDBAPIHANDLE ret = NULL;

  try {
    // Instantiate and initialize enum object
    enum_obj = new SdbInterfaceEnumObject();

    if (enum_obj->InitializeEnum(class_id,
                                 exclude_not_present,
                                 exclude_removed,
                                 active_only)) {
      // After successful initialization we can create handle.
      ret = enum_obj->CreateHandle();
    }
  } catch (...) {
    SetLastError(ERROR_OUTOFMEMORY);
  }

  if (NULL != enum_obj)
    enum_obj->Release();

  return ret;
}

bool __cdecl SdbNextInterface(SDBAPIHANDLE sdb_handle,
                      SdbInterfaceInfo* info,
                      unsigned long* size) {
  if (NULL == size) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return false;
  }

  // Lookup SdbInterfaceEnumObject object for the handle
  SdbInterfaceEnumObject* sdb_ienum_object =
    LookupObject<SdbInterfaceEnumObject>(sdb_handle);
  if (NULL == sdb_ienum_object)
    return false;

  // Everything is verified. Pass it down to the object
  bool ret = sdb_ienum_object->Next(info, size);

  sdb_ienum_object->Release();

  return ret;
}

bool __cdecl SdbResetInterfaceEnum(SDBAPIHANDLE sdb_handle) {
  // Lookup SdbInterfaceEnumObject object for the handle
  SdbInterfaceEnumObject* sdb_ienum_object =
    LookupObject<SdbInterfaceEnumObject>(sdb_handle);
  if (NULL == sdb_ienum_object)
    return false;

  // Everything is verified. Pass it down to the object
  bool ret = sdb_ienum_object->Reset();

  sdb_ienum_object->Release();

  return ret;
}

SDBAPIHANDLE __cdecl SdbCreateInterfaceByName(
    const wchar_t* interface_name) {
  SdbInterfaceObject* obj = NULL;
  SDBAPIHANDLE ret = NULL;

  try {
    // Instantiate interface object, depending on the USB driver type.
    if (IsLegacyInterface(interface_name)) {
      // We have legacy USB driver underneath us.
      obj = new SdbLegacyInterfaceObject(interface_name);
    } else {
    printf("IsLegacyInterface failed\n");
      // We have WinUsb driver underneath us. Make sure that SdbWinUsbApi.dll
      // is loaded and its InstantiateWinUsbInterface routine address has
      // been cached.
      if (NULL != InstantiateSdbWinUsbInterface) {
	  	printf("NULL != InstantiateSdbWinUsbInterface\n");
        obj = InstantiateSdbWinUsbInterface(interface_name);
        if (NULL == obj) {
	  	printf("NULL == obj\n");
          return NULL;
        }
      } else {
      printf("NULL != InstantiateWinUsbInterface\n");
        return NULL;
      }
    }

    // Create handle for it
    ret = obj->CreateHandle();
  } catch (...) {
    SetLastError(ERROR_OUTOFMEMORY);
  }

  if (NULL != obj)
    obj->Release();

  return ret;
}

SDBAPIHANDLE __cdecl SdbCreateInterface(GUID class_id,
                                unsigned short vendor_id,
                                unsigned short product_id,
                                unsigned char interface_id) {
  // Enumerate all active interfaces for the given class
  SdbEnumInterfaceArray interfaces;

  if (!EnumerateDeviceInterfaces(class_id,
                                 DIGCF_DEVICEINTERFACE | DIGCF_PRESENT,
                                 true,
                                 true,
                                 &interfaces)) {
    return NULL;
  }

  if (interfaces.empty()) {
    SetLastError(ERROR_DEVICE_NOT_AVAILABLE);
    return NULL;
  }

  // Now iterate over active interfaces looking for the name match.
  // The name is formatted as such:
  // "\\\\?\\usb#vid_xxxx&pid_xxxx&mi_xx#123456789abcdef#{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}"
  // where
  //    vid_xxxx is for the vendor id (xxxx are hex for the given vendor id),
  //    pid_xxxx is for the product id (xxxx are hex for the given product id)
  //    mi_xx is for the interface id  (xx are hex for the given interface id)
  // EnumerateDeviceInterfaces will guarantee that returned interface names
  // will have our class id at the end of the name (those last XXXes in the
  // format). So, we only need to match the beginning of the name
  wchar_t match_name[64];
  if (0xFF == interface_id) {
    // No interface id for the name.
    swprintf(match_name, L"\\\\?\\usb#vid_%04x&pid_%04x#",
             vendor_id, product_id);
  } else {
    // With interface id for the name.
    swprintf(match_name, L"\\\\?\\usb#vid_%04x&pid_%04x&mi_%02x#",
             vendor_id, product_id, interface_id);
  }
  size_t match_len = wcslen(match_name);

  for (SdbEnumInterfaceArray::iterator it = interfaces.begin();
       it != interfaces.end(); it++) {
    const SdbInstanceEnumEntry& next_interface = *it;
    if (0 == _wcsnicmp(match_name,
                      next_interface.device_name().c_str(),
                      match_len)) {
      // Found requested interface among active interfaces.
      return SdbCreateInterfaceByName(next_interface.device_name().c_str());
    }
  }

  SetLastError(ERROR_DEVICE_NOT_AVAILABLE);
  return NULL;
}

bool __cdecl SdbGetInterfaceName(SDBAPIHANDLE sdb_interface,
                         void* buffer,
                         unsigned long* buffer_char_size,
                         bool ansi) {
  // Lookup interface object for the handle
  SdbInterfaceObject* sdb_object =
    LookupObject<SdbInterfaceObject>(sdb_interface);

  if (NULL != sdb_object) {
    // Dispatch call to the found object
    bool ret = sdb_object->GetInterfaceName(buffer, buffer_char_size, ansi);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}

bool __cdecl SdbGetSerialNumber(SDBAPIHANDLE sdb_interface,
                        void* buffer,
                        unsigned long* buffer_char_size,
                        bool ansi) {
  // Lookup interface object for the handle
  SdbInterfaceObject* sdb_object =
    LookupObject<SdbInterfaceObject>(sdb_interface);

  if (NULL != sdb_object) {
    // Dispatch call to the found object
    bool ret = sdb_object->GetSerialNumber(buffer, buffer_char_size, ansi);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}

bool __cdecl SdbGetUsbDeviceDescriptor(SDBAPIHANDLE sdb_interface,
                               USB_DEVICE_DESCRIPTOR* desc) {
  // Lookup interface object for the handle
  SdbInterfaceObject* sdb_object =
    LookupObject<SdbInterfaceObject>(sdb_interface);

  if (NULL != sdb_object) {
    // Dispatch close to the found object
    bool ret = sdb_object->GetUsbDeviceDescriptor(desc);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}

bool __cdecl SdbGetUsbConfigurationDescriptor(SDBAPIHANDLE sdb_interface,
                                      USB_CONFIGURATION_DESCRIPTOR* desc) {
  // Lookup interface object for the handle
  SdbInterfaceObject* sdb_object =
    LookupObject<SdbInterfaceObject>(sdb_interface);

  if (NULL != sdb_object) {
    // Dispatch close to the found object
    bool ret = sdb_object->GetUsbConfigurationDescriptor(desc);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}

bool __cdecl SdbGetUsbInterfaceDescriptor(SDBAPIHANDLE sdb_interface,
                                  USB_INTERFACE_DESCRIPTOR* desc) {
  // Lookup interface object for the handle
  SdbInterfaceObject* sdb_object =
    LookupObject<SdbInterfaceObject>(sdb_interface);

  if (NULL != sdb_object) {
    // Dispatch close to the found object
    bool ret = sdb_object->GetUsbInterfaceDescriptor(desc);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}

bool __cdecl SdbGetEndpointInformation(SDBAPIHANDLE sdb_interface,
                               UCHAR endpoint_index,
                               SdbEndpointInformation* info) {
  // Lookup interface object for the handle
  SdbInterfaceObject* sdb_object =
    LookupObject<SdbInterfaceObject>(sdb_interface);

  if (NULL != sdb_object) {
    // Dispatch close to the found object
    bool ret = sdb_object->GetEndpointInformation(endpoint_index, info);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}

bool __cdecl SdbGetDefaultBulkReadEndpointInformation(SDBAPIHANDLE sdb_interface,
                                              SdbEndpointInformation* info) {
  return SdbGetEndpointInformation(sdb_interface,
                                   SDB_QUERY_BULK_READ_ENDPOINT_INDEX,
                                   info);
}

bool __cdecl SdbGetDefaultBulkWriteEndpointInformation(SDBAPIHANDLE sdb_interface,
                                               SdbEndpointInformation* info) {
  return SdbGetEndpointInformation(sdb_interface,
                                   SDB_QUERY_BULK_WRITE_ENDPOINT_INDEX,
                                   info);
}

SDBAPIHANDLE __cdecl SdbOpenEndpoint(SDBAPIHANDLE sdb_interface,
                             unsigned char endpoint_index,
                             SdbOpenAccessType access_type,
                             SdbOpenSharingMode sharing_mode) {
  // Lookup interface object for the handle
  SdbInterfaceObject* sdb_object =
    LookupObject<SdbInterfaceObject>(sdb_interface);

  if (NULL != sdb_object) {
    // Dispatch close to the found object
    SDBAPIHANDLE ret =
      sdb_object->OpenEndpoint(endpoint_index, access_type, sharing_mode);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return NULL;
  }
}

SDBAPIHANDLE __cdecl SdbOpenDefaultBulkReadEndpoint(SDBAPIHANDLE sdb_interface,
                                            SdbOpenAccessType access_type,
                                            SdbOpenSharingMode sharing_mode) {
  return SdbOpenEndpoint(sdb_interface,
                         SDB_QUERY_BULK_READ_ENDPOINT_INDEX,
                         access_type,
                         sharing_mode);
}

SDBAPIHANDLE __cdecl SdbOpenDefaultBulkWriteEndpoint(SDBAPIHANDLE sdb_interface,
                                             SdbOpenAccessType access_type,
                                             SdbOpenSharingMode sharing_mode) {
  return SdbOpenEndpoint(sdb_interface,
                         SDB_QUERY_BULK_WRITE_ENDPOINT_INDEX,
                         access_type,
                         sharing_mode);
}

SDBAPIHANDLE __cdecl SdbGetEndpointInterface(SDBAPIHANDLE sdb_endpoint) {
  // Lookup endpoint object for the handle
  SdbEndpointObject* sdb_object =
    LookupObject<SdbEndpointObject>(sdb_endpoint);

  if (NULL != sdb_object) {
    // Dispatch the call to the found object
    SDBAPIHANDLE ret = sdb_object->GetParentInterfaceHandle();
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return NULL;
  }
}

bool __cdecl SdbQueryInformationEndpoint(SDBAPIHANDLE sdb_endpoint,
                                 SdbEndpointInformation* info) {
  // Lookup endpoint object for the handle
  SdbEndpointObject* sdb_object =
    LookupObject<SdbEndpointObject>(sdb_endpoint);

  if (NULL != sdb_object) {
    // Dispatch the call to the found object
    bool ret = sdb_object->GetEndpointInformation(info);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}

SDBAPIHANDLE __cdecl SdbReadEndpointAsync(SDBAPIHANDLE sdb_endpoint,
                                  void* buffer,
                                  unsigned long bytes_to_read,
                                  unsigned long* bytes_read,
                                  unsigned long time_out,
                                  HANDLE event_handle) {
  // Lookup endpoint object for the handle
  SdbEndpointObject* sdb_object =
    LookupObject<SdbEndpointObject>(sdb_endpoint);

  if (NULL != sdb_object) {
    // Dispatch the call to the found object
    SDBAPIHANDLE ret = sdb_object->AsyncRead(buffer,
                                             bytes_to_read,
                                             bytes_read,
                                             event_handle,
                                             time_out);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return NULL;
  }
}

SDBAPIHANDLE __cdecl SdbWriteEndpointAsync(SDBAPIHANDLE sdb_endpoint,
                                   void* buffer,
                                   unsigned long bytes_to_write,
                                   unsigned long* bytes_written,
                                   unsigned long time_out,
                                   HANDLE event_handle) {
  // Lookup endpoint object for the handle
  SdbEndpointObject* sdb_object =
    LookupObject<SdbEndpointObject>(sdb_endpoint);

  if (NULL != sdb_object) {
    // Dispatch the call to the found object
    SDBAPIHANDLE ret = sdb_object->AsyncWrite(buffer,
                                              bytes_to_write,
                                              bytes_written,
                                              event_handle,
                                              time_out);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}

bool __cdecl SdbReadEndpointSync(SDBAPIHANDLE sdb_endpoint,
                         void* buffer,
                         unsigned long bytes_to_read,
                         unsigned long* bytes_read,
                         unsigned long time_out) {
  // Lookup endpoint object for the handle
  SdbEndpointObject* sdb_object =
    LookupObject<SdbEndpointObject>(sdb_endpoint);

  if (NULL != sdb_object) {
    // Dispatch the call to the found object
    bool ret =
      sdb_object->SyncRead(buffer, bytes_to_read, bytes_read, time_out);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return NULL;
  }
}

bool __cdecl SdbWriteEndpointSync(SDBAPIHANDLE sdb_endpoint,
                          void* buffer,
                          unsigned long bytes_to_write,
                          unsigned long* bytes_written,
                          unsigned long time_out) {
  // Lookup endpoint object for the handle
  SdbEndpointObject* sdb_object =
    LookupObject<SdbEndpointObject>(sdb_endpoint);

  if (NULL != sdb_object) {
    // Dispatch the call to the found object
    bool ret =
      sdb_object->SyncWrite(buffer, bytes_to_write, bytes_written, time_out);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}

bool __cdecl SdbGetOvelappedIoResult(SDBAPIHANDLE sdb_io_completion,
                             LPOVERLAPPED overlapped,
                             unsigned long* bytes_transferred,
                             bool wait) {
  // Lookup endpoint object for the handle
  SdbIOCompletion* sdb_object =
    LookupObject<SdbIOCompletion>(sdb_io_completion);

  if (NULL != sdb_object) {
    // Dispatch the call to the found object
    bool ret =
      sdb_object->GetOvelappedIoResult(overlapped, bytes_transferred, wait);
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}

bool __cdecl SdbHasOvelappedIoComplated(SDBAPIHANDLE sdb_io_completion) {
  // Lookup endpoint object for the handle
  SdbIOCompletion* sdb_object =
    LookupObject<SdbIOCompletion>(sdb_io_completion);

  if (NULL != sdb_object) {
    // Dispatch the call to the found object
    bool ret =
      sdb_object->IsCompleted();
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return true;
  }
}

bool __cdecl SdbCloseHandle(SDBAPIHANDLE sdb_handle) {
  // Lookup object for the handle
  SdbObjectHandle* sdb_object = SdbObjectHandle::Lookup(sdb_handle);

  if (NULL != sdb_object) {
    // Dispatch close to the found object
    bool ret = sdb_object->CloseHandle();
    sdb_object->Release();
    return ret;
  } else {
    SetLastError(ERROR_INVALID_HANDLE);
    return false;
  }
}
