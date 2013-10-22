/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <assert.h>

#define  TRACE_TAG  TRACE_SDB

#include "fdevent.h"
#include "strutils.h"

#ifdef HAVE_TERMIO_H
#include <termios.h>
#endif
#include "utils.h"
#include "sdb_client.h"
#include "file_sync_service.h"
#include "log.h"

#include "linkedlist.h"
#include "sdb_constants.h"
#include "sdb_model.h"
#include "commandline.h"
#include "command_function.h"

static void print_help(LIST_NODE* optlist, LIST_NODE* cmdlist);
static void create_opt_list(LIST_NODE** opt_list);
static void create_cmd_list(LIST_NODE** cmd_list);
static int do_cmd(transport_type ttype, char* serial, char *cmd, ...);

#ifdef HAVE_TERMIO_H

static __inline__ void stdin_raw_init(int fd, struct termios* tio_save);
static __inline__ void stdin_raw_restore(int fd, struct termios* tio_save);

static __inline__ void stdin_raw_init(int fd, struct termios* tio_save)
{
    struct termios tio;

    if(tcgetattr(fd, &tio)) return;
    memcpy(tio_save, &tio, sizeof(struct termios));

    tio.c_lflag = 0; /* disable CANON, ECHO*, etc */

        /* no timeout but request at least one character per read */
    tio.c_cc[VTIME] = 0;
    tio.c_cc[VMIN] = 1;

    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIFLUSH);
}

static __inline__ void stdin_raw_restore(int fd, struct termios* tio_save)
{
    tcsetattr(fd, TCSANOW, tio_save);
    tcflush(fd, TCIFLUSH);
}
#endif

void read_and_dump(int fd)
{
    char buf[PATH_MAX];
    int len;

    while(fd >= 0) {
        D("read_and_dump(): pre sdb_read(fd=%d)\n", fd);
        len = sdb_read(fd, buf, PATH_MAX);
        D("read_and_dump(): post sdb_read(fd=%d): len=%d\n", fd, len);
        if(len == 0) {
            break;
        }

        if(len < 0) {
            if(errno == EINTR) continue;
            break;
        }
        fwrite(buf, 1, len, stdout);
        fflush(stdout);
    }

}

static void *stdin_read_thread(void *x)
{
    unsigned char buf[1024];
    int r, n;

    void** args = (void**) x;
    int fd = *(int*)args[0];
#ifdef HAVE_TERMIO_H
    struct termios* tio_save = args[1];
#endif
    free(args[0]);
    free(args);
    for(;;) {
        /* fdi is really the client's stdin, so use read, not sdb_read here */
        D("stdin_read_thread(): pre unix_read(fdi=%d,...)\n", INPUT_FD);
        r = unix_read(INPUT_FD, buf, 1024);
        D("stdin_read_thread(): post unix_read(fdi=%d,...)\n", INPUT_FD);
        if(r == 0) break;
        if(r < 0) {
            if(errno == EINTR) continue;
            break;
        }
        for(n = 0; n < r; n++){
            if(buf[n] == '\n' || buf[n] == '\r') {
                n++;
                if(buf[n] == '~') {
                    n++;
                    if(buf[n] == '.') {
                        fprintf(stderr,"\n* disconnect *\n");
#ifdef HAVE_TERMIO_H
                    stdin_raw_restore(INPUT_FD, tio_save);
                    free(tio_save);
#endif
                    exit(0);
                    }
                }
            }
        }
        r = sdb_write(fd, buf, r);
        if(r <= 0) {
            break;
        }
    }
    return 0;
}

