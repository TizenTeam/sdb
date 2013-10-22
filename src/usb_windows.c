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

#include <windows.h>
#include <setupapi.h>
#include <winerror.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include "fdevent.h"
#include "utils.h"
#include "strutils.h"
#include "linkedlist.h"

#define   TRACE_TAG  TRACE_USB
#include "sdb.h"
#include "sdb_usb.h"
#include "ddk/usb100.h"
#include "ddk/usbioctl.h"

#define DLL_SYMBOL_DECLARE(api, ret, name, args) \
   typedef ret (api * __dll_##name##_t)args; \
   static __dll_##name##_t name

#define LOAD_FUNCPTR(name)                                    \
  do {                                                        \
  HMODULE h = GetModuleHandle("winusb.dll");                  \
  if(h == NULL)                                               \
    h = LoadLibrary("winusb.dll");                            \
  if(h == NULL)                                               \
    break;                                                    \
  if((name = (__dll_##name##_t)GetProcAddress(h, #name)))     \
    break;                                                    \
  if((name = (__dll_##name##_t)GetProcAddress(h, #name "A"))) \
    break;                                                    \
  if((name = (__dll_##name##_t)GetProcAddress(h, #name "W"))) \
    break;                                                    \
  } while(0)

// The USBD_PIPE_TYPE enumerator indicates the type of pipe.
typedef enum _USBD_PIPE_TYPE { 
  UsbdPipeTypeControl      = 0, //Indicates that the pipe is a control pipe.
  UsbdPipeTypeIsochronous  = 1, //Indicates that the pipe is an isochronous transfer pipe.
  UsbdPipeTypeBulk         = 2, //Indicates that the pipe is a bulk transfer pipe.
  UsbdPipeTypeInterrupt    = 3  //Indicates that the pipe is a interrupt pipe.
} USBD_PIPE_TYPE;

// from Winusbio.h
typedef struct _WINUSB_PIPE_INFORMATION {
  USBD_PIPE_TYPE PipeType;          //A USBD_PIPE_TYPE-type enumeration value that specifies the pipe type.
  UCHAR          PipeId;            //The pipe identifier (ID).
  USHORT         MaximumPacketSize; //The maximum size, in bytes, of the packets that are transmitted on the pipe.
  UCHAR          Interval;          //The pipe interval.
} WINUSB_PIPE_INFORMATION, *PWINUSB_PIPE_INFORMATION;

// from Winusb.h
#pragma pack(1)
typedef struct _WINUSB_SETUP_PACKET {
  UCHAR  RequestType;   //The request type. The values that are assigned to this member are defined in Table 9.2 of section 9.3 of the Universal Serial Bus (USB) specification (www.usb.org).
  UCHAR  Request;       //The device request. The values that are assigned to this member are defined in Table 9.3 of section 9.4 of the Universal Serial Bus (USB) specification.
  USHORT Value;         //The meaning of this member varies according to the request. For an explanation of this member, see the Universal Serial Bus (USB) specification.
  USHORT Index;         //The meaning of this member varies according to the request. For an explanation of this member, see the Universal Serial Bus (USB) specification.
  USHORT Length;        //The number of bytes to transfer.
} WINUSB_SETUP_PACKET, *PWINUSB_SETUP_PACKET;
#pragma pack()

typedef void *WINUSB_INTERFACE_HANDLE, *PWINUSB_INTERFACE_HANDLE;

#define PIPE_TRANSFER_TIMEOUT   0x03 // Waits for a time-out interval before canceling the request
struct usb_handle {
    LIST_NODE* node;

    HANDLE hnd;
    WINUSB_INTERFACE_HANDLE fd;
    char unique_node_path[PATH_MAX+1]; //MAX_DEVICE_ID_LEN <device-ID>\<instance-specific-ID>
    UCHAR end_point[2];
    unsigned int zero_mask;
};
usb_handle *usb_open(const char *device_path);
int win_usb_close(usb_handle *dev);
int usb_get_string_simple(usb_handle *dev, int index, char *buf, size_t buflen);
void *device_poll_thread(void* unused);
int usb_bulk_transfer(usb_handle* handle, BOOL is_read, const void *data, unsigned long length, unsigned long *actual_length, unsigned long timeout);
void do_lsusb(void);

// Class ID assigned to the device by aNdrOiduSb.sys
static const GUID TIZEN_CLASSID = {0x9ca29f37, 0xdd62, 0x4aad, {0x82, 0x65, 0xcf, 0xf7, 0x88, 0xc8, 0xba, 0x89}};

static LIST_NODE* handle_list = NULL;

SDB_MUTEX_DEFINE( usb_lock );

// winusb.dll entrypoints
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_Initialize, (HANDLE, PWINUSB_INTERFACE_HANDLE));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_Free, (WINUSB_INTERFACE_HANDLE));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_GetDescriptor, (WINUSB_INTERFACE_HANDLE, UCHAR, UCHAR, USHORT, PUCHAR, ULONG, PULONG));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_QueryInterfaceSettings, (WINUSB_INTERFACE_HANDLE, UCHAR, PUSB_INTERFACE_DESCRIPTOR));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_SetCurrentAlternateSetting, (WINUSB_INTERFACE_HANDLE, UCHAR));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_GetCurrentAlternateSetting, (WINUSB_INTERFACE_HANDLE, PUCHAR));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_QueryPipe, (WINUSB_INTERFACE_HANDLE, UCHAR, UCHAR, PWINUSB_PIPE_INFORMATION));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_SetPipePolicy, (WINUSB_INTERFACE_HANDLE, UCHAR, ULONG, ULONG, PVOID));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_GetPipePolicy, (WINUSB_INTERFACE_HANDLE, UCHAR, ULONG, PULONG, PVOID));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_ReadPipe, (WINUSB_INTERFACE_HANDLE, UCHAR, PUCHAR, ULONG, PULONG, LPOVERLAPPED));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_WritePipe, (WINUSB_INTERFACE_HANDLE, UCHAR, PUCHAR, ULONG, PULONG, LPOVERLAPPED));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_ControlTransfer, (WINUSB_INTERFACE_HANDLE, WINUSB_SETUP_PACKET, PUCHAR, ULONG, PULONG, LPOVERLAPPED));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_ResetPipe, (WINUSB_INTERFACE_HANDLE, UCHAR));
DLL_SYMBOL_DECLARE(WINAPI, BOOL, WinUsb_GetOverlappedResult, (WINUSB_INTERFACE_HANDLE, LPOVERLAPPED, LPDWORD, BOOL));

