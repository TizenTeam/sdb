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
* See the License for the specific language governing permissions and
* limitations under the License.
*
* Contributors:
* - S-Core Co., Ltd
*
*/

#ifndef COMMAND_FUNCTION_H_
#define COMMAND_FUNCTION_H_

#define  TRACE_TAG  TRACE_SDB

#ifdef OS_WINDOWS
#define PATH_MAX 4096
#endif


int da(int argc, char ** argv, void** extargv);
int oprofile(int argc, char ** argv, void** extargv);
int launch(int argc, char ** argv, void** extargv);
int devices(int argc, char ** argv, void** extargv);
int __disconnect(int argc, char ** argv, void** extargv);
int __connect(int argc, char ** argv, void** extargv);
int get_state_serialno(int argc, char ** argv, void** extargv);
int root(int argc, char ** argv, void** extargv);
int status_window(int argc, char ** argv, void** extargv);
int start_server(int argc, char ** argv, void** extargv);
int kill_server(int argc, char ** argv, void** extargv);
int version(int argc, char ** argv, void** extargv);
int forward(int argc, char ** argv, void** extargv);
int push(int argc, char ** argv, void** extargv);
int pull(int argc, char ** argv, void** extargv);
int dlog(int argc, char ** argv, void** extargv);
int install(int argc, char **argv, void** extargv);
int uninstall(int argc, char **argv, void** extargv);
int forkserver(int argc, char** argv, void** extargv);
int shell(int argc, char ** argv, void** extargv);

#endif /* COMMAND_FUNCTION_H_ */
