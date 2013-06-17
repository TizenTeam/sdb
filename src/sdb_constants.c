/*
* SDB - Smart Development Bridge
*
* Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
*
* Contact:
* Ho Namkoong <ho.namkoong@samsung.com>
* Yoonki Park <yoonki.park@samsung.com>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions andㄴ
* limitations under the License.
*
* Contributors:
* - S-Core Co., Ltd
*
*/

#include "sdb_constants.h"

    const char* HELP_APPEND_STR = "                                ";

    const char* EMPTY_STRING = "";
    const char* DIR_APP_TMP = "/opt/usr/apps/tmp/";
    const char* QUOTE_CHAR = " \"\\()";

    const char* PREFIX_HOST = "host:";
    const char* PREFIX_HOST_USB = "host-usb:";
    const char* PREFIX_HOST_LOCAL = "host-local:";
    const char* PREFIX_HOST_SERIAL = "host-serial:";

    const char* PREFIX_TRANSPORT_ANY = "host:transport-any";
    const char* PREFIX_TRANSPORT_USB = "host:transport-usb";
    const char* PREFIX_TRANSPORT_LOCAL = "host:transport-local";
    const char* PREFIX_TRANSPORT_SERIAL = "host:transport:";

    const char* COMMANDLINE_MSG_FULL_CMD = "full command of %s: %s\n";
    const char* COMMANDLINE_OPROFILE_NAME = "oprofile";
    const int COMMANDLINE_OPROFILE_MAX_ARG = -1;
    const int COMMANDLINE_OPROFILE_MIN_ARG = 0;

    const char* COMMANDLINE_DA_NAME = "da";
    const int COMMANDLINE_DA_MAX_ARG = -1;
    const int COMMANDLINE_DA_MIN_ARG = 0;

    const char* COMMANDLINE_LAUNCH_NAME = "launch";
    const int COMMANDLINE_LAUNCH_MAX_ARG = -1;
    const int COMMANDLINE_LAUNCH_MIN_ARG = 0;

    const char* COMMANDLINE_DEVICES_NAME = "devices";
    const char* COMMANDLINE_DEVICES_DESC[] = {
            "list all connected devices"
    };
    const int COMMANDLINE_DEVICES_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_DEVICES_DESC, char*);
    const int COMMANDLINE_DEVICES_MAX_ARG = 0;
    const int COMMANDLINE_DEVICES_MIN_ARG = 0;

    const char* COMMANDLINE_DISCONNECT_NAME = "disconnect";
    const char* COMMANDLINE_DISCONNECT_DESC[] = {
            "disconnect from a TCP/IP device.",
            "Port 26101 is used by default if no port number is specified",
            "Using this command with no additional arguments",
            "will disconnect from all connected TCP/IP devices",
    };
    const int COMMANDLINE_DISCONNECT_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_DISCONNECT_DESC, char*);
    const char* COMMANDLINE_DISCONNECT_ARG_DESC = "[<host>[:<port>]]";
    const int COMMANDLINE_DISCONNECT_MAX_ARG = 1;
    const int COMMANDLINE_DISCONNECT_MIN_ARG = 0;

    const char* COMMANDLINE_CONNECT_NAME = "connect";
    const char* COMMANDLINE_CONNECT_DESC[] = {
            "connect to a device via TCP/IP",
            "Port 26101 is used by default if no port number is specified"
    };
    const int COMMANDLINE_CONNECT_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_CONNECT_DESC, char*);
    const char* COMMANDLINE_CONNECT_ARG_DESC = "<host>[:<port>]";
    const int COMMANDLINE_CONNECT_MAX_ARG = 1;
    const int COMMANDLINE_CONNECT_MIN_ARG = 1;

    const char* COMMANDLINE_GSERIAL_NAME = "get-serialno";
    const char* COMMANDLINE_GSERIAL_DESC[] = {
            "print: <serial-number>"
    };
    const int COMMANDLINE_GSERIAL_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_GSERIAL_DESC, char*);
    const int COMMANDLINE_GSERIAL_MAX_ARG = 0;
    const int COMMANDLINE_GSERIAL_MIN_ARG = 0;

    const char* COMMANDLINE_GSTATE_NAME = "get-state";
    const char* COMMANDLINE_GSTATE_DESC[] = {
            "print: offline | bootloader | device"
    };
    const int COMMANDLINE_GSTATE_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_GSTATE_DESC, char*);
    const int COMMANDLINE_GSTATE_MAX_ARG = 0;
    const int COMMANDLINE_GSTATE_MIN_ARG = 0;

    const char* COMMANDLINE_ROOT_NAME = "root";
    const char* COMMANDLINE_ROOT_DESC[] = {
            "switch to root or developer account mode",
            "'on' means to root mode, and vice versa"
    };
    const int COMMANDLINE_ROOT_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_ROOT_DESC, char*);
    const char* COMMANDLINE_ROOT_ARG_DESC = "<on|off>";
    const int COMMANDLINE_ROOT_MAX_ARG = 1;
    const int COMMANDLINE_ROOT_MIN_ARG = 1;

    const char* COMMANDLINE_SWINDOW_NAME = "status-window";
    const char* COMMANDLINE_SWINDOW_DESC[] = {
            "continuously print device status for a specified device"
    };
    const int COMMANDLINE_SWINDOW_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_SWINDOW_DESC, char*);
    const int COMMANDLINE_SWINDOW_MAX_ARG = 0;
    const int COMMANDLINE_SWINDOW_MIN_ARG = 0;

    const char* COMMANDLINE_SSERVER_NAME = "start-server";
    const char* COMMANDLINE_SSERVER_DESC[] = {
            "ensure that there is a server running"
    };
    const int COMMANDLINE_SSERVER_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_SSERVER_DESC, char*);
    const int COMMANDLINE_SSERVER_MAX_ARG = 0;
    const int COMMANDLINE_SSERVER_MIN_ARG = 0;

    const char* COMMANDLINE_KSERVER_NAME = "kill-server";
    const char* COMMANDLINE_KSERVER_DESC[] = {
            "kill the server if it is running"
    };
    const int COMMANDLINE_KSERVER_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_KSERVER_DESC, char*);
    const int COMMANDLINE_KSERVER_MAX_ARG = 0;
    const int COMMANDLINE_KSERVER_MIN_ARG = 0;

    const char* COMMANDLINE_HELP_NAME = "help";
    const char* COMMANDLINE_HELP_DESC[] = {
            "show this help message"
    };
    const int COMMANDLINE_HELP_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_HELP_DESC, char*);

    const char* COMMANDLINE_VERSION_NAME = "version";
    const char* COMMANDLINE_VERSION_DESC[] = {
            "show version num"
    };
    const int COMMANDLINE_VERSION_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_VERSION_DESC, char*);
    const int COMMANDLINE_VERSION_MAX_ARG = 0;
    const int COMMANDLINE_VERSION_MIN_ARG = 0;

    const char* COMMANDLINE_DLOG_NAME = "dlog";
    const char* COMMANDLINE_DLOG_DESC[] = {
            "view device log"
    };
    const int COMMANDLINE_DLOG_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_DLOG_DESC, char*);
    const char* COMMANDLINE_DLOG_ARG_DESC = "[<filter-spec>]";
    const int COMMANDLINE_DLOG_MAX_ARG = -1;
    const int COMMANDLINE_DLOG_MIN_ARG = 0;

    const char* COMMANDLINE_FORWARD_NAME = "forward";
    const char* COMMANDLINE_FORWARD_DESC[] = {
            "forward socket connections",
            "forward spec is : tcp:<port>"
    };
    const int COMMANDLINE_FORWARD_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_FORWARD_DESC, char*);
    const char* COMMANDLINE_FORWARD_ARG_DESC = "<local> <remote>";
    const int COMMANDLINE_FORWARD_MAX_ARG = 2;
    const int COMMANDLINE_FORWARD_MIN_ARG = 2;

    const char* COMMANDLINE_PUSH_NAME = "push";
    const char* COMMANDLINE_PUSH_DESC[] = {
            "copy file/dir to device",
            "(--with-utf8 means to create the remote file with utf-8 character encoding)"
    };
    const int COMMANDLINE_PUSH_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_PUSH_DESC, char*);
    const char* COMMANDLINE_PUSH_ARG_DESC = "<local> <remote> [--with-utf8]";
    const int COMMANDLINE_PUSH_MAX_ARG = -1;
    const int COMMANDLINE_PUSH_MIN_ARG = 2;

    const char* COMMANDLINE_PULL_NAME = "pull";
    const char* COMMANDLINE_PULL_DESC[] = {
            "copy file/dir from device"
    };
    const int COMMANDLINE_PULL_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_PULL_DESC, char*);
    const char* COMMANDLINE_PULL_ARG_DESC = "<remote> [<local>]";
    const int COMMANDLINE_PULL_MAX_ARG = -2;
    const int COMMANDLINE_PULL_MIN_ARG = 1;

    const char* COMMANDLINE_SHELL_NAME = "shell";
    const char* COMMANDLINE_SHELL_DESC[] = {
            "run remote shell interactively"
    };
    const int COMMANDLINE_SHELL_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_SHELL_DESC, char*);
    const char* COMMANDLINE_SHELL_ARG_DESC = "[command]";
    const int COMMANDLINE_SHELL_MAX_ARG = -1;
    const int COMMANDLINE_SHELL_MIN_ARG = 0;

    const char* COMMANDLINE_INSTALL_NAME = "install";
    const char* COMMANDLINE_INSTALL_DESC[] = {
            "push package file and install it"
    };
    const int COMMANDLINE_INSTALL_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_INSTALL_DESC, char*);
    const char* COMMANDLINE_INSTALL_ARG_DESC = "<pkg_path>";
    const int COMMANDLINE_INSTALL_MAX_ARG = 1;
    const int COMMANDLINE_INSTALL_MIN_ARG = 1;

    const char* COMMANDLINE_UNINSTALL_NAME = "uninstall";
    const char* COMMANDLINE_UNINSTALL_DESC[] = {
            "uninstall an app from the device"
    };
    const int COMMANDLINE_UNINSTALL_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_UNINSTALL_DESC, char*);
    const char* COMMANDLINE_UNINSTALL_ARG_DESC = "<app_id>";
    const int COMMANDLINE_UNINSTALL_MAX_ARG = 1;
    const int COMMANDLINE_UNINSTALL_MIN_ARG = 1;

    const char* COMMANDLINE_FORKSERVER_NAME = "fork-server";
    const int COMMANDLINE_FORKSERVER_MAX_ARG = 1;
    const int COMMANDLINE_FORKSERVER_MIN_ARG = 1;

    const char* COMMANDLINE_SERIAL_SHORT_OPT = "s";
    const char* COMMANDLINE_SERIAL_LONG_OPT = "serial";
    const char* COMMANDLINE_SERIAL_DESC[] = {
            "direct command to the USB device or emulator with the given serial number."
    };
    const int COMMANDLINE_SERIAL_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_SERIAL_DESC, char*);
    const char* COMMANDLINE_SERIAL_ARG_DESC = "<serial number>";
    const int COMMANDLINE_SERIAL_HAS_ARG = 1;

    const char* COMMANDLINE_DEVICE_SHORT_OPT = "d";
    const char* COMMANDLINE_DEVICE_LONG_OPT = "device";
    const char* COMMANDLINE_DEVICE_DESC[] = {
            "direct command to the only connected USB device.",
            "return an error if more than one USB device is present."
    };
    const int COMMANDLINE_DEVICE_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_DEVICE_DESC, char*);
    const int COMMANDLINE_DEVICE_HAS_ARG = 0;

    const char* COMMANDLINE_EMULATOR_SHORT_OPT = "e";
    const char* COMMANDLINE_EMULATOR_LONG_OPT = "emulator";
    const char* COMMANDLINE_EMULATOR_DESC[] = {
            "direct command to the only running emulator.",
            "return an error if more than one emulator is running."
    };
    const int COMMANDLINE_EMULATOR_DESC_SIZE = GET_ARRAY_SIZE(COMMANDLINE_EMULATOR_DESC, char*);
    const int COMMANDLINE_EMULATOR_HAS_ARG = 0;

    const char* COMMANDLINE_ERROR_ARG_MISSING = "argument %s is missing for command %s";