void win_usb_init(void) {
    // Initialize DLL functions
    if (!WinUsb_Initialize) {
        LOAD_FUNCPTR(WinUsb_Initialize);
        LOAD_FUNCPTR(WinUsb_Free);
        LOAD_FUNCPTR(WinUsb_GetDescriptor);
        LOAD_FUNCPTR(WinUsb_QueryInterfaceSettings);
        LOAD_FUNCPTR(WinUsb_SetCurrentAlternateSetting);
        LOAD_FUNCPTR(WinUsb_GetCurrentAlternateSetting);
        LOAD_FUNCPTR(WinUsb_QueryPipe);
        LOAD_FUNCPTR(WinUsb_SetPipePolicy);
        LOAD_FUNCPTR(WinUsb_GetPipePolicy);
        LOAD_FUNCPTR(WinUsb_ReadPipe);
        LOAD_FUNCPTR(WinUsb_WritePipe);
        LOAD_FUNCPTR(WinUsb_ControlTransfer);
        LOAD_FUNCPTR(WinUsb_ResetPipe);
        LOAD_FUNCPTR(WinUsb_GetOverlappedResult);
    }
}

int is_device_registered(const char *node_path)
{
    int r = 0;
    sdb_mutex_lock(&usb_lock, "usb is_device_registered");

    LIST_NODE* cur_ptr = handle_list;
    while(cur_ptr != NULL) {
    	usb_handle* usb = cur_ptr->data;
    	cur_ptr = cur_ptr->next_ptr;

    	if(!strcmp(usb->unique_node_path, node_path)) {
    		r = 1;
    		break;
    	}
    }
    sdb_mutex_unlock(&usb_lock, "usb is_device_registered");
    return r;
}

