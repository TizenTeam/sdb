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

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include "sdb_constants.h"
#include "utils.h"
#include "sdb_client.h"
#include "strutils.h"
#include "auto_complete.h"
#include "file_sync_service.h"

static int parse_opt(int argc, char** argv);
static int parse_cmd(int argc, char** argv);
static int parse_emul(int argc, char** argv);
static int parse_dev(int argc, char** argv);
static int parse_serial(int argc, char** argv);
static int no_parse(int argc, char** argv);
static int parse_root(int argc, char** argv);
static int parse_install(int argc, char** argv);
static int parse_uninstall(int argc, char** argv);
static int parse_disconnect(int argc, char** argv);
static int parse_push(int argc, char** argv);
static int parse_pull(int argc, char** argv);
static int parse_shell(int argc, char** argv);
static int initialize_ac(int complete);
static void print_char_with_completion_flag(int argc, char** argv, char** not_complete_char);
static void print_element_with_completion_flag(int argc, AC_ELEMENT** argv, char** not_complete_char);
static void print_local_dirlist_with_complete_flag(int argc, char** argv);
static void print_local_dirlist(char* src_dir, char** not_complete_char);
static void print_remote_dirlist_with_complete_flag(int argc, char** argv);
static void print_remote_dirlist(char* src_dir, char** not_complete_char);

static int COMPLETE_FLAG = 0;
static FILE* AC_STDOUT = NULL;
static FILE* AC_STDERR = NULL;
static const char IFS = '\n';

static struct ac_element emulator_short = {
        .keyword = "-e",
        .func = parse_emul
};

static struct ac_element emulator_long = {
        .keyword = "--emulator",
        .func = parse_emul
};

static struct ac_element device_short = {
        .keyword = "-d",
        .func = parse_dev
};

static struct ac_element device_long = {
        .keyword = "--device",
        .func = parse_dev
};

static struct ac_element serial_short = {
        .keyword = "-s",
        .func = parse_serial
};

static struct ac_element serial_long = {
        .keyword = "--serial",
        .func = parse_serial
};

static struct ac_element ac_root = {
        .keyword = "root",
        .func = parse_root
};

static struct ac_element ac_swindow = {
        .keyword = "status-window",
        .func = no_parse
};

static struct ac_element ac_gserial = {
        .keyword = "get-serialno",
        .func = no_parse
};

static struct ac_element ac_gstate = {
        .keyword = "get-state",
        .func = no_parse
};

static struct ac_element ac_kserver = {
        .keyword = "kill-server",
        .func = no_parse
};

static struct ac_element ac_sserver = {
        .keyword = "start-server",
        .func = no_parse
};

static struct ac_element ac_version = {
        .keyword = "version",
        .func = no_parse
};

static struct ac_element ac_help = {
        .keyword = "help",
        .func = no_parse
};

static struct ac_element ac_forward = {
        .keyword = "forward",
        .func = no_parse
};

static struct ac_element ac_uninstall= {
        .keyword = "uninstall",
        .func = parse_uninstall
};

static struct ac_element ac_install= {
        .keyword = "install",
        .func = parse_install
};

static struct ac_element ac_dlog= {
        .keyword = "install",
        .func = no_parse
};

static struct ac_element ac_shell= {
        .keyword = "shell",
        .func = parse_shell
};

static struct ac_element ac_pull= {
        .keyword = "pull",
        .func = parse_pull
};

static struct ac_element ac_push= {
        .keyword = "push",
        .func = parse_push
};

static struct ac_element ac_disconnect= {
        .keyword = "disconnect",
        .func = parse_disconnect
};

static struct ac_element ac_connect= {
        .keyword = "connect",
        .func = no_parse
};

static struct ac_element ac_devices= {
        .keyword = "devices",
        .func = no_parse
};

static const AC_ELEMENT* pre_options[] = {&emulator_short, &emulator_long, &device_short, &device_long, &serial_short, &serial_long};
static const int pre_options_size = GET_ARRAY_SIZE(pre_options, AC_ELEMENT*);

