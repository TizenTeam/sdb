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
#include <limits.h>
#include "utils.h"
#include "fdevent.h"

#include "commandline.h"
#include "command_function.h"
#include "sdb_client.h"
#include "sdb_constants.h"
#include "file_sync_service.h"

#include "strutils.h"
#include "file_sync_client.h"
#include "file_sync_functions.h"
#include "common_modules.h"

#include "log.h"
#include "sdb.h"

static const char *SDK_TOOL_PATH="/home/developer/sdk_tools";
static const char *APP_PATH_PREFIX="/opt/apps";

static void __inline__ format_host_command(char* buffer, size_t  buflen, const char* command, transport_type ttype, const char* serial);
static int get_pkgtype_file_name(const char* file_name);
static int get_pkgtype_from_app_id(const char* app_id, void** extargv);
static int kill_gdbserver_if_running(const char* process_cmd, void** extargv);
static int verify_gdbserver_exist(void** extargv);

int da(int argc, char ** argv, void** extargv) {
    char full_cmd[PATH_MAX] = "shell:/usr/bin/da_command";

    append_args(full_cmd, --argc, (const char**)++argv, PATH_MAX-1);
    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    int result = __sdb_command(full_cmd, extargv);

    if(result < 0) {
        return 1;
    }
    return 0;
}

int oprofile(int argc, char ** argv, void** extargv) {
    char full_cmd[PATH_MAX] = "shell:/usr/bin/oprofile_command";

    append_args(full_cmd, --argc, (const char**)++argv, PATH_MAX- 1);
    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    int result = __sdb_command(full_cmd, extargv);

    if(result < 0) {
        return 1;
    }
    return 0;
}