usb_handle *usb_open(const char *device_path) {
    // Allocate storage for handles
    usb_handle* usb = calloc(1, sizeof(usb_handle));
    s_strncpy(usb->unique_node_path, device_path, sizeof(usb->unique_node_path));

    // Open generic handle to device
    HANDLE hnd = CreateFile(device_path, GENERIC_WRITE | GENERIC_READ,
            FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED, NULL);

    if (INVALID_HANDLE_VALUE == hnd) {
        D("fail to create device file: %s due to: %d\n", device_path, GetLastError());
       return NULL;
    }

    // Initialize WinUSB for this device and get a WinUSB handle for it
    WINUSB_INTERFACE_HANDLE fd;
    if (!WinUsb_Initialize(hnd, &fd)) {
        return NULL;
    }

    // fetch USB device descriptor
    USB_DEVICE_DESCRIPTOR         usb_device_descriptor;
    unsigned long bytes_written;

    if (!WinUsb_GetDescriptor(fd, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0,
            (PUCHAR)&usb_device_descriptor, sizeof(usb_device_descriptor), &bytes_written)) {
        return NULL;
    }

    USB_INTERFACE_DESCRIPTOR      usb_interface_descriptor;

    // fetch usb interface descriptor
    UCHAR interface_number;
    if (!WinUsb_GetCurrentAlternateSetting(fd, &interface_number)) {
        return NULL;
    }

    if (!WinUsb_QueryInterfaceSettings(fd, interface_number, &usb_interface_descriptor)) {
        return NULL;
    }

    if (2 != usb_interface_descriptor.bNumEndpoints) {
        D("the number of endpoint should be two\n");
        return NULL;
    }

    if (!is_sdb_interface(usb_device_descriptor.idVendor, usb_interface_descriptor.bInterfaceClass, usb_interface_descriptor.bInterfaceSubClass,
            usb_interface_descriptor.bInterfaceProtocol)) {
        return NULL;
    }
    UCHAR endpoint_index = 0;

    for (endpoint_index = 0; endpoint_index < usb_interface_descriptor.bNumEndpoints; endpoint_index++) {
        // fetch endpoint information
        WINUSB_PIPE_INFORMATION pipe_info;
        if (!WinUsb_QueryPipe(fd, interface_number, endpoint_index, &pipe_info)) {
            return NULL;
        }

        if(usb_interface_descriptor.bInterfaceProtocol == 0x01) {
            if (endpoint_index == 0) {
                usb->zero_mask = pipe_info.MaximumPacketSize - 1;
            }
        }
        // only interested in bulk type
        if (UsbdPipeTypeBulk == pipe_info.PipeType) {
            if (USB_ENDPOINT_DIRECTION_IN(pipe_info.PipeId)) {
                D("builk in endpoint index: %d, id: %04x\n", endpoint_index, pipe_info.PipeId);
                usb->end_point[0] = pipe_info.PipeId;
            }

            if (USB_ENDPOINT_DIRECTION_OUT(pipe_info.PipeId)) {
                D("builk out endpoint index: %d, id: %04x\n", endpoint_index, pipe_info.PipeId);
                usb->end_point[1] = pipe_info.PipeId;
            }

        }
    }
    usb->hnd = hnd;
    usb->fd = fd;

    return usb;
}