static const AC_ELEMENT* commands[] = {&ac_root, &ac_swindow , &ac_gserial, &ac_gstate, &ac_kserver, &ac_sserver,
        &ac_version, &ac_help, &ac_forward, &ac_uninstall, &ac_install, &ac_dlog, &ac_shell, &ac_pull, &ac_push, &ac_disconnect,
        &ac_connect, &ac_devices};
static const int cmds_size = GET_ARRAY_SIZE(commands, AC_ELEMENT*);

static int initialize_ac(int complete) {

    COMPLETE_FLAG = complete;
    int ac_stdout_fd = dup(STDOUT_FILENO);

    if(ac_stdout_fd < 0) {
        fprintf(stderr, "error: exception happend while duplicating stdout '%s'\n", strerror(errno));
        return -1;
    }

    AC_STDOUT = fdopen(ac_stdout_fd, "w");

    close_on_exec(ac_stdout_fd);

    int ac_stderr_fd = dup(STDOUT_FILENO);

    if(ac_stderr_fd < 0) {
        fprintf(stderr, "error: exception happend while duplicating stdout '%s'\n", strerror(errno));
        return -1;
    }

    AC_STDERR = fdopen(ac_stderr_fd, "w");

    close_on_exec(ac_stderr_fd);

    int null_fd;
    null_fd = unix_open("/dev/null", O_WRONLY);

    if(null_fd < 0) {
        sdb_close(null_fd);
        fprintf(stderr, "error: exception happend while opening /dev/null '%s'\n", strerror(errno));
        return -1;
    }

    if(dup2(null_fd, STDOUT_FILENO) < 0){
        sdb_close(null_fd);
        fprintf(stderr, "error: exception happend while duplicating /dev/null to the stdout '%s'\n", strerror(errno));
        return -1;
    }

    if(dup2(null_fd, STDERR_FILENO) < 0){
        sdb_close(null_fd);
        fprintf(stderr, "error: exception happend while duplicating /dev/null to the stderr '%s'\n", strerror(errno));
        return -1;
    }

    sdb_close(null_fd);
    return 0;
}

int auto_complete(int argc, char** argv, int complete) {

    if(initialize_ac(complete)) {
        return -1;
    }
    if(!parse_opt(argc, argv)) {
        return 1;
    }
    parse_cmd(argc, argv);
    return 1;
}

static int parse_emul(int argc, char** argv) {
    target_ttype = kTransportLocal;
    parse_cmd(argc, argv);

    return -1;
}

static int parse_dev(int argc, char** argv) {
    target_ttype = kTransportUsb;
    parse_cmd(argc, argv);

    return -1;
}

static int parse_serial(int argc, char** argv) {
    if(argc == 0) {

        char* tmp = sdb_query("host:devices");
        if(tmp) {
            char* tokens[MAX_TOKENS];
            int devices_number = tokenize(tmp, "\n", tokens, MAX_TOKENS);
            int i = 0;
            for(; i<devices_number; i++) {
                char* tab = strchr(tokens[i], '\t');
                if(tab != NULL) {
                    *tab = '\0';
                }
                rtrim(tokens[i]);
            }
            print_char_with_completion_flag(devices_number, tokens, argv);
        }

        return 0;
    }

    target_serial = argv[0];
    parse_cmd(argc - 1, argv + 1);
    return -1;
}

static int no_parse(int argc, char** argv) {
    return 0;
}

static int parse_root(int argc, char** argv) {
    if(argc == 0) {
        char* root_options[] = {"on", "off"};
        int ropt_size = GET_ARRAY_SIZE(root_options, char*);
        print_char_with_completion_flag(ropt_size, (char**)root_options, argv);
    }
    return -1;
}

