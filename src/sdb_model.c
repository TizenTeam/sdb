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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "utils.h"
#include "fdevent.h"
#include "sdb_model.h"
#include "linkedlist.h"
#include "log.h"

const COMMAND NULL_COMMAND = {
        NULL,
        0,
        NULL,
        NULL,
        null_function,
        -1,
        0
};

//Create a command. A user should free cmdptr manually.
void create_command(COMMAND** cmdptr, const char* name, const char** desc, int desc_size, const char* argdesc,
        int (*Func)(int, char**, void**), int maxargs, int minargs) {
    *cmdptr = (COMMAND*)malloc(sizeof(COMMAND));
    (*cmdptr)->name = name;
    (*cmdptr)->desc = desc;
    (*cmdptr)->desc_size = desc_size;
    (*cmdptr)->argdesc = argdesc;
    (*cmdptr)->Func = Func;
    (*cmdptr)->maxargs = maxargs;
    (*cmdptr)->minargs = minargs;
}

//Create a command. A user should free optptr manually.
void create_option(OPTION** optptr, const char* longopt, const char* shortopt, const char** desc,
        int desc_size, const char* argdesc, int hasarg) {
    *optptr = (OPTION*)malloc(sizeof(OPTION));
    (*optptr)->longopt = longopt;
    (*optptr)->shortopt = shortopt;
    (*optptr)->desc = desc;
    (*optptr)->desc_size = desc_size;
    (*optptr)->argdesc = argdesc;
    (*optptr)->hasarg = hasarg;
}

COMMAND* get_command(LIST_NODE* cmd_list, char* name) {

    LIST_NODE* curptr = cmd_list;

    while(curptr != NULL) {
        COMMAND* curcmd = (COMMAND*)(curptr->data);

        if(!strcmp(curcmd->name, name)) {
            return curcmd;
        }
        curptr = curptr->next_ptr;
    }
    return (COMMAND*)&NULL_COMMAND;
}

OPTION* get_option(LIST_NODE* opt_list, char* name, int longname) {

    LIST_NODE* curptr = opt_list;

    while(curptr != NULL) {
        OPTION* curopt = (OPTION*)(curptr->data);
        if(longname) {
            if(!strcmp(curopt->longopt, name)) {
                return curopt;
            }
        }
        else if(!strcmp(curopt->shortopt, name)) {
            return curopt;
        }
        curptr = curptr->next_ptr;
    }
    return NULL;
}

INPUT_OPTION* get_inputopt(LIST_NODE* inputopt_list, char* shortname) {
    LIST_NODE* curptr = inputopt_list;

    while(curptr != NULL) {
        INPUT_OPTION* cur_inputopt = (INPUT_OPTION*)(curptr->data);

        if(!strcmp(cur_inputopt->option->shortopt, shortname)) {
                return cur_inputopt;
        }

        curptr = curptr->next_ptr;
    }

    return NULL;
}

int parse_opt(int argc, char** argv, LIST_NODE* opt_list, LIST_NODE** result_list) {

    D("Parsing options.\n");
    int pass_arg = 0;
    while(argc > 0) {
        if(argv[0][0] == '-') {
            int local_pass_arg = 1;
            int longname = 0;
            char* name = NULL;
            //long name
            if(strlen(argv[0]) > 1 && argv[0][1] == '-') {
                    longname = 1;
                    name = argv[0] + 2;
            }
            else {
                name = argv[0] + 1;
            }
            D("Parse option: %s with longname %d\n", name, longname);
            OPTION* option = get_option(opt_list, name, longname);
            if(option == NULL) {
                fprintf(stderr, "unrecognized option: %s\n", name);
                return -1;
            }

            char* value = NULL;
            if(option->hasarg) {
                if(argc > 1 && argv[1][0] != '-') {
                    value = argv[1];
                    local_pass_arg++;
                }
                else {
                    fprintf(stderr, "option: %s should have a argument\n", name);
                    return -1;
                }
            }
            D("Option %s with value %s", name, value);
            INPUT_OPTION* input = (INPUT_OPTION*)malloc(sizeof(INPUT_OPTION));
            input->option = option;
            input->value = value;
            prepend(result_list, input);

            argv = argv + local_pass_arg;
            argc = argc - local_pass_arg;
            pass_arg = pass_arg + local_pass_arg;
        }
        else {
            return pass_arg;
        }
    }
    return pass_arg;
}

int null_function (int argc, char** argv, void** extraarg) {
    fprintf(stderr, "unsupported command: %s\n", argv[0]);
    return -1;
}