int register_device(usb_handle *hnd) {
    if (is_device_registered(hnd->unique_node_path)) {
        return 0;
    }

    sdb_mutex_lock(&usb_lock, "usb register_device");

    hnd->node = prepend(&handle_list, hnd);

    sdb_mutex_unlock(&usb_lock, "usb register_device");

    return 1;
}

int usb_get_string_simple(usb_handle *dev, int index, char *buf, size_t buflen) {
    unsigned char temp[MAXIMUM_USB_STRING_LENGTH];

    ULONG actlen = 0;
    //0x0409 for English (US)
    if (!WinUsb_GetDescriptor(dev->fd, USB_STRING_DESCRIPTOR_TYPE, index, 0x0409, temp, sizeof(temp), &actlen)) {
        return -GetLastError();
    }
    // Skip first two bytes of result (descriptor id and length), then take
    // every other byte as a cheap way to convert Unicode to ASCII
    unsigned int i, j;
    for (i = 2, j = 0; i < actlen && j < (buflen - 1); i += 2, ++j)
        buf[j] = temp[i];
    buf[j] = '\0';

    return strlen(buf);
}

static int get_serial_number(usb_handle *dev, char *buf, int buflen) {
    USB_DEVICE_DESCRIPTOR         usb_device_descriptor;
    ULONG actlen = 0;

    if (!WinUsb_GetDescriptor(dev->fd, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, (unsigned char*) &usb_device_descriptor,
            sizeof(usb_device_descriptor), &actlen)) {
        return 0;
    }

    return usb_get_string_simple(dev, usb_device_descriptor.iSerialNumber, buf, buflen);
}

int usb_find_devices(GUID deviceClassID) {
    SP_DEVINFO_DATA deviceInfoData;
    char devicePath[PATH_MAX + 1];
    BOOL bResult = TRUE;
    PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = NULL;
    int index = 0;

    // from http://msdn.microsoft.com/en-us/library/windows/hardware/ff540174(v=vs.85).aspx
    // Get information about all the installed devices for the specified device interface class.
    HDEVINFO hDeviceInfo = SetupDiGetClassDevs(&deviceClassID, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDeviceInfo == INVALID_HANDLE_VALUE) {
        D("fail to find any device: %d\n", GetLastError());
        return 0;
    }

    //Enumerate all the device interfaces in the device information set.
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (index = 0; ; index++) {

        //Get information about the device interface.
        SP_DEVICE_INTERFACE_DATA interfaceData;
        interfaceData.cbSize = sizeof(SP_INTERFACE_DEVICE_DATA);

        bResult = SetupDiEnumDeviceInterfaces(hDeviceInfo, NULL, &deviceClassID, index, &interfaceData);
        // Check if last item
        if (GetLastError() == ERROR_NO_MORE_ITEMS) {
            break;
        }

        //Check for some other error
        if (!bResult)
        {
            D("Error SetupDiEnumDeviceInterfaces: %d.\n", GetLastError());
            break;
        }

        // Determine required size for interface detail data
        ULONG requiredLength = 0;

        //Interface data is returned in SP_DEVICE_INTERFACE_DETAIL_DATA
        //which we need to allocate, so we have to call this function twice.
        //First to get the size so that we know how much to allocate
        //Second, the actual call with the allocated buffer
        bResult = SetupDiGetDeviceInterfaceDetail(hDeviceInfo, &interfaceData, NULL, 0, &requiredLength, NULL);

        //Check for some other error
        if (!bResult) {
            if ((ERROR_INSUFFICIENT_BUFFER == GetLastError()) && (requiredLength > 0)) {
                // Allocate storage for interface detail data
                detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc(requiredLength);

                if (detailData == NULL) {
                    D("fail to allocate memory\n");
                    break;
                }
            } else {
                D("Error SetupDiEnumDeviceInterfaces: %d.\n", GetLastError());
                break;
            }
        }

        // Finally, do fetch interface detail data
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
        //Now call it with the correct size and allocated buffer
        bResult = SetupDiGetDeviceInterfaceDetail(hDeviceInfo, &interfaceData, detailData, requiredLength, NULL,
                &deviceInfoData);

        //Check for some other error
        if (!bResult) {
            D("fail to setdup get device interface detail: %d\n", GetLastError());
            if (detailData != NULL) {
                free(detailData);
            }
            break;
        }

        //copy device path
        s_strncpy(devicePath, detailData->DevicePath, sizeof(devicePath));

        if (detailData != NULL) {
            free(detailData);
        }

        if (!is_device_registered(devicePath)) {
            struct usb_handle *hnd = usb_open(devicePath);
            if (hnd != NULL) {
                char serial[256];
                if (get_serial_number(hnd, serial, sizeof(serial)) > 0) {
                    D("register usb for: %s\n", serial);
                    if (register_device(hnd)) {
                        register_usb_transport(hnd, serial);
                    } else {
                        D("fail to register usb\n");
                        win_usb_close(hnd);
                        free(hnd);
                    }
                } else {
                    D("fail to get usb serial name: kick? close?\n");
                    win_usb_close(hnd);
                    free(hnd);
                }
            }
        }
    }
    //  Cleanup
    bResult = SetupDiDestroyDeviceInfoList(hDeviceInfo);

    return bResult;
}

