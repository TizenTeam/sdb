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
 *
 * documents from https://developer.apple.com/library/mac/documentation/DeviceDrivers/Conceptual/USBBook/USBOverview/USBOverview.html
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>

#include "fdevent.h"
#include "strutils.h"
#include "memutils.h"
#include "sdb_usb.h"
#include "device_vendors.h"
#include "log.h"
#include "utils.h"
#include "linkedlist.h"
#include "transport.h"

#define MAX_C_STRLEN            128
#define kUSBLanguageEnglish     0x409

static IONotificationPortRef gNotifyPort;
static io_iterator_t* gAddedIter;
static CFRunLoopRef gRunLoop;

static pthread_mutex_t usb_init_lock;
static pthread_cond_t usb_init_cond;

struct usb_handle {
    UInt8 end_point[2]; // 0:in, 1:out
    IOUSBInterfaceInterface **interface;
    io_object_t usbNotification;
    unsigned int zero_mask;
};

static void usb_unplugged(usb_handle *handle) {
    LOG_INFO("clean interface resources\n");
    if (!handle)
        return;

    if (handle->interface) {
        (*handle->interface)->USBInterfaceClose(handle->interface);
        (*handle->interface)->Release(handle->interface);
        handle->interface = 0;
    }
}

void DeviceNotification(void * refCon, io_service_t service, natural_t messageType,
        void * messageArgument) {

    kern_return_t kr;
    usb_handle *handle = (usb_handle *) refCon;

    if (messageType == kIOMessageServiceIsTerminated) {
        LOG_DEBUG("Device 0x%08x removed.\n", service);

        kr = IOObjectRelease(handle->usbNotification);
        sdb_usb_kick(handle);
    }

}

/**
 * Authors: Vipul Gupta, Pete St. Pierre
 **/
static void UsbCharactersToHostCharacters(UniChar *p, UInt16 len) {
    for (; len > 0; --len, ++p)
        *p = USBToHostWord(*p);
}

kern_return_t getUSBSerial(IOUSBDeviceInterface182 **dev, UInt8 string_id, char *spotSerial) {
    UInt8 buffer[256];
    UInt16 result_length;
    CFStringRef result;
    kern_return_t kr = -1;
    IOUSBDevRequest request;

    memset(buffer, 0, sizeof(buffer));
    if (string_id != 0) {
        result_length = sizeof(buffer);
        request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
        request.bRequest = kUSBRqGetDescriptor;
        request.wValue = (kUSBStringDesc << 8) | string_id;
        request.wIndex = kUSBLanguageEnglish;
        request.wLength = result_length;
        request.pData = buffer;
        kr = (*dev)->DeviceRequest(dev, &request);

        if ((kIOReturnSuccess == kr) && (request.wLength > 0)
                && (request.wLength <= sizeof(buffer))) {
            result_length = buffer[0];
            if ((0 < result_length) && (result_length <= sizeof(buffer))) {
                /*
                 * Convert USB string (always little-endian) to host-endian but
                 * leave the descriptor type byte and the length alone.
                 */
                UsbCharactersToHostCharacters(((UniChar *) buffer) + 1, ((result_length - 2) >> 1));

                /* Recreate a string from the buffer of unicode characters */
                result = CFStringCreateWithCharacters(kCFAllocatorDefault, ((UniChar *) buffer) + 1,
                        ((result_length - 2) >> 1));

                /* Copy the character contents to a local C string */
                CFStringGetCString(result, spotSerial, MAX_C_STRLEN, kCFStringEncodingASCII);
            }
        }
    }

    return kr;
}