int launch(int argc, char ** argv, void** extargv) {
    int i;
    int result = 0;
    char pkgid[11] = {0,};
    char exe[512] = {0,};
    char args[512] = {0,};
    int mode = 0;
    int port = 0;
    int pid = 0;
    int type = 0;
    char fullcommand[PATH_MAX] = {'s','h','e','l','l',':',};
    char buf[128] = {0,};
    char flag = 0;

    if (argc < 7 || argc > 15 ) {
        fprintf(stderr,"usage: sdb launch -p <pkgid> -e <executable> -m <run|debug|da|oprofile> [-P <port>] [-attach <pid>] [-t <gtest,gcov>]  [<args...>]\n");
        return -1;
    }

    if(SDB_HIGHER_THAN_2_2_3(extargv)) {
        int full_len = PATH_MAX - 1;
        strncat(fullcommand, WHITE_SPACE, full_len);
        strncat(fullcommand, SDB_LAUNCH_SCRIPT, full_len);
        for(i = 1; i < argc; i ++) {
            strncat(fullcommand, WHITE_SPACE, full_len);
            strncat(fullcommand, argv[i], full_len);
        }

        return __sdb_command(fullcommand, extargv);
    }
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) {
            flag = 'p';
            continue;
        }
        if (!strcmp(argv[i], "-e")) {
            flag = 'e';
            continue;
        }
        if (!strcmp(argv[i], "-m")) {
            flag = 'm';
            continue;
        }
        if (!strcmp(argv[i], "-P")) {
            flag = 'P';
            continue;
        }
        if (!strcmp(argv[i], "-t")) {
            flag = 't';
            continue;
        }
        if (!strcmp(argv[i], "-attach")) {
            flag = 'a';
            continue;
        }
        D("launch cmd args: %c : %s\n", flag, argv[i]);

        switch (flag) {
        case 'p' :
            s_strncpy(pkgid, argv[i], sizeof(pkgid));
            flag = 0;
            break;
        case 'e':
            s_strncpy(exe, argv[i], sizeof(exe));
            flag = 0;
            break;
        case 'm': {
            if (!strcmp(argv[i], "run")) {
                mode = 0;
            } else if (!strcmp(argv[i], "debug")) {
                mode = 1;
            } else if (!strcmp(argv[i], "da") || !strcmp(argv[i], "oprofile")) {
                fprintf(stderr,"The -m option for da and oprofile is supported in sdbd higher than 2.2.0\n");
                return -1;
            }
            else {
                fprintf(stderr,"The -m option accepts arguments only run or debug options\n");
                return -1;
            }
            flag = 0;
            break;
        }
        case 'P': {
            if (mode != 1) {
                fprintf(stderr,"The -P option should be used in debug mode\n");
                return -1;
            }
            port = atoi(argv[i]);
            flag = 0;
            break;
        }
        case 'a': {
            if (mode != 1) {
                fprintf(stderr, "The -attach option should be used in debug mode\n");
                return -1;
            }
            pid = atoi(argv[i]);
            flag = 0;
            break;
        }
        case 't': {
            char *str = argv[i];
            for (; *str; str++) {
                if (!memcmp(str, "gtest", 5)) {
                    snprintf(buf, sizeof(buf), "export LD_LIBRARY_PATH=%s/gtest/usr/lib && ", SDK_TOOL_PATH);
                    strncat(fullcommand, buf, sizeof(fullcommand) - 1);
                    type = 1;
                }
                if (!memcmp(str, "gcov", 4)) {
                    snprintf(buf, sizeof(buf), "export GCOV_PREFIX=/tmp/%s/data && export GCOV_PREFIX_STRIP=0 && ", pkgid);
                    strncat(fullcommand, buf, sizeof(fullcommand) - 1);
                    type = 2;
                }
                char *ptr = strstr(str, ",");
                if (ptr) {
                    str = ptr;
                }
            }
            flag = 0;
        }
            break;
        default : {
            while (i < argc) {
                strncat(args, " ", sizeof(args)-1);
                strncat(args, argv[i], sizeof(args)-1);
                i++;
            }
            break;
        }
        }
    }

    if (mode == 0) {
        if (type == 0) {
            snprintf(buf, sizeof(buf), "/usr/bin/launch_app %s.%s", pkgid, exe);
            strncat(fullcommand, buf, sizeof(fullcommand)-1);
        } else {
            snprintf(buf, sizeof(buf), "%s/%s/bin/%s", APP_PATH_PREFIX, pkgid, exe);
            strncat(fullcommand, buf, sizeof(fullcommand)-1);
        }
    } else if (mode == 1) {
        if (verify_gdbserver_exist(extargv) < 0) {
            return -1;
        }
        if (!port) {
            fprintf(stderr,"The port number is not valid\n");
            return -1;
        }
        if (pid) {
            snprintf(buf, sizeof(buf), "%s/gdbserver/gdbserver :%d --attach %d", SDK_TOOL_PATH, port, pid);
        } else {
            snprintf(buf, sizeof(buf), "%s/gdbserver/gdbserver :%d %s/%s/bin/%s", SDK_TOOL_PATH, port, APP_PATH_PREFIX, pkgid, exe);
        }
        if (kill_gdbserver_if_running(buf, extargv) < 0) {
            fprintf(stderr, "Gdbserver is already running on your target.\nAn gdb is going to connect the previous gdbserver process.\n");
            return -1;
        }
        strncat(fullcommand, buf, sizeof(fullcommand)-1);
    }
    if (strlen(args) > 1) {
        strncat(fullcommand, " ", sizeof(fullcommand)-1);
        strncat(fullcommand, args, sizeof(fullcommand)-1);
    }

    D("launch command: [%s]\n", fullcommand);
    result = __sdb_command(fullcommand, extargv);
    sdb_close(result);

    return result;
}

int devices(int argc, char ** argv, void** extargv) {
    char *tmp;
    char full_cmd[PATH_MAX];

    snprintf(full_cmd, sizeof full_cmd, "host:%s", argv[0]);
    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    tmp = sdb_query(full_cmd, extargv);
    if(tmp) {
        printf("List of devices attached \n");
        printf("%s", tmp);
        return 0;
    } else {
        return 1;
    }
}

int __disconnect(int argc, char ** argv, void** extargv) {
    char full_cmd[PATH_MAX];
    char* tmp;

    if (argc == 2) {
        snprintf(full_cmd, sizeof full_cmd, "host:disconnect:%s", argv[1]);
    } else {
        snprintf(full_cmd, sizeof full_cmd, "host:disconnect:");
    }
    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    tmp = sdb_query(full_cmd, extargv);
    if(tmp) {
        printf("%s\n", tmp);
        return 0;
    } else {
        return 1;
    }
}

int __connect(int argc, char ** argv, void** extargv) {
    char full_cmd[PATH_MAX];
    char * tmp;
    snprintf(full_cmd, sizeof full_cmd, "host:connect:%s", argv[1]);
    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    tmp = sdb_query(full_cmd, extargv);
    if(tmp) {
        printf("%s\n", tmp);
        return 0;
    } else {
        return 1;
    }
}