int interactive_shell(void** extargv)
{
    sdb_thread_t thr;

    int fd = sdb_connect("shell:", extargv);
    if(fd < 0) {
        return 1;
    }
    int* fd_p = malloc(sizeof(int));
    *fd_p = fd;

#ifdef HAVE_TERMIO_H
    void** args = (void**)malloc(sizeof(void*)*2);
    struct termios tio_save;
    stdin_raw_init(INPUT_FD, &tio_save);
    struct termios* tio_save_p = (struct termios*)malloc(sizeof(struct termios));
    memcpy(tio_save_p, &tio_save, sizeof(struct termios));
    args[1] = tio_save_p;
#else
    void** args = (void**)malloc(sizeof(void*));
#endif
    args[0] = fd_p;
    sdb_thread_create(&thr, stdin_read_thread, args);
    read_and_dump(fd);
#ifdef HAVE_TERMIO_H
    stdin_raw_restore(INPUT_FD, &tio_save);
#endif
    return 0;
}

int send_shellcommand(char* buf, void** extargv)
{
    int fd, ret;
    char* serial = (char *)extargv[0];
    transport_type ttype = *(transport_type*)extargv[1];

    for(;;) {
        fd = sdb_connect(buf, extargv);
        if(fd >= 0)
            break;
        fprintf(stderr,"- waiting for device -\n");
        sdb_sleep_ms(1000);
        do_cmd(ttype, serial, "wait-for-device", 0);
    }

    read_and_dump(fd);
    ret = sdb_close(fd);
    if (ret)
        perror("close");

    return ret;
}

static int do_cmd(transport_type ttype, char* serial, char *cmd, ...)
{
    char *argv[16];
    int argc;
    va_list ap;

    va_start(ap, cmd);
    argc = 0;

    if (serial) {
        argv[argc++] = "-s";
        argv[argc++] = serial;
    } else if (ttype == kTransportUsb) {
        argv[argc++] = "-d";
    } else if (ttype == kTransportLocal) {
        argv[argc++] = "-e";
    }

    argv[argc++] = cmd;
    while((argv[argc] = va_arg(ap, char*)) != 0) {
        argc++;
    }
    va_end(ap);

#if 0
    int n;
    fprintf(stderr,"argc = %d\n",argc);
    for(n = 0; n < argc; n++) {
        fprintf(stderr,"argv[%d] = \"%s\"\n", n, argv[n]);
    }
#endif

    return process_cmdline(argc, argv);
}

int __sdb_command(const char* cmd, void** extargv) {
    int result = sdb_connect(cmd, extargv);

    if(result < 0) {
        return result;
    }

    D("about to read_and_dump(fd=%d)\n", result);
    read_and_dump(result);
    D("read_and_dump() done.\n");
    sdb_close(result);

    return 0;
}

const char* get_basename(const char* filename)
{
    const char* basename = sdb_dirstop(filename);
    if (basename) {
        basename++;
        return basename;
    } else {
        return filename;
    }
}

int get_server_port() {
    return DEFAULT_SDB_PORT;
}

static void create_opt_list(LIST_NODE** opt_list) {

    OPTION* serial = NULL;
    create_option(&serial, COMMANDLINE_SERIAL_LONG_OPT, COMMANDLINE_SERIAL_SHORT_OPT, COMMANDLINE_SERIAL_DESC,
            COMMANDLINE_SERIAL_DESC_SIZE, COMMANDLINE_SERIAL_ARG_DESC, COMMANDLINE_SERIAL_HAS_ARG);
    prepend(opt_list, serial);

    OPTION* device = NULL;
    create_option(&device, COMMANDLINE_DEVICE_LONG_OPT, COMMANDLINE_DEVICE_SHORT_OPT, COMMANDLINE_DEVICE_DESC,
            COMMANDLINE_DEVICES_DESC_SIZE, EMPTY_STRING, COMMANDLINE_DEVICE_HAS_ARG);
    prepend(opt_list, device);

    OPTION* emulator = NULL;
    create_option(&emulator, COMMANDLINE_EMULATOR_LONG_OPT, COMMANDLINE_EMULATOR_SHORT_OPT, COMMANDLINE_EMULATOR_DESC,
            COMMANDLINE_EMULATOR_DESC_SIZE, EMPTY_STRING, COMMANDLINE_EMULATOR_HAS_ARG);
    prepend(opt_list, emulator);
}