static IOReturn FindInterfaces(IOUSBInterfaceInterface **interface, UInt16 vendor, UInt16 product,
        usb_handle *handle) {
    IOReturn kr;
    UInt8 intfClass;
    UInt8 intfSubClass;
    UInt8 intfProtocol;
    UInt8 intfNumEndpoints;
    int pipeRef;

    // open the interface. This will cause the pipes to be instantiated that are
    // associated with the endpoints defined in the interface descriptor.
    kr = (*interface)->USBInterfaceOpen(interface);
    if (kIOReturnSuccess != kr) {
        LOG_DEBUG("unable to open interface (%08x)\n", kr);
        return kr;
    }

    kr = (*interface)->GetNumEndpoints(interface, &intfNumEndpoints);
    if (kIOReturnSuccess != kr) {
        LOG_DEBUG("unable to get number of endpoints (%08x)\n", kr);
        (void) (*interface)->USBInterfaceClose(interface);
        return kr;
    }

    kr = (*interface)->GetInterfaceClass(interface, &intfClass);
    kr = (*interface)->GetInterfaceSubClass(interface, &intfSubClass);
    kr = (*interface)->GetInterfaceProtocol(interface, &intfProtocol);

    LOG_DEBUG("interface class %0x, subclass %0x, protocol: %0x\n", intfClass, intfSubClass,
            intfProtocol);

    if (!is_sdb_interface(vendor, intfClass, intfSubClass, intfProtocol)) {
        LOG_DEBUG("it is not sdb interface\n");
        (void) (*interface)->USBInterfaceClose(interface);
        return kIOReturnError;
    }

    for (pipeRef = 0; pipeRef <= intfNumEndpoints; pipeRef++) {
        UInt8 direction;
        UInt8 number;
        UInt8 transferType;
        UInt16 maxPacketSize;
        UInt8 interval;
        char *message;

        kr = (*interface)->GetPipeProperties(interface, pipeRef, &direction, &number, &transferType,
                &maxPacketSize, &interval);
        if (kIOReturnSuccess != kr)
            LOG_DEBUG("unable to get properties of pipe %d (%08x)\n", pipeRef, kr);
        else {
            LOG_DEBUG("++ pipeRef:%d: ++\n ", pipeRef);

            switch (transferType) {
            case kUSBControl:
                message = "control";
                break;
            case kUSBIsoc:
                message = "isoc";
                break;
            case kUSBBulk:
                message = "bulk";
                break;
            case kUSBInterrupt:
                message = "interrupt";
                break;
            case kUSBAnyType:
                message = "any";
                break;
            default:
                message = "???";
            }
            LOG_DEBUG("transfer type:%s, maxPacketSize:%d\n", message, maxPacketSize);
            if (kUSBBulk != transferType) {
                continue;
            }
            handle->zero_mask = maxPacketSize - 1;

            switch (direction) {
            case kUSBOut:
                message = "out";
                handle->end_point[1] = pipeRef;
                break;
            case kUSBIn:
                message = "in";
                handle->end_point[0] = pipeRef;
                break;
            case kUSBNone:
                message = "none";
                break;
            case kUSBAnyDirn:
                message = "any";
                break;
            default:
                message = "???";
            }
            LOG_DEBUG("direction:%0x(%s)\n", pipeRef, message);
            kr = kIOReturnSuccess;
        }
    }
    handle->interface = interface;

    return kr;
}

void DeviceAdded(void *refCon, io_iterator_t iterator) {
    kern_return_t kr;
    io_service_t usbDevice;
    io_service_t usbInterface;
    IOCFPlugInInterface **ioDev = NULL;
    IOUSBDeviceInterface182 **intf = NULL;
    IOUSBInterfaceInterface220 **interface = NULL;
    SInt32 score;
    HRESULT res;
    UInt16 usbVendor;
    UInt16 usbProduct;
    UInt8 serialIndex;
    char serial[256];

    while ((usbInterface = IOIteratorNext(iterator))) {
        // Create an intermediate plug-in
        kr = IOCreatePlugInInterfaceForService(usbInterface, kIOUSBInterfaceUserClientTypeID,
                kIOCFPlugInInterfaceID, &ioDev, &score);
        IOObjectRelease(usbInterface);

        if ((kIOReturnSuccess != kr) || !ioDev) {
            LOG_DEBUG("couldn't create a device interface plugin, find next...\n");
            continue;
        }
        res = (*ioDev)->QueryInterface(ioDev,
                CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID) & interface);
        (*ioDev)->Release(ioDev);

        if (res || !interface) {
            LOG_DEBUG("couldn't create an IOUSBInterfaceInterface, find next... \n");
            continue;
        }

        kr = (*interface)->GetDevice(interface, &usbDevice);
        if (kIOReturnSuccess != kr || !usbDevice) {
            LOG_DEBUG("couldn't get device from interface, find next...\n");
            continue;
        }

        kr = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID,
                kIOCFPlugInInterfaceID, &ioDev, &score);
        // Release the usbDevice object after getting the plug-in
        IOObjectRelease(usbDevice);

        if ((kIOReturnSuccess != kr) || !ioDev) {
            LOG_DEBUG("unable to create a device plugin, find next...\n");
            continue;
        }

        // I have the device plugin, I need the device interface
        //
        res = (*ioDev)->QueryInterface(ioDev,
                CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID)&intf);
        (*ioDev)->Release(ioDev);

        if (res || !intf) {
            LOG_DEBUG("couldn't create a usb interface, find next...\n");
            continue;
        }
        // Get vendor, product and serial index
        kr = (*intf)->GetDeviceVendor(intf, &usbVendor);
        if (kIOReturnSuccess != kr) {
            LOG_ERROR("couldn't get usb vendor name\n");
        }
        kr = (*intf)->GetDeviceProduct(intf, &usbProduct);
        if (kIOReturnSuccess != kr) {
            LOG_ERROR("couldn't get usb product name\n");
        }
        kr = (*intf)->USBGetSerialNumberStringIndex(intf, &serialIndex);
        if (kIOReturnSuccess != kr) {
            LOG_ERROR("couldn't get usb serial index\n");
        }
        kr = getUSBSerial(intf, serialIndex, serial);
        (*intf)->Release(intf);

        usb_handle* handle = calloc(1, sizeof(usb_handle));
        if (handle == NULL) {
            LOG_FATAL("could't alloc mememroy\n");
        }

        kr = FindInterfaces((IOUSBInterfaceInterface**) interface, usbVendor, usbProduct, handle);
        if (kIOReturnSuccess != kr) {
            free(handle);
            handle = NULL;
            (*interface)->Release(interface);
            continue;
        }

        LOG_DEBUG("found tizen device and register usb transport.........\n");
        register_usb_transport(handle, serial);

        // Register for an interest notification for this device. Pass the reference to our
        // private data as the refCon for the notification.
        //
        kr = IOServiceAddInterestNotification(gNotifyPort, // notifyPort
                usbInterface, // service
                kIOGeneralInterest, // interestType
                DeviceNotification, // callback
                handle, // refCon
                &(handle->usbNotification) // notification
                );

        if (kIOReturnSuccess != kr) {
            LOG_DEBUG("IOServiceAddInterestNotification returned 0x%08x\n", kr);
        }

    }
    LOG_DEBUG("signal to wake up main thread\n");
    sdb_mutex_lock(&usb_init_lock, "++usb locking++");
    sdb_cond_signal(&usb_init_cond);
    sdb_mutex_unlock(&usb_init_lock, "--usb unlocking--");
}