int device_con(int argc, char ** argv, void** extargv) {

    char *tmp;
    char full_cmd[PATH_MAX];

    snprintf(full_cmd, sizeof full_cmd, "host:device_con:%s:%s", argv[1], argv[2]);
    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    tmp = sdb_query(full_cmd, extargv);

    if(tmp != NULL) {
        printf("%s", tmp);
        return 0;
    }

    return 1;
}

int get_state_serialno(int argc, char ** argv, void** extargv) {
    char* serial = (char *)extargv[0];
    transport_type* ttype = (transport_type*)extargv[1];
    char *tmp;
    char full_cmd[PATH_MAX];

    format_host_command(full_cmd, sizeof full_cmd, argv[0], *ttype, serial);
    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    tmp = sdb_query(full_cmd, extargv);
    if(tmp) {
        printf("%s\n", tmp);
        return 0;
    } else {
        return 1;
    }
}

int root(int argc, char ** argv, void** extargv) {
    char full_cmd[20];
    snprintf(full_cmd, sizeof(full_cmd), "root:%s", argv[1]);
    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    int fd = sdb_connect(full_cmd, extargv);

    if(fd >= 0) {
        read_and_dump(fd);
        sdb_close(fd);
        return 0;
    }
    return 1;
}

int status_window(int argc, char ** argv, void** extargv) {
    char* serial = (char *)extargv[0];
    transport_type* ttype = (transport_type*)extargv[1];

    char full_cmd[PATH_MAX];
    char *state = 0;
    char *laststate = 0;

        /* silence stderr (It means 2>/dev/null) */
#ifdef _WIN32
    /* XXX: TODO */
#else
    int  fd;
    fd = unix_open("/dev/null", O_WRONLY);

    if(fd >= 0) {
        dup2(fd, 2);
        sdb_close(fd);
    }
#endif

    format_host_command(full_cmd, sizeof full_cmd, "get-state", *ttype, serial);

    for(;;) {
        sdb_sleep_ms(250);

        if(state) {
            free(state);
            state = 0;
        }

        D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
        state = sdb_query(full_cmd, extargv);

        if(state) {
            if(laststate && !strcmp(state,laststate)){
                continue;
            } else {
                if(laststate) free(laststate);
                laststate = strdup(state);
            }
        }

        printf("%c[2J%c[2H", 27, 27);
        printf("Samsung Development Bridge\n");
        printf("State: %s\n", state ? state : "offline");
        fflush(stdout);
    }

    return 0;
}

int kill_server(int argc, char ** argv, void** extargv) {
    int fd;
    fd = _sdb_connect("host:kill", extargv);
    if(fd == -1) {
        fprintf(stderr,"* server not running *\n");
        return 1;
    }
    return 0;
}

int start_server(int argc, char ** argv, void** extargv) {
    return sdb_connect("host:start-server", extargv);
}

int version(int argc, char ** argv, void** extargv) {
    transport_type ttype = *(transport_type*)extargv[1];

    if (ttype == kTransportUsb || ttype == kTransportLocal) {
        char* VERSION_QUERY ="shell:rpm -qa | grep sdbd";
        send_shellcommand(VERSION_QUERY, extargv);
    } else {
        fprintf(stdout, "Smart Development Bridge version %d.%d.%d\n",
             SDB_VERSION_MAJOR, SDB_VERSION_MINOR, SDB_VERSION_PATCH);
    }
    return 0;
}

int forward(int argc, char ** argv, void** extargv) {
    char* serial = (char *)extargv[0];
    transport_type ttype = *(transport_type*)extargv[1];

    char full_cmd[PATH_MAX];
    char prefix[NAME_MAX];

    get_host_prefix(prefix, NAME_MAX, ttype, serial, host);
    snprintf(full_cmd, sizeof full_cmd, "%sforward:%s;%s",prefix, argv[1], argv[2]);

    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    if(sdb_command(full_cmd, extargv) < 0) {
        return 1;
    }
    return 0;
}

int dlog(int argc, char ** argv, void** extargv) {
    D("dlog with serial: %s\n", (char*)extargv[0]);

    char full_cmd[PATH_MAX] = "shell:/usr/bin/dlogutil";

    int i;
    for(i = 1; i<argc; i++) {
        char quoted_string[MAX_INPUT];
        dup_quote(quoted_string, argv[i], MAX_INPUT);

        strncat(full_cmd, " ", sizeof(full_cmd)-1);
        strncat(full_cmd, quoted_string, sizeof(full_cmd)-1);
    }

    send_shellcommand(full_cmd, extargv);
    return 0;
}

