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

#ifndef SDB_MODEL_H_
#define SDB_MODEL_H_

#include "linkedlist.h"
#include "sdb_constants.h"

#define TRACE_TAG TRACE_SDB

struct command {
    const char* name;
    //hidden command does not have a description.
    int desc_size;
    const char** desc;
    const char* argdesc;
    int (*Func)(int, char**);
    // -1 means no max limit.
    int maxargs;
    int minargs;
};
typedef struct command COMMAND;

struct option {
    const char* longopt;
    const char* shortopt;
    //hidden option does not have a description.
    const char** desc;
    int desc_size;
    const char* argdesc;
    int hasarg;
};
typedef struct option OPTION;

struct input_option {
    char* value;
    OPTION* option;
};
typedef struct input_option INPUT_OPTION;

const COMMAND NULL_COMMAND;

int null_function (int argc, char** argv);
void create_command(COMMAND** cmdptr, const char* name, const char** desc, int desc_size, const char* argdesc,
        int (*Func)(int, char**), int maxargs, int minargs);

void create_option(OPTION** optptr, const char* longopt, const char* shortopt, const char** desc, int desc_size, const char* argdesc, int hasarg);
COMMAND* get_command(LIST_NODE* cmd_list, char* name);
OPTION* get_option(LIST_NODE* opt_list, char* name, int longname);
INPUT_OPTION* get_inputopt(LIST_NODE* inputopt_list, char* shortname);
int parse_opt(int argc, char** argv, LIST_NODE* opt_list, LIST_NODE** result_list);

#endif /* SDB_MODEL_H_ */
