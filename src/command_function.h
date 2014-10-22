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
#undef PATH_MAX
#define PATH_MAX 4096
#endif

int da(int argc, char ** argv);
int oprofile(int argc, char ** argv);
int profile(int argc, char ** argv);
int launch(int argc, char ** argv);
int devices(int argc, char ** argv);
int __disconnect(int argc, char ** argv);
int __connect(int argc, char ** argv);
int device_con(int argc, char ** argv);
int get_state_serialno(int argc, char ** argv);
int root(int argc, char ** argv);
int status_window(int argc, char ** argv);
int start_server(int argc, char ** argv);
int kill_server(int argc, char ** argv);
int version(int argc, char ** argv);
int forward(int argc, char ** argv);
int forward_list();
int forward_remove(char *local);
int forward_remove_all();
int push(int argc, char ** argv);
int pull(int argc, char ** argv);
int dlog(int argc, char ** argv);
int install(int argc, char **argv);
int uninstall(int argc, char **argv);
int forkserver(int argc, char** argv);
int shell(int argc, char ** argv);

#endif /* COMMAND_FUNCTION_H_ */