int push(int argc, char ** argv, void** extargv) {
    int i=0;
    int utf8 = 0;

    if(argc > 3 && !strcmp(argv[argc-1], "--with-utf8")) {
        D("push with utf8");
        utf8 = 1;
        --argc;
    }

    for (i=1; i<argc-1; i++) {
        do_sync_copy(argv[i], argv[argc-1], (FILE_FUNC*)&LOCAL_FILE_FUNC, (FILE_FUNC*)&REMOTE_FILE_FUNC, utf8, extargv);
    }
    return 0;
}

int pull(int argc, char ** argv, void** extargv) {
    if (argc == 2) {
        return do_sync_copy(argv[1], ".", (FILE_FUNC*)&REMOTE_FILE_FUNC, (FILE_FUNC*)&LOCAL_FILE_FUNC, 0, extargv);
    }
    return do_sync_copy(argv[1], argv[2], (FILE_FUNC*)&REMOTE_FILE_FUNC, (FILE_FUNC*)&LOCAL_FILE_FUNC, 0, extargv);
}

int shell(int argc, char ** argv, void** extargv) {
    char buf[4096];

        int r;
        int fd;

        if(argc < 2) {
            D("starting interactive shell\n");
            r = interactive_shell(extargv);
            return r;
        }

        snprintf(buf, sizeof buf, "shell:%s", argv[1]);
        argc -= 2;
        argv += 2;
        while(argc-- > 0) {
            strcat(buf, " ");

            /* quote empty strings and strings with spaces */
            int quote = (**argv == 0 || strchr(*argv, ' '));
            if (quote)
                strcat(buf, "\"");
            strcat(buf, *argv++);
            if (quote)
                strcat(buf, "\"");
        }

        D("interactive shell loop. buff=%s\n", buf);
        fd = sdb_connect(buf, extargv);
        if(fd >= 0) {
            D("about to read_and_dump(fd=%d)\n", fd);
            read_and_dump(fd);
            D("read_and_dump() done.\n");
            sdb_close(fd);
            r = 0;
        } else {
            r = 1;
        }

        D("interactive shell loop. return r=%d\n", r);
        return r;
}

int forkserver(int argc, char** argv, void** extargv) {
    if(!strcmp(argv[1], "server")) {
        int r = sdb_main(1, get_server_port());
        return r;
    }
    else {
        fprintf(stderr,COMMANDLINE_ERROR_ARG_MISSING, "server", "forkserver");
        return 1;
    }
}

int install(int argc, char **argv, void** extargv) {

    char* srcpath = argv[1];
    const char* filename = sdb_dirstop(srcpath);

    char destination[PATH_MAX];
    snprintf(destination, PATH_MAX, "%s", DIR_APP_TMP );

    if (filename) {
        filename++;
        strncat(destination, filename, PATH_MAX - 1 );
    } else {
        strncat(destination, srcpath, PATH_MAX - 1 );
    }

    D("Install path%s\n", destination);
    int tpk = get_pkgtype_file_name(srcpath);
    if (tpk == -1) {
        fprintf(stderr, "error: unknown package type\n");
        return 1;
    }

    D("Push file: %s to %s\n", srcpath, destination);
    int result = do_sync_copy(srcpath, destination, (FILE_FUNC*)&LOCAL_FILE_FUNC, (FILE_FUNC*)&REMOTE_FILE_FUNC, 0, extargv);

    if(result < 0) {
        return 1;
    }

    const char* SHELL_INSTALL_CMD ="shell:/usr/bin/pkgcmd -i -t %s -p %s -q";
    char full_cmd[PATH_MAX];

    if(tpk == 1) {
        snprintf(full_cmd, sizeof full_cmd, SHELL_INSTALL_CMD, "tpk", destination);
    }
    else if(tpk == 0){
        snprintf(full_cmd, sizeof full_cmd, SHELL_INSTALL_CMD, "wgt", destination);
    }

    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    result = __sdb_command(full_cmd, extargv);

    if(result < 0) {
        return 1;
    }

    const char* SHELL_REMOVE_CMD = "shell:rm %s";
    snprintf(full_cmd, sizeof full_cmd, SHELL_REMOVE_CMD, destination);
    D(COMMANDLINE_MSG_FULL_CMD, "remove", full_cmd);
    result = __sdb_command(full_cmd, extargv);

    if(result < 0) {
        return 1;
    }

    return 0;
}

