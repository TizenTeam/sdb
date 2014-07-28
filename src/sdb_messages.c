/*
 * SDB - Smart Development Bridge
 *
 * Copyright (c) 2000 - 2014 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Shingil Kang <shingil.kang@samsung.com>
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
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 * - S-Core Co., Ltd
 *
 */
#include "sdb_messages.h"

const char* NO_ERR = "no error, successful exit";
const char* ERR_GENERAL_CONNECTION_FAIL = "connection fails";
const char* ERR_GENERAL_UNKNOWN = "unknown reason";
const char* ERR_GENERAL_INITIALIZE_ENV_FAIL = "failed to initialize SBD environment";
const char* ERR_GENERAL_EMPTY_SERVICE_NAME = "empty service name";
const char* ERR_GENERAL_TOO_LONG_SERVICE_NAME = "service name too long";
const char* ERR_GENERAL_INVALID_SERVICE_NAME = "invalid service name";
const char* ERR_GENERAL_WRITE_MESSAGE_SIZE_FAIL =  "failed to write message size";
const char* ERR_GENERAL_INVALID_PORT = "invalid port '%s'";
const char* ERR_GENERAL_START_SERVER_FAIL = "failed to start server";
const char* ERR_GENERAL_SERVER_NOT_RUN = "server not running";
const char* ERR_GENERAL_LOG_FAIL = "failed to log";
const char* ERR_GENERAL_DUPLICATE_FAIL = "failed to duplicate '%s'";
const char* ERR_GENERAL_LAUNCH_APP_FAIL = "failed to launch application";
const char* ERR_GENERAL_KILL_SERVER_FAIL = "failed to kill server";
const char* ERR_GENERAL_SET_FD_FAIL = "cannot set the FD status";
const char* ERR_GENERAL_INVALID_IP = "invalid IP '%s'";
const char* ERR_GENERAL_NO_READ_PERMISSION = "no read permission";
const char* ERR_GENERAL_NO_WRITE_PERMISSION =  "no write permission";
const char* ERR_GENERAL_NO_EXECUTE_PERMISSION =  "no execute permission";
const char* ERR_GENERAL_PROTOCOL_WRONG_ID =  "protocol error; expected '%s' but got '%s'";
const char* ERR_GENERAL_PROTOCOL_DATA_OVERRUN   = "protocol error; data length '%d' overruns '%d'";

const char* ERR_SYNC_NOT_FILE  = "'%s' is not a file";
const char* ERR_SYNC_OPEN_CHANNEL_FAIL  = "failed to open sync channel";
const char* ERR_SYNC_STAT_FAIL = "failed to get status of '%s'";
const char* ERR_SYNC_GET_DIRLIST_FAIL = "failed to read directory list of '%s'";
const char* ERR_SYNC_READ_FAIL = "failed to read '%s'";
const char* ERR_SYNC_OPEN_FAIL = "failed to open '%s'";
const char* ERR_SYNC_CREATE_FAIL = "failed to create '%s'";
const char* ERR_SYNC_CLOSE_FAIL = "failed to close '%s'";
const char* ERR_SYNC_WRITE_FAIL = "failed to write '%s'";
const char* ERR_SYNC_COPY_FAIL =  "failed to copy";
const char* ERR_SYNC_TOO_LONG_FILENAME = "file name too long";
const char* ERR_SYNC_INVALID_FILENAME = "invalid file name";
const char* ERR_SYNC_UNKNOWN_TYPE_FILE = "invalid file type";
const char* ERR_SYNC_NOT_EXIST_FILE = "'%s' does not exist";
const char* ERR_SYNC_NOT_DIRECTORY = "'%s' is not a directory";
const char* ERR_SYNC_LOCKED  = "'%s' is locked";
const char* ERR_SYNC_CANNOT_ACCESS = "cannot access '%s'";

const char* ERR_CONNECT_MORE_THAN_ONE_TARGET = "more than one target found. Specify the target with -s option.";
const char* ERR_CONNECT_MORE_THAN_ONE_EMUL = "more than one emulator found. Specify the emulator with -e option.";
const char* ERR_CONNECT_MORE_THAN_ONE_DEV = "more than one device found. Specify the device with -d option.";
const char* ERR_CONNECT_TARGET_NOT_FOUND = "target not found";
const char* ERR_CONNECT_TARGET_OFFLINE = "target offline";
const char* ERR_CONNECT_TARGET_LOCKED = "target locked";
const char* ERR_CONNECT_TARGET_SUSPENDED = "target suspended";
const char* ERR_CONNECT_CONNECT_REMOTE_TARGET_FAILED = "failed to connect to remote target '%s'";
const char* ERR_CONNECT_TARGET_NO_RESPONSE = "no response from target";
const char* ERR_CONNECT_WRONG_SERIAL = "serial number '%s' wrong";

const char* ERR_COMMAND_MISSING_ARGUMENT = "missing arguments for '%s' command";
const char* ERR_COMMAND_TOO_FEW_ARGUMENTS = "too few arguments";
const char* ERR_COMMAND_TOO_MANY_ARGUMENTS = "too many arguments";
const char* ERR_COMMAND_RUN_COMMAND_FAILED = "failed to run command";
const char* ERR_COMMAND_COMMAND_NO_SUPPORT = "command '%s' not supported";
const char* ERR_COMMAND_OPTION_NO_SUPPORT =  "option '%s' not supported";
const char* ERR_COMMAND_OPTION_MUST_HAVE_ARGUMENT =  "option '%s' must have an argument";
const char* ERR_FORWARD_UNSUPPORT_TRANSMISSION_PROTOCOL = "unsupported transmission protocol";
const char* ERR_FORWARD_INSTALL_FAIL = "cannot install forward listener";
const char* ERR_FORWARD_REMOVE_FAIL =  "cannot remove forward listener";
const char* ERR_FORWARD_INVALID_PROTOCOL = "invalid protocol";
const char* ERR_FORWARD_BIND_PORT_FAIL = "failed to bind port '%s'";
const char* ERR_PACKAGE_TYPE_UNKNOWN = "package type unknown";
const char* ERR_PACKAGE_GET_TEMP_PATH_FAIL = "failed to get package temporary path";
const char* ERR_PACKAGE_GET_TYPE_FAIL = "failed to get package type";
const char* ERR_PACKAGE_ID_NOT_EXIST = "package ID '%s' does not exist";
const char* ERR_PACKAGE_ID_NOT_UNIQUE = "package ID '%s' not unique";
const char* ERR_LAUNCH_M_OPTION_SUPPORT = "In DA and Oprofile, the -m option is supported in sdbd higher than 2.2.0";
const char* ERR_LAUNCH_M_OPTION_ARGUMENT = "The -m option accepts arguments only for run or debug options";
const char* ERR_LAUNCH_P_OPTION_DEBUG_MODE = "The -P option must be used in debug mode";
const char* ERR_LAUNCH_ATTACH_OPTION_DEBUG_MODE =  "The -attach option must be used in debug mode";
