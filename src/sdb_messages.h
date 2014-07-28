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
#ifndef SDB_MESSAGES_H_
#define SDB_MESSAGES_H_

extern const char* NO_ERR;
extern const char* ERR_GENERAL_CONNECTION_FAIL;
extern const char* ERR_GENERAL_UNKNOWN;
extern const char* ERR_GENERAL_INITIALIZE_ENV_FAIL;
extern const char* ERR_GENERAL_EMPTY_SERVICE_NAME;
extern const char* ERR_GENERAL_TOO_LONG_SERVICE_NAME;
extern const char* ERR_GENERAL_INVALID_SERVICE_NAME;
extern const char* ERR_GENERAL_WRITE_MESSAGE_SIZE_FAIL;
extern const char* ERR_GENERAL_INVALID_PORT;
extern const char* ERR_GENERAL_START_SERVER_FAIL;
extern const char* ERR_GENERAL_SERVER_NOT_RUN;
extern const char* ERR_GENERAL_LOG_FAIL;
extern const char* ERR_GENERAL_DUPLICATE_FAIL;
extern const char* ERR_GENERAL_LAUNCH_APP_FAIL;
extern const char* ERR_GENERAL_KILL_SERVER_FAIL;
extern const char* ERR_GENERAL_SET_FD_FAIL;
extern const char* ERR_GENERAL_INVALID_IP;
extern const char* ERR_GENERAL_NO_READ_PERMISSION;
extern const char* ERR_GENERAL_NO_WRITE_PERMISSION;
extern const char* ERR_GENERAL_NO_EXECUTE_PERMISSION;
extern const char* ERR_GENERAL_PROTOCOL_WRONG_ID;
extern const char* ERR_GENERAL_PROTOCOL_DATA_OVERRUN;

extern const char* ERR_SYNC_NOT_FILE;
extern const char* ERR_SYNC_OPEN_CHANNEL_FAIL;
extern const char* ERR_SYNC_STAT_FAIL;
extern const char* ERR_SYNC_GET_DIRLIST_FAIL;
extern const char* ERR_SYNC_READ_FAIL;
extern const char* ERR_SYNC_OPEN_FAIL;
extern const char* ERR_SYNC_CREATE_FAIL;
extern const char* ERR_SYNC_CLOSE_FAIL;
extern const char* ERR_SYNC_WRITE_FAIL;
extern const char* ERR_SYNC_COPY_FAIL;
extern const char* ERR_SYNC_TOO_LONG_FILENAME;
extern const char* ERR_SYNC_INVALID_FILENAME;
extern const char* ERR_SYNC_UNKNOWN_TYPE_FILE;
extern const char* ERR_SYNC_NOT_EXIST_FILE;
extern const char* ERR_SYNC_NOT_DIRECTORY;
extern const char* ERR_SYNC_LOCKED;
extern const char* ERR_SYNC_CANNOT_ACCESS;

extern const char* ERR_CONNECT_MORE_THAN_ONE_TARGET;
extern const char* ERR_CONNECT_MORE_THAN_ONE_EMUL;
extern const char* ERR_CONNECT_MORE_THAN_ONE_DEV;
extern const char* ERR_CONNECT_TARGET_NOT_FOUND;
extern const char* ERR_CONNECT_TARGET_OFFLINE;
extern const char* ERR_CONNECT_TARGET_LOCKED;
extern const char* ERR_CONNECT_TARGET_SUSPENDED;
extern const char* ERR_CONNECT_CONNECT_REMOTE_TARGET_FAILED;
extern const char* ERR_CONNECT_TARGET_NO_RESPONSE;
extern const char* ERR_CONNECT_WRONG_SERIAL;

extern const char* ERR_COMMAND_MISSING_ARGUMENT;
extern const char* ERR_COMMAND_TOO_FEW_ARGUMENTS;
extern const char* ERR_COMMAND_TOO_MANY_ARGUMENTS;
extern const char* ERR_COMMAND_RUN_COMMAND_FAILED;
extern const char* ERR_COMMAND_COMMAND_NO_SUPPORT;
extern const char* ERR_COMMAND_OPTION_NO_SUPPORT;
extern const char* ERR_COMMAND_OPTION_MUST_HAVE_ARGUMENT;
extern const char* ERR_FORWARD_UNSUPPORT_TRANSMISSION_PROTOCOL;
extern const char* ERR_FORWARD_INSTALL_FAIL;
extern const char* ERR_FORWARD_REMOVE_FAIL;
extern const char* ERR_FORWARD_INVALID_PROTOCOL;
extern const char* ERR_FORWARD_BIND_PORT_FAIL;
extern const char* ERR_PACKAGE_TYPE_UNKNOWN;
extern const char* ERR_PACKAGE_GET_TEMP_PATH_FAIL;
extern const char* ERR_PACKAGE_GET_TYPE_FAIL;
extern const char* ERR_PACKAGE_ID_NOT_EXIST;
extern const char* ERR_PACKAGE_ID_NOT_UNIQUE;
extern const char* ERR_LAUNCH_M_OPTION_SUPPORT;
extern const char* ERR_LAUNCH_M_OPTION_ARGUMENT;
extern const char* ERR_LAUNCH_P_OPTION_DEBUG_MODE;
extern const char* ERR_LAUNCH_ATTACH_OPTION_DEBUG_MODE;

#endif /* SDB_MESSAGE_H_ */