static void create_cmd_list(LIST_NODE** cmd_list) {

    COMMAND* devices_cmd = NULL;
    create_command(&devices_cmd, COMMANDLINE_DEVICES_NAME, COMMANDLINE_DEVICES_DESC,
            COMMANDLINE_DEVICES_DESC_SIZE, EMPTY_STRING, devices, COMMANDLINE_DEVICES_MAX_ARG, COMMANDLINE_DEVICES_MIN_ARG);
    prepend(cmd_list, devices_cmd);

    COMMAND* connect_cmd = NULL;
    create_command(&connect_cmd, COMMANDLINE_CONNECT_NAME, COMMANDLINE_CONNECT_DESC,
            COMMANDLINE_CONNECT_DESC_SIZE, COMMANDLINE_CONNECT_ARG_DESC, __connect, COMMANDLINE_CONNECT_MAX_ARG, COMMANDLINE_CONNECT_MIN_ARG);
    prepend(cmd_list, connect_cmd);

    //TODO REMOTE_DEVICE_CONNECT security issue should be resolved first
#if 0
    COMMAND* device_con_cmd = NULL;
    create_command(&device_con_cmd, COMMANDLINE_DEVICE_CON_NAME, COMMANDLINE_DEVICE_CON_DESC,
            COMMANDLINE_DEVICE_CON_DESC_SIZE, COMMANDLINE_DEVICE_CON_ARG_DESC, device_con, COMMANDLINE_DEVICE_CON_MAX_ARG, COMMANDLINE_DEVICE_CON_MIN_ARG);
    prepend(cmd_list, device_con_cmd);
#endif
    COMMAND* disconnect_cmd = NULL;
    create_command(&disconnect_cmd, COMMANDLINE_DISCONNECT_NAME, COMMANDLINE_DISCONNECT_DESC,
            COMMANDLINE_DISCONNECT_DESC_SIZE, COMMANDLINE_DISCONNECT_ARG_DESC, __disconnect, COMMANDLINE_DISCONNECT_MAX_ARG, COMMANDLINE_DISCONNECT_MIN_ARG);
    prepend(cmd_list, disconnect_cmd);

    COMMAND* push_cmd = NULL;
    create_command(&push_cmd, COMMANDLINE_PUSH_NAME, COMMANDLINE_PUSH_DESC,
            COMMANDLINE_PUSH_DESC_SIZE, COMMANDLINE_PUSH_ARG_DESC, push, COMMANDLINE_PUSH_MAX_ARG, COMMANDLINE_PUSH_MIN_ARG);
    prepend(cmd_list, push_cmd);

    COMMAND* pull_cmd = NULL;
    create_command(&pull_cmd, COMMANDLINE_PULL_NAME, COMMANDLINE_PULL_DESC,
            COMMANDLINE_PULL_DESC_SIZE, COMMANDLINE_PULL_ARG_DESC, pull, COMMANDLINE_PULL_MAX_ARG, COMMANDLINE_PULL_MIN_ARG);
    prepend(cmd_list, pull_cmd);

    COMMAND* shell_cmd = NULL;
    create_command(&shell_cmd, COMMANDLINE_SHELL_NAME, COMMANDLINE_SHELL_DESC,
            COMMANDLINE_SHELL_DESC_SIZE, COMMANDLINE_SHELL_ARG_DESC, shell, COMMANDLINE_SHELL_MAX_ARG, COMMANDLINE_SHELL_MIN_ARG);
    prepend(cmd_list, shell_cmd);

    COMMAND* dlog_cmd = NULL;
    create_command(&dlog_cmd, COMMANDLINE_DLOG_NAME, COMMANDLINE_DLOG_DESC,
            COMMANDLINE_DLOG_DESC_SIZE, COMMANDLINE_DLOG_ARG_DESC, dlog, COMMANDLINE_DLOG_MAX_ARG, COMMANDLINE_DLOG_MIN_ARG);
    prepend(cmd_list, dlog_cmd);

    COMMAND* install_cmd = NULL;
    create_command(&install_cmd, COMMANDLINE_INSTALL_NAME, COMMANDLINE_INSTALL_DESC,
            COMMANDLINE_INSTALL_DESC_SIZE, COMMANDLINE_INSTALL_ARG_DESC, install, COMMANDLINE_INSTALL_MAX_ARG, COMMANDLINE_INSTALL_MIN_ARG);
    prepend(cmd_list, install_cmd);

    COMMAND* uninstall_cmd = NULL;
    create_command(&uninstall_cmd, COMMANDLINE_UNINSTALL_NAME, COMMANDLINE_UNINSTALL_DESC,
            COMMANDLINE_UNINSTALL_DESC_SIZE, COMMANDLINE_UNINSTALL_ARG_DESC, uninstall, COMMANDLINE_UNINSTALL_MAX_ARG, COMMANDLINE_UNINSTALL_MIN_ARG);
    prepend(cmd_list, uninstall_cmd);

    COMMAND* forward_cmd = NULL;
    create_command(&forward_cmd, COMMANDLINE_FORWARD_NAME, COMMANDLINE_FORWARD_DESC,
            COMMANDLINE_FORWARD_DESC_SIZE, COMMANDLINE_FORWARD_ARG_DESC, forward, COMMANDLINE_FORWARD_MAX_ARG, COMMANDLINE_FORWARD_MIN_ARG);
    prepend(cmd_list, forward_cmd);

    COMMAND* help_cmd = NULL;
    create_command(&help_cmd, COMMANDLINE_HELP_NAME, COMMANDLINE_HELP_DESC,
            COMMANDLINE_HELP_DESC_SIZE, EMPTY_STRING, NULL, 0, 0);
    prepend(cmd_list, help_cmd);

    COMMAND* version_cmd = NULL;
    create_command(&version_cmd, COMMANDLINE_VERSION_NAME, COMMANDLINE_VERSION_DESC,
            COMMANDLINE_VERSION_DESC_SIZE, EMPTY_STRING, version, COMMANDLINE_VERSION_MAX_ARG, COMMANDLINE_VERSION_MIN_ARG);
    prepend(cmd_list, version_cmd);

    COMMAND* sserver_cmd = NULL;
    create_command(&sserver_cmd, COMMANDLINE_SSERVER_NAME, COMMANDLINE_SSERVER_DESC,
            COMMANDLINE_SSERVER_DESC_SIZE, EMPTY_STRING, start_server, COMMANDLINE_SSERVER_MAX_ARG, COMMANDLINE_SSERVER_MIN_ARG);
    prepend(cmd_list, sserver_cmd);

    COMMAND* kserver_cmd = NULL;
    create_command(&kserver_cmd, COMMANDLINE_KSERVER_NAME, COMMANDLINE_KSERVER_DESC,
            COMMANDLINE_KSERVER_DESC_SIZE, EMPTY_STRING, kill_server, COMMANDLINE_KSERVER_MAX_ARG, COMMANDLINE_KSERVER_MIN_ARG);
    prepend(cmd_list, kserver_cmd);

    COMMAND* gstate_cmd = NULL;
    create_command(&gstate_cmd, COMMANDLINE_GSTATE_NAME, COMMANDLINE_GSTATE_DESC,
            COMMANDLINE_GSTATE_DESC_SIZE, EMPTY_STRING, get_state_serialno, COMMANDLINE_GSTATE_MAX_ARG, COMMANDLINE_GSTATE_MIN_ARG);
    prepend(cmd_list, gstate_cmd);

    COMMAND* gserial_cmd = NULL;
    create_command(&gserial_cmd, COMMANDLINE_GSERIAL_NAME, COMMANDLINE_GSERIAL_DESC,
            COMMANDLINE_GSERIAL_DESC_SIZE, EMPTY_STRING, get_state_serialno, COMMANDLINE_GSERIAL_MAX_ARG, COMMANDLINE_GSERIAL_MIN_ARG);
    prepend(cmd_list, gserial_cmd);

    COMMAND* swindow_cmd = NULL;
    create_command(&swindow_cmd, COMMANDLINE_SWINDOW_NAME, COMMANDLINE_SWINDOW_DESC,
            COMMANDLINE_SWINDOW_DESC_SIZE, EMPTY_STRING, status_window, COMMANDLINE_SWINDOW_MAX_ARG, COMMANDLINE_SWINDOW_MIN_ARG);
    prepend(cmd_list, swindow_cmd);

    COMMAND* root_cmd = NULL;
    create_command(&root_cmd, COMMANDLINE_ROOT_NAME, COMMANDLINE_ROOT_DESC,
            COMMANDLINE_ROOT_DESC_SIZE, COMMANDLINE_ROOT_ARG_DESC, root, COMMANDLINE_ROOT_MAX_ARG, COMMANDLINE_ROOT_MIN_ARG);
    prepend(cmd_list, root_cmd);

    COMMAND* launch_cmd = NULL;
    create_command(&launch_cmd, COMMANDLINE_LAUNCH_NAME, NULL,
            0, EMPTY_STRING, launch, COMMANDLINE_LAUNCH_MAX_ARG, COMMANDLINE_LAUNCH_MIN_ARG);
    prepend(cmd_list, launch_cmd);

    COMMAND* forkserver_cmd = NULL;
    create_command(&forkserver_cmd, COMMANDLINE_FORKSERVER_NAME, NULL,
            0, EMPTY_STRING, forkserver, COMMANDLINE_FORKSERVER_MAX_ARG, COMMANDLINE_FORKSERVER_MIN_ARG);
    prepend(cmd_list, forkserver_cmd);

    COMMAND* oprofile_cmd = NULL;
    create_command(&oprofile_cmd, COMMANDLINE_OPROFILE_NAME, NULL,
            0, EMPTY_STRING, oprofile, COMMANDLINE_OPROFILE_MAX_ARG, COMMANDLINE_OPROFILE_MIN_ARG);
    prepend(cmd_list, oprofile_cmd);

    COMMAND* da_cmd = NULL;
    create_command(&da_cmd , COMMANDLINE_DA_NAME, NULL,
            0, EMPTY_STRING, da, COMMANDLINE_DA_MAX_ARG, COMMANDLINE_DA_MIN_ARG);
    prepend(cmd_list, da_cmd );
}

