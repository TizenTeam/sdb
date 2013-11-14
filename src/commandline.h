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

#ifndef COMMANDLINE_H_
#define COMMANDLINE_H_

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#ifndef MAX_INPUT
#define MAX_INPUT 255
#endif

#ifndef INPUT_FD
#define INPUT_FD 0
#endif

int send_shellcommand(char* buf, void** extargv);
int process_cmdline(int argc, char** argv);
void read_and_dump(int fd);
int interactive_shell(void** extargv);
int get_server_port();
int __sdb_command(const char* cmd, void** extargv);

#endif /* COMMANDLINE_H_ */