int uninstall(int argc, char **argv, void** extargv) {
    char* appid = argv[1];
    const char* SHELL_UNINSTALL_CMD ="shell:/usr/bin/pkgcmd -u -t %s -n %s -q";
    char full_cmd[PATH_MAX];
    int result = 0;
    int tpk = get_pkgtype_from_app_id(appid, extargv);
    if(tpk == 1) {
        snprintf(full_cmd, sizeof full_cmd, SHELL_UNINSTALL_CMD, "tpk", appid);
    }
    else if(tpk == 0){
        snprintf(full_cmd, sizeof full_cmd, SHELL_UNINSTALL_CMD, "wgt", appid);
    }
    else {
        return 1;
    }
    D(COMMANDLINE_MSG_FULL_CMD, argv[0], full_cmd);
    result = __sdb_command(full_cmd, extargv);

    if(result < 0) {
        return 1;
    }

    return 0;
}

// Returns 0 if pkg type is wgt. Returns 1 if pkg type is tpk. Returns minus if exception happens.
static int get_pkgtype_file_name(const char* file_name) {

    char* pkg_type;

    int result = -1;

    pkg_type = strrchr(file_name, '.')+1;
    if (pkg_type != NULL) {
        if(!strcmp(pkg_type, "wgt")) {
            result = 0;
        }
        else if(!strcmp(pkg_type, "tpk")) {
            result = 1;
        }
    }

    return result;
}

// Returns 0 if pkg type is wgt. Returns 1 if pkg type is tpk. Returns minus if exception happens.
static int get_pkgtype_from_app_id(const char* app_id, void** extargv) {

    char* GET_PKG_TYPE_CMD = "shell:/usr/bin/pkgcmd -l | grep %s | awk '{print $2}'";
    char full_cmd[PATH_MAX];
    snprintf(full_cmd, sizeof full_cmd, GET_PKG_TYPE_CMD, app_id);

    int result = sdb_connect(full_cmd, extargv);
    if(result < 0) {
        return result;
    }
    char buf[100] = "";

    int rl_result = read_line(result, buf, 100);
    if(rl_result < 0) {
        D("Error to read buffer (fd=%d)\n", rl_result);
        return rl_result;
    }

    sdb_close(result);
    result = -1;

    if(strstr(buf, "[tpk]") != NULL) {
        result = 1;
    } else if(strstr(buf, "[wgt]") != NULL) {
        result = 0;
    }
    return result;
}


/*
 * kill gdbserver if running
 */

static int kill_gdbserver_if_running(const char* process_cmd, void** extargv) {
    char cmd[512] = {};
    char buf[512] = {};

    // hopefully, it is not going to happen, but check executable gdbserver is existed
    snprintf(cmd, sizeof(cmd), "shell:/usr/bin/da_command process | grep '%s' | grep -v grep | wc -l", process_cmd);
    int result = sdb_connect(cmd, extargv);

    if(result < 0) {
        return -1;
    }
    if (read_line(result, buf, sizeof(buf)) < 0) {
        sdb_close(result);
        return -1;
    }
    if(memcmp(buf, "0", 1)) {
/*
        // TODO: check cmd return code
        snprintf(cmd, sizeof(cmd), "shell:/usr/bin/da_command killapp '%s'", process_cmd);
        result = sdb_connect(cmd);
        if (read_line(result, buf, sizeof(buf)) < 0) {
            sdb_close(result);
            return -1;
        }
*/
    }
    sdb_close(result);
    return 1;
}

static void __inline__ format_host_command(char* buffer, size_t  buflen, const char* command, transport_type ttype, const char* serial)
{
    char prefix[NAME_MAX];
    get_host_prefix(prefix, NAME_MAX, ttype, serial, host);
    snprintf(buffer, buflen, "%s%s", prefix, command);
}


/*
 * returns -1 if gdbserver exists
 */
static int verify_gdbserver_exist(void** extargv) {
    char cmd[512] = {};
    char buf[512] = {};

    snprintf(cmd, sizeof(cmd), "shell:%s/gdbserver/gdbserver --version 1>/dev/null", SDK_TOOL_PATH);
    int result = sdb_connect(cmd, extargv);

    if(result < 0) {
        sdb_close(result);
        return -1;
    }
    if (read_line(result, buf, sizeof(buf)) > 0) {
        fprintf(stderr, "error: %s\n", buf);
        sdb_close(result);
        return -1;
    }
    sdb_close(result);
    return result;
}