int process_cmdline(int argc, char** argv) {

    transport_type ttype = kTransportAny;
    char* serial = NULL;

    // TODO: also try TARGET_PRODUCT/TARGET_DEVICE as a hint

    LIST_NODE* cmd_list = NULL;
    LIST_NODE* opt_list = NULL;
    LIST_NODE* input_opt_list = NULL;

    create_cmd_list(&cmd_list);
    create_opt_list(&opt_list);
    int parsed_argc = parse_opt(argc, argv, opt_list, &input_opt_list);

    if(parsed_argc < 0) {
        return -1;
    }

    D("Parsed %d arguments\n", parsed_argc);

    argc = argc - parsed_argc;
    argv = argv + parsed_argc;

    int server_port = get_server_port();
    void* extraarg[3];
    extraarg[2] = &server_port;

    INPUT_OPTION* opt_s = get_inputopt(input_opt_list, (char*)COMMANDLINE_SERIAL_SHORT_OPT);
    if(opt_s != NULL) {
        serial = opt_s->value;

        char buf[PATH_MAX];
        char *tmp;
        snprintf(buf, sizeof(buf), "host:serial-match:%s", serial);


        tmp = sdb_query(buf, extraarg);
        if (tmp) {
            serial = strdup(tmp);
        } else {
            fprintf(stderr, "wrong serial number '%s'\n", serial);
            return 1;
        }
    }
    else {
        INPUT_OPTION* opt_d = get_inputopt(input_opt_list, (char*)COMMANDLINE_DEVICE_SHORT_OPT);
        if(opt_d != NULL) {
            ttype = kTransportUsb;
        }
        else {
            INPUT_OPTION* opt_e = get_inputopt(input_opt_list, (char*)COMMANDLINE_EMULATOR_SHORT_OPT);
            if(opt_e != NULL) {
                ttype = kTransportLocal;
            }
        }
    }

    if(argc > 0) {
        if(!strcmp(argv[0], COMMANDLINE_HELP_NAME)) {
            print_help(opt_list, cmd_list);
            return 1;
        }
        COMMAND* command = get_command(cmd_list, argv[0]);

        D("process command: %s\n", command->name);
        int minargs = command->minargs;
        int maxargs = command->maxargs;

        if(argc < minargs + 1) {
            fprintf(stderr, "%s command has following args: %s, and it requires at least %d arguments\n", argv[0],  command->argdesc, minargs);
            if (serial != NULL) {
                free(serial);
            }
            return 1;
        }
        if(argc > maxargs + 1 && maxargs > -1) {
            fprintf(stderr, "command %s require at most %d arguments\n", argv[0], maxargs);
            if (serial != NULL) {
                free(serial);
            }
            return 1;
        }
        extraarg[0] = serial;
        extraarg[1] = &ttype;
        int (*Func)(int, char**, void**) = command->Func;
        free_list(cmd_list, NULL);
        free_list(input_opt_list, NULL);
        free_list(opt_list, NULL);
        return Func(argc, argv, extraarg);
    }

    print_help(opt_list, cmd_list);
    if (serial != NULL) {
        free(serial);
    }
    return 1;
}

