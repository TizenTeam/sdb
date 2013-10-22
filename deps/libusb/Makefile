#
#
# Makefile for libusb
#

#
HOST_OS := $(shell uname -s | tr A-Z a-z | cut -d'_' -f1)

# =========================================================

ifeq ($(HOST_OS),darwin)
	CC := /Applications/Xcode.app/Contents/Developer/usr/llvm-gcc-4.2/bin/llvm-gcc-4.2
else
	CC := gcc
endif

#
ifeq ($(HOST_OS),linux)
	LOCAL_USB_SRC :=  linux.c error.c usb.c descriptors.c
	LOCAL_LFLAGS := -lrt -lpthread
	LOCAL_CFLAGS := -DOS_LINUX
endif

ifeq ($(HOST_OS),darwin)
	LOCAL_USB_SRC := darwin.c error.c usb.c descriptors.c
	LOCAL_LFLAGS := -lpthread -framework CoreFoundation -framework IOKit -framework Carbon
	LOCAL_CFLAGS := -DOS_DARWIN -mmacosx-version-min=10.4
endif

MODULE = libusb.so

all : $(MODULE)

$(MODULE) : $(LOCAL_USB_SRC)
	$(CC) -o $(MODULE) -fPIC $(LOCAL_USB_SRC) $(LOCAL_LFLAGS) $(LOCAL_CFLAGS) -shared

install :
	mkdir -p ../../bin
	cp $(MODULE) ../../bin
clean :
	rm -rf $(MODULE)
	rm -rf *.o
