#
#
# Makefile for sdb
#

#
HOST_OS := $(shell uname -s | tr A-Z a-z)

# sdb host tool
# =========================================================

# Default to a virtual (sockets) usb interface
USB_SRCS :=
EXTRA_SRCS :=

ifeq ($(HOST_OS),linux)
	USB_SRCS := usb_linux.c
	EXTRA_SRCS := get_my_path_linux.c
	LOCAL_LDLIBS += -lrt -lncurses -lpthread
endif

ifeq ($(HOST_OS),darwin)
	USB_SRCS := usb_osx.c
	EXTRA_SRCS := get_my_path_darwin.c
	LOCAL_LDLIBS += -lpthread -framework CoreFoundation -framework IOKit -framework Carbon
	SDB_EXTRA_CFLAGS := -mmacosx-version-min=10.4
endif

ifeq ($(HOST_OS),freebsd)
	USB_SRCS := usb_libusb.c
	EXTRA_SRCS := get_my_path_freebsd.c
	LOCAL_LDLIBS += -lpthread -lusb
endif



SDB_SRC_FILES := \
	src/sdb.c \
	src/console.c \
	src/transport.c \
	src/transport_local.c \
	src/transport_usb.c \
	src/commandline.c \
	src/sdb_client.c \
	src/sockets.c \
	src/services.c \
	src/file_sync_client.c \
	src/$(EXTRA_SRCS) \
	src/$(USB_SRCS) \
	src/utils.c \
	src/usb_vendors.c \
	src/fdevent.c \
	src/socket_inaddr_any_server.c \
	src/socket_local_client.c \
	src/socket_local_server.c \
	src/socket_loopback_client.c \
	src/socket_loopback_server.c \
	src/socket_network_client.c \
	src/strutils.c

SDB_CFLAGS := -O2 -g -DSDB_HOST=1 -DSDB_HOST_ON_TARGET=1 -Wall -Wno-unused-parameter
SDB_CFLAGS += -D_XOPEN_SOURCE -D_GNU_SOURCE
SDB_CFLAGS += -DHAVE_FORKEXEC -DHAVE_TERMIO_H -DHAVE_SYMLINKS
SDB_LFLAGS := $(LOCAL_LDLIBS)

SDBD_SRC_FILES := \
	src/sdb.c \
	src/fdevent.c \
	src/transport.c \
	src/transport_local.c \
	src/transport_usb.c \
	src/sockets.c \
	src/services.c \
	src/file_sync_service.c \
	src/framebuffer_service.c \
	src/remount_service.c \
	src/usb_linux_client.c \
	src/utils.c \
	src/socket_inaddr_any_server.c \
	src/socket_local_client.c \
	src/socket_local_server.c \
	src/socket_loopback_client.c \
	src/socket_loopback_server.c \
	src/socket_network_client.c \
	src/properties.c \
	src/android_reboot.c \
	src/strutils.c

SDBD_CFLAGS := -O2 -g -DSDB_HOST=0 -Wall -Wno-unused-parameter
SDBD_CFLAGS += -D_XOPEN_SOURCE -D_GNU_SOURCE
SDBD_CFLAGS += -DHAVE_FORKEXEC -fPIE

IFLAGS := -Iinclude -Isrc
OBJDIR := bin
INSTALLDIR := usr/sbin
INITSCRIPTDIR := etc/init.d
RCSCRIPTDIR := etc/rc.d/rc3.d

UNAME := $(shell uname -sm)
ifneq (,$(findstring 86,$(UNAME)))
	HOST_ARCH := x86
endif

TARGET_ARCH = $(HOST_ARCH)
ifeq ($(TARGET_ARCH),)
	TARGET_ARCH := arm
endif

ifeq ($(TARGET_ARCH),arm)
	MODULE := sdbd
	SDBD_CFLAGS += -DANDROID_GADGET=1
else
ifeq ($(TARGET_HOST),true)
	MODULE := sdb
else
	MODULE := sdbd
endif
endif

all : $(MODULE)

sdb : $(SDB_SRC_FILES)
	mkdir -p $(OBJDIR)
	$(CC) -pthread -o $(OBJDIR)/$(MODULE) $(SDB_CFLAGS) $(SDB_EXTRA_CFLAGS) $(SDB_LFLAGS) $(IFLAGS) $(SDB_SRC_FILES)

sdbd : $(SDBD_SRC_FILES)
	mkdir -p $(OBJDIR)
	$(CC) -pthread -o $(OBJDIR)/$(MODULE) $(SDBD_CFLAGS) $(IFLAGS) $(SDBD_SRC_FILES)

install :
	mkdir -p $(DESTDIR)/$(INSTALLDIR)
	install $(OBJDIR)/$(MODULE) $(DESTDIR)/$(INSTALLDIR)/$(MODULE)
ifeq ($(MODULE),sdbd)
	mkdir -p $(DESTDIR)/$(INITSCRIPTDIR)
	install script/sdbd $(DESTDIR)/$(INITSCRIPTDIR)/sdbd
endif
	mkdir -p $(DESTDIR)/$(RCSCRIPTDIR)
	install script/S06sdbd $(DESTDIR)/$(RCSCRIPTDIR)/S06sdbd

clean :
	rm -rf $(OBJDIR)/*