static void print_help(LIST_NODE* optlist, LIST_NODE* cmdlist) {

    fprintf(stderr, "Smart Development Bridge version %d.%d.%d\n",
         SDB_VERSION_MAJOR, SDB_VERSION_MINOR, SDB_SERVER_VERSION);
    fprintf(stderr, "\n Usage : sdb [option] <command> [parameters]\n\n");
    fprintf(stderr, " options:\n");

    LIST_NODE* curptr = optlist;
    int append_len = strlen(HELP_APPEND_STR);
    char* append_str = (char*)malloc(sizeof(char)*append_len);
    char* help_str = (char*)malloc(sizeof(char)*append_len*3);
    while(curptr != NULL) {
        OPTION* opt = (OPTION*)curptr->data;
        curptr = curptr->next_ptr;
        const char** des = opt->desc;
        if(des != NULL) {
            snprintf(help_str, append_len*3, "  -%s, --%s %s", opt->shortopt, opt->longopt, opt->argdesc);
            int opt_len = strlen(help_str);
            if(opt_len >= append_len) {
                fprintf(stderr, "%s\n%s", help_str, HELP_APPEND_STR);
            }
            else {
                snprintf(append_str, append_len - opt_len + 1, "%s", HELP_APPEND_STR);
                fprintf(stderr, "%s%s", help_str, append_str);
            }
            int array_len = opt->desc_size;
            fprintf(stderr, "- %s\n", des[0]);
            int i = 1;
            for(; i< array_len; i++) {
                fprintf(stderr, "%s  %s\n", HELP_APPEND_STR, des[i]);
            }
        }
    }
    fprintf(stderr, "\n commands:\n");

    curptr = cmdlist;
    while(curptr != NULL) {
        COMMAND* cmd = (COMMAND*)curptr ->data;
        curptr = curptr->next_ptr;
        const char** des = cmd->desc;
        if(des != NULL) {
            snprintf(help_str, append_len*3, "  sdb %s %s", cmd->name, cmd->argdesc);
            int cmd_len = strlen(help_str);
            if(cmd_len >= append_len) {
                fprintf(stderr, "%s\n%s", help_str, HELP_APPEND_STR);
            }
            else {
                snprintf(append_str, append_len - cmd_len + 1, "%s", HELP_APPEND_STR);
                fprintf(stderr, "%s%s", help_str, append_str);
            }
            int array_len = cmd->desc_size;
            fprintf(stderr, "- %s\n", des[0]);
            int i = 1;
            for(; i< array_len; i++) {
                fprintf(stderr, "%s  %s\n", HELP_APPEND_STR, des[i]);
            }
        }
    }

    free(append_str);
    free(help_str);
}