static int parse_uninstall(int argc, char** argv) {

    if(argc == 0) {
        char full_cmd[255];
        if(COMPLETE_FLAG) {
            snprintf(full_cmd, sizeof full_cmd, "shell:/usr/bin/pkgcmd -l | grep -E '\\[tpk\\]|\\[wgt\\]' | awk '{print $4}'");
        }
        else {
            snprintf(full_cmd, sizeof full_cmd, "shell:/usr/bin/pkgcmd -l | grep -E '\\[tpk\\]|\\[wgt\\]' | awk '{print $4}' | grep '^\\[%s'", argv[0]);
        }

        int result = sdb_connect(full_cmd);
        if(result < 0) {
            return -1;
        }

        char pkg_ids[10000];
        int lines = read_lines(result, pkg_ids, 10000);

        char** pkg_id_tokens = malloc(sizeof(char*) * lines);
        int pkg_id_number = tokenize(pkg_ids, "\n", pkg_id_tokens, lines);
        int i = 0;
        for(; i<pkg_id_number; i++) {

            if(pkg_id_tokens[i][0] == '[') {
                pkg_id_tokens[i]++;
                char* end = strchr(pkg_id_tokens[i], ']');
                if(end != NULL) {
                    *end = '\0';
                    fprintf(AC_STDOUT, "%s%c", pkg_id_tokens[i], IFS);
                }
            }
        }

        free(pkg_id_tokens);
    }

    return -1;
}

static int parse_disconnect(int argc, char** argv) {
    if(argc == 0) {
        char* tmp = sdb_query("host:remote_emul");
        if(tmp) {
            char* tokens[MAX_TOKENS];
            int devices_number = tokenize(tmp, "\n", tokens, MAX_TOKENS);
            int i = 0;
            for(; i<devices_number; i++) {
                char* tab = strchr(tokens[i], '\t');
                if(tab != NULL) {
                    *tab = '\0';
                }
                rtrim(tokens[i]);
            }
            print_char_with_completion_flag(devices_number, tokens, argv);
        }

        return 0;
    }
    return -1;
}

static int parse_install(int argc, char** argv) {
    if(argc == 0) {
        print_local_dirlist_with_complete_flag(argc, argv);
    }
    return -1;
}

static int parse_push(int argc, char** argv) {

    if(argc == 0) {
        print_local_dirlist_with_complete_flag(argc, argv);
    }
    else if(argc == 1) {
        argc--;
        argv++;
        print_remote_dirlist_with_complete_flag(argc, argv);
    }
    return -1;
}

static int parse_pull(int argc, char** argv) {
    if(argc == 0) {
        print_remote_dirlist_with_complete_flag(argc, argv);
    }
    else if(argc == 1) {
        argc--;
        argv++;
        print_local_dirlist_with_complete_flag(argc, argv);
    }
    return -1;
}

static int parse_shell(int argc, char** argv) {
    //TODO
    return -1;
}

static int parse_cmd(int argc, char** argv) {
    if(argc == 0) {
        print_element_with_completion_flag(cmds_size, (AC_ELEMENT**)commands, argv);
        return -1;
    }

    int i = 0;
    for(; i<cmds_size; i++) {
        if(!strcmp(commands[i]->keyword, argv[0])) {
            commands[i]->func(--argc, ++argv);
            return 0;
        }
    }
    return -1;
}

static int parse_opt(int argc, char** argv) {
    if(argc == 0) {
        print_element_with_completion_flag(pre_options_size, (AC_ELEMENT**)pre_options, argv);
        return -1;
    }

    int i = 0;
    for(; i<pre_options_size; i++) {
        if(!strcmp(pre_options[i]->keyword, argv[0])) {
            pre_options[i]->func(--argc, ++argv);
            return 0;
        }
    }
    return -1;
}

static void print_element_with_completion_flag(int argc, AC_ELEMENT** argv, char** not_complete_char) {
    int i = 0;
    if(COMPLETE_FLAG) {
        for(; i<argc; i++) {
            fprintf(AC_STDOUT, "%s%c", argv[i]->keyword, IFS);
        }
    }
    else {
        int len = strnlen(not_complete_char[0], 255);
        for(; i<argc; i++) {
            if(!strncmp(not_complete_char[0], argv[i]->keyword, len)) {
                fprintf(AC_STDOUT, "%s%c", argv[i]->keyword, IFS);
            }
        }
    }
}

static void print_char_with_completion_flag(int argc, char** argv, char** not_complete_char) {
    int i = 0;
    if(COMPLETE_FLAG) {
        for(; i<argc; i++) {
            fprintf(AC_STDOUT, "%s%c", argv[i], IFS);
        }
    }
    else {
        int len = strnlen(not_complete_char[0], 255);
        for(; i<argc; i++) {
            if(!strncmp(not_complete_char[0], argv[i], len)) {
                fprintf(AC_STDOUT, "%s%c", argv[i], IFS);
            }
        }
    }
}

