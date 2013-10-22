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

#ifndef SDB_CONSTANTS_H_
#define SDB_CONSTANTS_H_

#define GET_ARRAY_SIZE(parameter, type) sizeof(parameter)/sizeof(type)

enum host_type {
    host,
    transport
};

typedef enum host_type HOST_TYPE;

    extern const char* SDB_LAUNCH_SCRIPT;
    extern const char* HELP_APPEND_STR;

    extern const char* EMPTY_STRING;
    extern const char* WHITE_SPACE;
    extern const char* DIR_APP_TMP;
    extern const char *DIR_APP;
    extern const char* QUOTE_CHAR;

    extern const char* PREFIX_HOST;
    extern const char* PREFIX_HOST_ANY;
    extern const char* PREFIX_HOST_USB;
    extern const char* PREFIX_HOST_LOCAL;
    extern const char* PREFIX_HOST_SERIAL;

    extern const char* PREFIX_TRANSPORT_ANY;
    extern const char* PREFIX_TRANSPORT_USB;
    extern const char* PREFIX_TRANSPORT_LOCAL;
    extern const char* PREFIX_TRANSPORT_SERIAL;

    extern const char* COMMANDLINE_MSG_FULL_CMD;

    extern const char* COMMANDLINE_OPROFILE_NAME;
    extern const int COMMANDLINE_OPROFILE_MAX_ARG;
    extern const int COMMANDLINE_OPROFILE_MIN_ARG;

    extern const char* COMMANDLINE_DA_NAME;
    extern const int COMMANDLINE_DA_MAX_ARG;
    extern const int COMMANDLINE_DA_MIN_ARG;

    extern const char* COMMANDLINE_LAUNCH_NAME;
    extern const int COMMANDLINE_LAUNCH_MAX_ARG;
    extern const int COMMANDLINE_LAUNCH_MIN_ARG;

    extern const char* COMMANDLINE_DEVICES_NAME;
    extern const char* COMMANDLINE_DEVICES_DESC[];
    extern const int COMMANDLINE_DEVICES_DESC_SIZE;
    extern const int COMMANDLINE_DEVICES_MAX_ARG;
    extern const int COMMANDLINE_DEVICES_MIN_ARG;

    extern const char* COMMANDLINE_DISCONNECT_NAME;
    extern const char* COMMANDLINE_DISCONNECT_DESC[];
    extern const int COMMANDLINE_DISCONNECT_DESC_SIZE;
    extern const char* COMMANDLINE_DISCONNECT_ARG_DESC;
    extern const int COMMANDLINE_DISCONNECT_MAX_ARG;
    extern const int COMMANDLINE_DISCONNECT_MIN_ARG;

    extern const char* COMMANDLINE_CONNECT_NAME;
    extern const char* COMMANDLINE_CONNECT_DESC[];
    extern const int COMMANDLINE_CONNECT_DESC_SIZE;
    extern const char* COMMANDLINE_CONNECT_ARG_DESC;
    extern const int COMMANDLINE_CONNECT_MAX_ARG;
    extern const int COMMANDLINE_CONNECT_MIN_ARG;

    extern const char* COMMANDLINE_DEVICE_CON_NAME;
    extern const char* COMMANDLINE_DEVICE_CON_DESC[];
    extern const int COMMANDLINE_DEVICE_CON_DESC_SIZE;
    extern const char* COMMANDLINE_DEVICE_CON_ARG_DESC;
    extern const int COMMANDLINE_DEVICE_CON_MAX_ARG;
    extern const int COMMANDLINE_DEVICE_CON_MIN_ARG;

    extern const char* COMMANDLINE_GSERIAL_NAME;
    extern const char* COMMANDLINE_GSERIAL_DESC[];
    extern const int COMMANDLINE_GSERIAL_DESC_SIZE;
    extern const int COMMANDLINE_GSERIAL_MAX_ARG;
    extern const int COMMANDLINE_GSERIAL_MIN_ARG;

    extern const char* COMMANDLINE_GSTATE_NAME;
    extern const char* COMMANDLINE_GSTATE_DESC[];
    extern const int COMMANDLINE_GSTATE_DESC_SIZE;
    extern const int COMMANDLINE_GSTATE_MAX_ARG;
    extern const int COMMANDLINE_GSTATE_MIN_ARG;

    extern const char* COMMANDLINE_ROOT_NAME;
    extern const char* COMMANDLINE_ROOT_DESC[];
    extern const int COMMANDLINE_ROOT_DESC_SIZE;
    extern const char* COMMANDLINE_ROOT_ARG_DESC;
    extern const int COMMANDLINE_ROOT_MAX_ARG;
    extern const int COMMANDLINE_ROOT_MIN_ARG;

    extern const char* COMMANDLINE_SWINDOW_NAME;
    extern const char* COMMANDLINE_SWINDOW_DESC[];
    extern const int COMMANDLINE_SWINDOW_DESC_SIZE;
    extern const int COMMANDLINE_SWINDOW_MAX_ARG;
    extern const int COMMANDLINE_SWINDOW_MIN_ARG;

    extern const char* COMMANDLINE_SSERVER_NAME;
    extern const char* COMMANDLINE_SSERVER_DESC[];
    extern const int COMMANDLINE_SSERVER_DESC_SIZE;
    extern const int COMMANDLINE_SSERVER_MAX_ARG;
    extern const int COMMANDLINE_SSERVER_MIN_ARG;

    extern const char* COMMANDLINE_KSERVER_NAME;
    extern const char* COMMANDLINE_KSERVER_DESC[];
    extern const int COMMANDLINE_KSERVER_DESC_SIZE;
    extern const int COMMANDLINE_KSERVER_MAX_ARG;
    extern const int COMMANDLINE_KSERVER_MIN_ARG;

    extern const char* COMMANDLINE_HELP_NAME;
    extern const char* COMMANDLINE_HELP_DESC[];
    extern const int COMMANDLINE_HELP_DESC_SIZE;

    extern const char* COMMANDLINE_VERSION_NAME;
    extern const char* COMMANDLINE_VERSION_DESC[];
    extern const int COMMANDLINE_VERSION_DESC_SIZE;
    extern const int COMMANDLINE_VERSION_MAX_ARG;
    extern const int COMMANDLINE_VERSION_MIN_ARG;

    extern const char* COMMANDLINE_FORWARD_NAME;
    extern const char* COMMANDLINE_FORWARD_DESC[];
    extern const int COMMANDLINE_FORWARD_DESC_SIZE;
    extern const char* COMMANDLINE_FORWARD_ARG_DESC;
    extern const int COMMANDLINE_FORWARD_MAX_ARG;
    extern const int COMMANDLINE_FORWARD_MIN_ARG;

    extern const char* COMMANDLINE_DLOG_NAME;
    extern const char* COMMANDLINE_DLOG_DESC[];
    extern const int COMMANDLINE_DLOG_DESC_SIZE;
    extern const char* COMMANDLINE_DLOG_ARG_DESC;
    extern const int COMMANDLINE_DLOG_MAX_ARG;
    extern const int COMMANDLINE_DLOG_MIN_ARG;

    extern const char* COMMANDLINE_PUSH_NAME;
    extern const char* COMMANDLINE_PUSH_DESC[];
    extern const int COMMANDLINE_PUSH_DESC_SIZE;
    extern const char* COMMANDLINE_PUSH_ARG_DESC;
    extern const int COMMANDLINE_PUSH_MAX_ARG;
    extern const int COMMANDLINE_PUSH_MIN_ARG;

    extern const char* COMMANDLINE_PULL_NAME;
    extern const char* COMMANDLINE_PULL_DESC[];
    extern const int COMMANDLINE_PULL_DESC_SIZE;
    extern const char* COMMANDLINE_PULL_ARG_DESC;
    extern const int COMMANDLINE_PULL_MAX_ARG;
    extern const int COMMANDLINE_PULL_MIN_ARG;

    extern const char* COMMANDLINE_SHELL_NAME;
    extern const char* COMMANDLINE_SHELL_DESC[];
    extern const int COMMANDLINE_SHELL_DESC_SIZE;
    extern const char* COMMANDLINE_SHELL_ARG_DESC;
    extern const int COMMANDLINE_SHELL_MAX_ARG;
    extern const int COMMANDLINE_SHELL_MIN_ARG;

    extern const char* COMMANDLINE_INSTALL_NAME;
    extern const char* COMMANDLINE_INSTALL_DESC[];
    extern const int COMMANDLINE_INSTALL_DESC_SIZE;
    extern const char* COMMANDLINE_INSTALL_ARG_DESC;
    extern const int COMMANDLINE_INSTALL_MAX_ARG;
    extern const int COMMANDLINE_INSTALL_MIN_ARG;

    extern const char* COMMANDLINE_UNINSTALL_NAME;
    extern const char* COMMANDLINE_UNINSTALL_DESC[];
    extern const int COMMANDLINE_UNINSTALL_DESC_SIZE;
    extern const char* COMMANDLINE_UNINSTALL_ARG_DESC;
    extern const int COMMANDLINE_UNINSTALL_MAX_ARG;
    extern const int COMMANDLINE_UNINSTALL_MIN_ARG;

    extern const char* COMMANDLINE_FORKSERVER_NAME;
    extern const int COMMANDLINE_FORKSERVER_MAX_ARG;
    extern const int COMMANDLINE_FORKSERVER_MIN_ARG;

    extern const char* COMMANDLINE_SERIAL_SHORT_OPT;
    extern const char* COMMANDLINE_SERIAL_LONG_OPT;
    extern const char* COMMANDLINE_SERIAL_DESC[];
    extern const int COMMANDLINE_SERIAL_DESC_SIZE;
    extern const char* COMMANDLINE_SERIAL_ARG_DESC;
    extern const int COMMANDLINE_SERIAL_HAS_ARG;

    extern const char* COMMANDLINE_DEVICE_SHORT_OPT;
    extern const char* COMMANDLINE_DEVICE_LONG_OPT;
    extern const char* COMMANDLINE_DEVICE_DESC[];
    extern const int COMMANDLINE_DEVICE_DESC_SIZE;
    extern const int COMMANDLINE_DEVICE_HAS_ARG;

    extern const char* COMMANDLINE_EMULATOR_SHORT_OPT;
    extern const char* COMMANDLINE_EMULATOR_LONG_OPT;
    extern const char* COMMANDLINE_EMULATOR_DESC[];
    extern const int COMMANDLINE_EMULATOR_DESC_SIZE;
    extern const int COMMANDLINE_EMULATOR_HAS_ARG;

    extern const char* COMMANDLINE_ERROR_ARG_MISSING;

    extern const char* STATE_OFFLINE;
    extern const char* STATE_BOOTLOADER;
    extern const char* STATE_DEVICE;
    extern const char* STATE_HOST;
    extern const char* STATE_RECOVERY;
    extern const char* STATE_SIDELOAD;
    extern const char* STATE_NOPERM;
    extern const char* STATE_LOCKED;
    extern const char* STATE_UNKNOWN;

    extern const char* TRANSPORT_ERR_MORE_THAN_ONE_TARGET;
    extern const char* TRANSPORT_ERR_MORE_THAN_ONE_EMUL;
    extern const char* TRANSPORT_ERR_MORE_THAN_ONE_DEV;
    extern const char* TRANSPORT_ERR_TARGET_OFFLINE;
    extern const char* TRANSPORT_ERR_TARGET_NOT_FOUND;

#endif /* SDB_CONSTANTS_H_*/