void* device_poll_thread(void* sleep_msec) {
    D("Created device thread\n");

    int  mseconds = (int) sleep_msec;
    while (1) {
        do_lsusb();
        sdb_sleep_ms(mseconds);
    }

    return NULL;
}

void sdb_usb_init() {
    sdb_thread_t tid;

    win_usb_init();
    if (sdb_thread_create(&tid, device_poll_thread, (void*)3000)) {
        LOG_FATAL("cannot create input thread\n");
    }
}

void sdb_usb_cleanup() {
    D("TODO: not imple yet\n");
}

int usb_bulk_transfer(usb_handle* handle, BOOL is_read, const void *data, unsigned long length, unsigned long *actual_length, unsigned long timeout) {
    ULONG tmp = timeout;
    UCHAR endpoint;

    if (is_read) {
        endpoint = handle->end_point[0];
    } else {
        endpoint = handle->end_point[1];
    }
    if (handle->fd == NULL) {
        D("invalid handle\n");
        return 0;
    }

    // do not complete within the specified time-out interval
    if (!WinUsb_SetPipePolicy(handle->fd, endpoint, PIPE_TRANSFER_TIMEOUT, sizeof(tmp), &tmp)) {
        D("fail to set timeout\n");
        SetLastError(ERROR_SEM_TIMEOUT);
        return 0;
    }

    // manual reset must be true (second param) as the reset occurs in read

    OVERLAPPED overlapped;
    ZeroMemory(&overlapped, sizeof(overlapped));
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    BOOL ret = TRUE;
    ULONG transferred = 0;

    if (is_read) {
        ret = WinUsb_ReadPipe(handle->fd, endpoint, (UCHAR*) data, length, &transferred, &overlapped);
    } else {
        ret = WinUsb_WritePipe(handle->fd, endpoint, (UCHAR*) data, length, &transferred, &overlapped);
    }

    if (!ret && (ERROR_IO_PENDING != GetLastError())) {
        if (NULL != overlapped.hEvent){
            CloseHandle(overlapped.hEvent);
        }
        D("pipe error: (%ld/ld) error:%d\n", length, transferred, GetLastError());
        return 0;
    }

    // wait for the operation to be IO completed
    ret = WinUsb_GetOverlappedResult(handle->fd, &overlapped, &transferred, TRUE);

    if (ret && (NULL != actual_length)) {
        *actual_length = transferred;
    }

    if (NULL != overlapped.hEvent) {
        CloseHandle(overlapped.hEvent);
    }

    return ret ? 1 : 0;
}