static void print_local_dirlist_with_complete_flag(int argc, char** argv) {
    if(COMPLETE_FLAG) {
        print_local_dirlist(NULL, argv);
    }
    else {
        char* src = strndup(argv[0], PATH_MAX);
        char* last = strlchr(src, '/');
        if(last != NULL) {
            *(++last) = '\0';
            print_local_dirlist(src, argv);
        }
        else {
            print_local_dirlist(NULL, argv);
        }
        free(src);
    }
}

static void print_local_dirlist(char* src_dir, char** not_complete_char) {
    DIR* d;

    int pwd_flag = 0;
    if(src_dir == NULL) {
        pwd_flag = 1;
        src_dir = strdup("./");
    }

    d = opendir(src_dir);
    if(d == 0) {
        goto finalize;
    }
    struct dirent* de;

    while((de = readdir(d))) {
        char* file_name = de->d_name;

        if(file_name[0] == '.') {
            if(file_name[1] == 0) {
                continue;
            }
            if((file_name[1] == '.') && (file_name[2] == 0)) {
                continue;
            }
        }

        char src_full_path[PATH_MAX];
        append_file(src_full_path, src_dir, file_name);

        char* src_ptr = src_full_path;
        if(pwd_flag) {
            src_ptr += 2;
        }

        if(COMPLETE_FLAG) {
            fprintf(AC_STDOUT, "%s%c", src_ptr, IFS);
        }
        else {
            int len = strnlen(not_complete_char[0], 255);
            if(!strncmp(not_complete_char[0], src_ptr, len)) {
                fprintf(AC_STDOUT, "%s%c", src_ptr, IFS);
            }
        }
    }

finalize:
    closedir(d);
    if(pwd_flag) {
        free(src_dir);
    }
}

static void print_remote_dirlist_with_complete_flag(int argc, char** argv) {
    if(COMPLETE_FLAG) {
        print_remote_dirlist("/", argv);
    }
    else {
        char* src = strndup(argv[0], PATH_MAX);
        char* last = strlchr(src, '/');
        if(last != NULL) {
            *(++last) = '\0';
            print_remote_dirlist(src, argv);
        }
        free(src);
    }
}

static void print_remote_dirlist(char* src_dir, char** not_complete_char) {

    int fd = sdb_connect("sync:");

    if(fd < 0) {
        goto finalize;
    }

    int len;
    len = strnlen(src_dir, SYNC_CHAR_MAX + 1);

    if(len > SYNC_CHAR_MAX) {
        goto finalize;
    }

    SYNC_MSG msg;
    msg.req.id = sync_list;
    msg.req.namelen = htoll(len);

    if(writex(fd, &msg.req, sizeof(msg.req)) ||
       writex(fd, src_dir, len)) {
        goto finalize;
    }

    while(1) {
        if(readx(fd, &msg.dent, sizeof(msg.dent)) ||
                msg.dent.id == sync_done ||
                msg.dent.id != sync_dent) {
            goto finalize;
        }
        len = ltohl(msg.dent.namelen);
        if(len > 256) {
            goto finalize;
        }

        char file_name[257];
        if(readx(fd, file_name, len)) {
            goto finalize;
        }
        file_name[len] = 0;

        if(file_name[0] == '.') {
            if(file_name[1] == 0) {
                continue;
            }
            if((file_name[1] == '.') && (file_name[2] == 0)) {
                continue;
            }
        }

        char src_full_path[PATH_MAX];
        append_file(src_full_path, src_dir, file_name);

        if(COMPLETE_FLAG) {
            fprintf(AC_STDOUT, "%s%c", src_full_path, IFS);
        }
        else {
            int len = strnlen(not_complete_char[0], 255);
            if(!strncmp(not_complete_char[0], src_full_path, len)) {
                fprintf(AC_STDOUT, "%s%c", src_full_path, IFS);
            }
        }

    }

finalize:
    msg.req.id = sync_quit;
    msg.req.namelen = 0;
    writex(fd, &msg.req, sizeof(msg.req));

    sdb_close(fd);
}
