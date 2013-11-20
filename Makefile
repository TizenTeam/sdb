#
#
# Makefile for sdb
#

#
HOST_OS := $(shell uname -s | tr A-Z a-z | cut -d'_' -f1)
LBITS := $(shell getconf LONG_BIT)
OBJDIR := bin
MODULE := sdb

# sdb host tool
# =========================================================

ifeq ($(HOST_OS),darwin)
	CC := clang
endif
#
ifeq ($(HOST_OS),linux)
	LOCAL_USB_SRC :=  src/usb_linux.c
	LOCAL_UTIL_SRC := src/utils_unix.c
	LOCAL_OTHER_SRC := src/fdevent.c src/fdevent_unix.c
	LOCAL_LFLAGS := -lrt -lpthread -ludev
	LOCAL_CFLAGS := -DOS_LINUX -DHAVE_FORKEXEC -DHAVE_TERMIO_H -DHAVE_SYMLINKS -DSDB_HOST=1 -DSDB_HOST_ON_TARGET=1 -D_FILE_OFFSET_BITS=64
endif

ifeq ($(HOST_OS),darwin)
	LOCAL_USB_SRC := src/usb_darwin.c
	LOCAL_UTIL_SRC := src/utils_unix.c
	LOCAL_OTHER_SRC := src/fdevent.c src/fdevent_unix.c
	LOCAL_LFLAGS := -lpthread -framework CoreFoundation -framework IOKit -framework Carbon
	LOCAL_CFLAGS := -DOS_DARWIN -DHAVE_FORKEXEC -DHAVE_TERMIO_H -DHAVE_SYMLINKS -mmacosx-version-min=10.4 -DSDB_HOST=1 -DSDB_HOST_ON_TARGET=1
endif

ifeq ($(HOST_OS),mingw32)
	LOCAL_USB_SRC := src/usb_windows.c
	LOCAL_UTIL_SRC := src/utils_windows.c
	LOCAL_OTHER_SRC := src/fdevent.c  src/fdevent_windows.c
	LOCAL_CFLAGS := -DOS_WINDOWS
	LOCAL_IFLAGS := -I/mingw/include/ddk
	LOCAL_LFLAGS := -lws2_32
	LOCAL_STATIC_LFLAGS := /mingw/lib/libsetupapi.a
endif


SDB_SRC_FILES := \
	src/sdb.c \
	src/transport.c \
	src/transport_local.c \
	src/transport_usb.c \
	src/commandline.c \
	src/sdb_client.c \
	src/sockets.c \
	src/file_sync_client.c \
	$(LOCAL_USB_SRC) \
	src/device_vendors.c \
	$(LOCAL_UTIL_SRC) \
	$(LOCAL_OTHER_SRC) \
	src/utils.c \
	src/strutils.c \
	src/memutils.c \
	src/linkedlist.c \
	src/sdb_model.c \
	src/sdb_constants.c \
	src/file_sync_functions.c \
	src/command_function.c \
	src/log.c \
	src/listener.c \
	src/sdb_map.c

SDB_CFLAGS := -O2 -g -Wall -Wno-unused-parameter
SDB_CFLAGS += -D_XOPEN_SOURCE -D_GNU_SOURCE
SDB_CFLAGS += -Iinclude -Isrc
SDB_CFLAGS += $(LOCAL_CFLAGS)
ifeq ($(MAKE_DEBUG),true)
SDB_CFLAGS += -DMAKE_DEBUG
endif
SDB_LFLAGS := $(LOCAL_LFLAGS)
STATIC_LFLAGS := $(LOCAL_STATIC_LFLAGS)

all : $(MODULE)

sdb : $(SDB_SRC_FILES)
	mkdir -p $(OBJDIR)
	$(CC) $(SDB_CFLAGS) -o $(OBJDIR)/$(MODULE) $(SDB_SRC_FILES) $(LOCAL_IFLAGS) $(SDB_LFLAGS) $(STATIC_LFLAGS)

clean :
	rm -rf $(OBJDIR)