int sdb_usb_write(usb_handle* handle, const void* data, int len) {
    unsigned long time_out = 5000;//5000;
    unsigned long written = 0;
    int ret;

    D("+sdb_usb_write %d\n", len);

    if (NULL != handle) {
        ret = usb_bulk_transfer(handle, FALSE, (void*)data, (unsigned long)len, &written, time_out);
        int saved_errno = GetLastError();
        D("sdb_usb_write got(ret:%d): %ld, expected: %d, errno: %d\n",ret, written, len, saved_errno);

        if (ret) {
            if (written == (unsigned long) len) {
                if (handle->zero_mask && (len & handle->zero_mask) == 0) {
                    // Send a zero length packet
                    usb_bulk_transfer(handle, FALSE, (void*)data, 0, &written, time_out);
                }
                return 0;
            }
        } else {
            // assume ERROR_INVALID_HANDLE indicates we are disconnected
            if (saved_errno == ERROR_INVALID_HANDLE) {
                sdb_usb_kick(handle);
            }
        }
        errno = saved_errno;
    } else {
        D("usb_write NULL handle\n");
        SetLastError(ERROR_INVALID_HANDLE);
    }

    D("-sdb_usb_write failed: %d\n", errno);

    return -1;
}

int sdb_usb_read(usb_handle *handle, void* data, int len) {
    unsigned long n = 0;
    int ret;

    D("+sdb_usb_read %d\n", len);
    if (NULL != handle) {
        while (len > 0) {
            int xfer = (len > 4096) ? 4096 : len;
            ret = usb_bulk_transfer(handle, TRUE, (void*)data, (unsigned long)xfer, &n, (unsigned long)0);
            int saved_errno = GetLastError();
            D("sdb_usb_read got(ret:%d): %ld, expected: %d, errno: %d\n", ret, n, xfer, saved_errno);

            if (ret) {
                data += n;
                len -= n;

                if (len == 0)
                    return 0;
            } else {
                // assume ERROR_INVALID_HANDLE indicates we are disconnected
                if (saved_errno == ERROR_INVALID_HANDLE) {
                    sdb_usb_kick(handle);
                }
                break;
            }
            errno = saved_errno;
        }
    } else {
        D("sdb_usb_read NULL handle\n");
        SetLastError(ERROR_INVALID_HANDLE);
    }

    D("-sdb_usb_read failed: %d\n", errno);

    return -1;
}

int win_usb_close(usb_handle *handle) {
    D("+usb win_usb_close\n");
    if (NULL != handle) {
        if (NULL != handle->fd) {
            WinUsb_Free(handle->fd);
            handle->fd = NULL;
        }
        if (NULL != handle->hnd) {
            CloseHandle(handle->hnd);
            handle->hnd = NULL;
        }
        handle = NULL;
    }
    D("-usb win_usb_close\n");
    return 0;
}

void sdb_usb_kick(usb_handle* handle) {
    D("+sdb_usb_kick: %p\n", handle);
    // called from transport remote kick if already registered
    // so only clean for win usb handle resources
    sdb_mutex_lock(&usb_lock, "usb sdb_usb_kick");
    win_usb_close(handle);
    sdb_mutex_unlock(&usb_lock , "usb sdb_usb_kick");
    D("-sdb_usb_kick: %p\n", handle);
}

int sdb_usb_close(usb_handle* handle) {
    D("+sdb_usb_close: %p\n", handle);
    if (NULL != handle) {
        sdb_mutex_lock(&usb_lock, "usb sdb_usb_close");
        remove_node(&handle_list, handle->node, NULL);
        sdb_mutex_unlock(&usb_lock, "usb sdb_usb_close");
    }
    D("-sdb_usb_close: %p\n", handle);
    return 0;
}

void do_lsusb() {
    usb_find_devices(TIZEN_CLASSID);
}