static int cleanup_flag = 0;
static void sig_handler(int sigraised) {
    int i = 0;

    LOG_DEBUG("Interrupted!\n");

    if (cleanup_flag == 1) {
        return;
    }

    // Clean up
    for (i = 0; i < vendor_total_cnt; i++) {
        IOObjectRelease(gAddedIter[i]);
    }
    gAddedIter = NULL;

    IONotificationPortDestroy(gNotifyPort);
    gRunLoop = 0;

    if (gAddedIter != NULL) {
        s_free(gAddedIter);
        gAddedIter = NULL;
    }

    LOG_DEBUG("RunLoopThread done\n");
    if (gRunLoop) {
        CFRunLoopStop(gRunLoop);
    }
    cleanup_flag = 1;
}

void do_lsusb(void) {
    mach_port_t masterPort;
    CFMutableDictionaryRef matchingDict;
    CFRunLoopSourceRef runLoopSource;
    CFNumberRef numberRef;
    kern_return_t kr;
    SInt32 usbVendor, subClass, protocol;
    sig_t oldHandler;
    int i = 0;

    // Set up a signal handler so we can clean up when we're interrupted from the command line
    // Otherwise we stay in our run loop forever.
    //
    oldHandler = signal(SIGINT, sig_handler);
    if (oldHandler == SIG_ERR) {
        LOG_DEBUG("Could not establish new signal handler\n");
    }

    // first create a master_port for my task
    //
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr || !masterPort) {
        LOG_DEBUG("ERR: Couldn't create a master IOKit Port(%08x)\n", kr);
        return;
    }

    // Create a notification port and add its run loop event source to our run loop
    // This is how async notifications get set up.
    //
    gNotifyPort = IONotificationPortCreate(masterPort);
    runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);

    gRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(gRunLoop, runLoopSource, kCFRunLoopDefaultMode);

    for (i = 0; tizen_device_vendors[i].vendor != NULL; i++) {
        // Set up the matching criteria for the devices we're interested in.  The matching criteria needs to follow
        // the same rules as kernel drivers:  mainly it needs to follow the USB Common Class Specification, pp. 6-7.
        // See also http://developer.apple.com/qa/qa2001/qa1076.html
        // One exception is that you can use the matching dictionary "as is", i.e. without adding any matching criteria
        // to it and it will match every IOUSBDevice in the system.  IOServiceAddMatchingNotification will consume this
        // dictionary reference, so there is no need to release it later on.
        //
        matchingDict = IOServiceMatching(kIOUSBInterfaceClassName); // Interested in instances of class
                                                                    // IOUSBInterface and its subclasses
        if (!matchingDict) {
            LOG_DEBUG("Can't create a USB matching dictionary\n");
            mach_port_deallocate(mach_task_self(), masterPort);
            return;
        }
        usbVendor = tizen_device_vendors[i].id;
        subClass = SDB_INTERFACE_SUBCLASS;
        protocol = SDB_INTERFACE_PROTOCOL;

        LOG_DEBUG(
                "Looking for devices matching vendor ID=%0x(%0x, %0x)\n", usbVendor, subClass, protocol);

        // We are interested in all USB Devices (as opposed to USB interfaces).  The Common Class Specification
        // tells us that we need to specify the idVendor, idProduct, and bcdDevice fields, or, if we're not interested
        // in particular bcdDevices, just the idVendor and idProduct.  Note that if we were trying to match an IOUSBInterface,
        // we would need to set more values in the matching dictionary (e.g. idVendor, idProduct, bInterfaceNumber and
        // bConfigurationValue.
        //

        // Create a CFNumber for the idVendor, interface subclass and protocol and set the value in the dictionary
        //
        numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor);
        CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorID), numberRef);
        CFRelease(numberRef);

        numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &subClass);
        CFDictionarySetValue(matchingDict, CFSTR(kUSBInterfaceSubClass), numberRef);
        CFRelease(numberRef);

        numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &protocol);
        CFDictionarySetValue(matchingDict, CFSTR(kUSBInterfaceProtocol), numberRef);
        CFRelease(numberRef);

        numberRef = 0;

        // Now set up a notification to be called when a device is first matched by I/O Kit.
        // Note that this will not catch any devices that were already plugged in so we take
        // care of those later.
        //
        kr = IOServiceAddMatchingNotification(gNotifyPort, // notifyPort
                kIOFirstMatchNotification, // notificationType
                matchingDict, // matching
                DeviceAdded, // callback
                NULL, // refCon
                &gAddedIter[i] // notification
                );

        // Iterate once to get already-present devices and arm the notification
        //
        DeviceAdded(NULL, gAddedIter[i]);
    }

    // Now done with the master_port
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;

    // Start the run loop. Now we'll receive notifications.
    //
    LOG_DEBUG("Starting run loop.\n");
    CFRunLoopRun();

    // We should never get here
    //
    LOG_DEBUG("Unexpectedly back from CFRunLoopRun()!\n");
}

void* usb_poll_thread(void* sleep_msec) {
    LOG_DEBUG("created usb detecting thread\n");
    do_lsusb();
    return NULL;
}

void sdb_usb_init() {
    sdb_thread_t tid;

    init_device_vendors();
    gAddedIter = (io_iterator_t*) s_malloc(vendor_total_cnt * sizeof(io_iterator_t));
    if (gAddedIter == NULL) {
        LOG_FATAL("Cound not alloc memory\n");
        return;
    }

    sdb_mutex_init(&usb_init_lock, NULL);
    sdb_cond_init(&usb_init_cond, NULL);

    if (sdb_thread_create(&tid, usb_poll_thread, NULL)) {
        LOG_FATAL("cannot create usb poll thread\n");
    }
    sdb_mutex_lock(&usb_init_lock, "++usb locking++");

    LOG_DEBUG("waiting until to finish to initilize....\n");
    // wait til finish to initialize some setting for usb detection
    sdb_cond_wait(&usb_init_cond, &usb_init_lock);

    LOG_DEBUG("woke up done....\n");
    sdb_mutex_unlock(&usb_init_lock, "--usb unlocking--");
    sdb_mutex_destroy(&usb_init_lock);
    sdb_cond_destroy(&usb_init_cond);
}

void sdb_usb_cleanup() {
    // called when server stop or interrupted
    close_usb_devices();
    sig_handler(0);
}

int sdb_usb_write(usb_handle *h, const void *data, int len) {
    IOReturn kr;

    kr = (*h->interface)->WritePipe(h->interface, h->end_point[1], (void *) data, len);
    if (kr == kIOReturnSuccess) {
        if (h->zero_mask && !(h->zero_mask & len)) {
            (*h->interface)->WritePipe(h->interface, h->end_point[1], (void *) data, 0);
        }
        return 0;
    }

    LOG_DEBUG("Unable to perform bulk write (%08x)\n", kr);
    return -1;
}

int sdb_usb_read(usb_handle *h, void *data, int len) {
    IOReturn kr;
    UInt32 size = len;

    kr = (*h->interface)->ReadPipe(h->interface, h->end_point[0], data, &size);

    if (kIOReturnSuccess != kr) {
        LOG_DEBUG("Unable to perform bulk read (%08x)\n", kr);
        return -1;
    }

    return 0;
}

int sdb_usb_close(usb_handle *h) {
    return 0;
}

void sdb_usb_kick(usb_handle *h) {
    usb_unplugged(h);
}
