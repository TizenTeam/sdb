/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
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

#include "sysdeps.h"

#ifdef HAVE_TERMIO_H
#include <termios.h>
#endif

#define  TRACE_TAG  TRACE_SDB
#include "sdb.h"
#include "sdb_client.h"
#include "file_sync_service.h"

enum {
    IGNORE_DATA,
    WIPE_DATA,
    FLASH_DATA
};
#if 0 //eric
static int do_cmd(transport_type ttype, char* serial, char *cmd, ...);

void get_my_path(char *s, size_t maxLen);

int find_sync_dirs(const char *srcarg,
        char **android_srcdir_out, char **data_srcdir_out);

int install_app(transport_type transport, char* serial, int argc, char** argv); eric
int uninstall_app(transport_type transport, char* serial, int argc, char** argv);

static const char *gProductOutPath = NULL;

static char *product_file(const char *extra)
{
    int n;
    char *x;

    if (gProductOutPath == NULL) {
        fprintf(stderr, "sdb: Product directory not specified; "
                "use -p or define ANDROID_PRODUCT_OUT\n");
        exit(1);
    }

    n = strlen(gProductOutPath) + strlen(extra) + 2;
    x = malloc(n);
    if (x == 0) {
        fprintf(stderr, "sdb: Out of memory (product_file())\n");
        exit(1);
    }

    snprintf(x, (size_t)n, "%s" OS_PATH_SEPARATOR_STR "%s", gProductOutPath, extra);
    return x;
}
#endif
void version(FILE * out) {
    fprintf(out, "Smart Development Bridge version %d.%d.%d\n",
         SDB_VERSION_MAJOR, SDB_VERSION_MINOR, SDB_SERVER_VERSION);
}

void help()
{
    version(stderr);

    fprintf(stderr,
	"\n"
	" Usage : sdb [option] <command> [parameters]\n"
        "\n"
	" options:\n"
	" -d                            - directs command to the only connected USB device\n"
	"                                 returns an error if more than one USB device is present.\n"
	" -e                            - directs command to the only running emulator.\n"
	"                                 returns an error if more than one emulator is running.\n"
	" -s <serial number>            - directs command to the USB device or emulator with\n"
	"                                 the given serial number.\n"
	" devices                       - list all connected devices\n"
	"\n"
	" commands:\n"
	"  sdb push <local> <remote>    - copy file/dir to device\n"
	"  sdb pull <remote> [<local>]  - copy file/dir from device\n"
	"  sdb shell                    - run remote shell interactively\n"
	"  sdb shell <command>          - run remote shell command\n"
	"  sdb dlog [ <filter-spec> ]   - view device log\n"
	"  sdb forward <local> <remote> - forward socket connections\n"
	"                                 forward spec is : \n"
	"                                   tcp:<port>\n"
	"  sdb help                     - show this help message\n"
	"  sdb version                  - show version num\n"
        "\n"
        "  sdb start-server             - ensure that there is a server running\n"
        "  sdb kill-server              - kill the server if it is running\n"
        "  sdb get-state                - prints: offline | bootloader | device\n"
        "  sdb get-serialno             - prints: <serial-number>\n"
        "  sdb status-window            - continuously print device status for a specified device\n"
        "\n"
        );
}

int usage()
{
    help();
    return 1;
}

#ifdef HAVE_TERMIO_H
static struct termios tio_save;

static void stdin_raw_init(int fd)
{
    struct termios tio;

    if(tcgetattr(fd, &tio)) return;
    if(tcgetattr(fd, &tio_save)) return;

    tio.c_lflag = 0; /* disable CANON, ECHO*, etc */

        /* no timeout but request at least one character per read */
    tio.c_cc[VTIME] = 0;
    tio.c_cc[VMIN] = 1;

    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIFLUSH);
}

static void stdin_raw_restore(int fd)
{
    tcsetattr(fd, TCSANOW, &tio_save);
    tcflush(fd, TCIFLUSH);
}
#endif

static void read_and_dump(int fd)
{
    char buf[4096];
    int len;

    while(fd >= 0) {
        len = sdb_read(fd, buf, 4096);
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
    int fd, fdi;
    unsigned char buf[1024];
    int r, n;
    int state = 0;

    int *fds = (int*) x;
    fd = fds[0];
    fdi = fds[1];
    free(fds);

    for(;;) {
        /* fdi is really the client's stdin, so use read, not sdb_read here */
        r = unix_read(fdi, buf, 1024);
        if(r == 0) break;
        if(r < 0) {
            if(errno == EINTR) continue;
            break;
        }
        for(n = 0; n < r; n++){
            switch(buf[n]) {
            case '\n':
                state = 1;
                break;
            case '\r':
                state = 1;
                break;
            case '~':
                if(state == 1) state++;
                break;
            case '.':
                if(state == 2) {
                    fprintf(stderr,"\n* disconnect *\n");
#ifdef HAVE_TERMIO_H
                    stdin_raw_restore(fdi);
#endif
                    exit(0);
                }
            default:
                state = 0;
            }
        }
        r = sdb_write(fd, buf, r);
        if(r <= 0) {
            break;
        }
    }
    return 0;
}

int interactive_shell(void)
{
    sdb_thread_t thr;
    int fdi, fd;
    int *fds;

    fd = sdb_connect("shell:");
    if(fd < 0) {
        fprintf(stderr,"error: %s\n", sdb_error());
        return 1;
    }
    fdi = 0; //dup(0);

    fds = malloc(sizeof(int) * 2);
    fds[0] = fd;
    fds[1] = fdi;

#ifdef HAVE_TERMIO_H
    stdin_raw_init(fdi);
#endif
    sdb_thread_create(&thr, stdin_read_thread, fds);
    read_and_dump(fd);
#ifdef HAVE_TERMIO_H
    stdin_raw_restore(fdi);
#endif
    return 0;
}


static void format_host_command(char* buffer, size_t  buflen, const char* command, transport_type ttype, const char* serial)
{
    if (serial) {
        snprintf(buffer, buflen, "host-serial:%s:%s", serial, command);
    } else {
        const char* prefix = "host";
        if (ttype == kTransportUsb)
            prefix = "host-usb";
        else if (ttype == kTransportLocal)
            prefix = "host-local";

        snprintf(buffer, buflen, "%s:%s", prefix, command);
    }
}

static void status_window(transport_type ttype, const char* serial)
{
    char command[4096];
    char *state = 0;
    char *laststate = 0;

        /* silence stderr */
#ifdef _WIN32
    /* XXX: TODO */
#else
    int  fd;
    fd = unix_open("/dev/null", O_WRONLY);
    dup2(fd, 2);
    sdb_close(fd);
#endif

    format_host_command(command, sizeof command, "get-state", ttype, serial);

    for(;;) {
        sdb_sleep_ms(250);

        if(state) {
            free(state);
            state = 0;
        }

        state = sdb_query(command);

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
}

/** duplicate string and quote all \ " ( ) chars + space character. */
static char *
dupAndQuote(const char *s)
{
    const char *ts;
    size_t alloc_len;
    char *ret;
    char *dest;

    ts = s;

    alloc_len = 0;

    for( ;*ts != '\0'; ts++) {
        alloc_len++;
        if (*ts == ' ' || *ts == '"' || *ts == '\\' || *ts == '(' || *ts == ')') {
            alloc_len++;
        }
    }

    ret = (char *)malloc(alloc_len + 1);

    ts = s;
    dest = ret;

    for ( ;*ts != '\0'; ts++) {
        if (*ts == ' ' || *ts == '"' || *ts == '\\' || *ts == '(' || *ts == ')') {
            *dest++ = '\\';
        }

        *dest++ = *ts;
    }

    *dest++ = '\0';

    return ret;
}
#if 0 //eric
/**
 * Run ppp in "notty" mode against a resource listed as the first parameter
 * eg:
 *
 * ppp dev:/dev/omap_csmi_tty0 <ppp options>
 *
 */
int ppp(int argc, char **argv)
{
#ifdef HAVE_WIN32_PROC
    fprintf(stderr, "error: sdb %s not implemented on Win32\n", argv[0]);
    return -1;
#else
    char *sdb_service_name;
    pid_t pid;
    int fd;

    if (argc < 2) {
        fprintf(stderr, "usage: sdb %s <sdb service name> [ppp opts]\n",
                argv[0]);

        return 1;
    }

    sdb_service_name = argv[1];

    fd = sdb_connect(sdb_service_name);

    if(fd < 0) {
        fprintf(stderr,"Error: Could not open sdb service: %s. Error: %s\n",
                sdb_service_name, sdb_error());
        return 1;
    }

    pid = fork();

    if (pid < 0) {
        perror("from fork()");
        return 1;
    } else if (pid == 0) {
        int err;
        int i;
        const char **ppp_args;

        // copy args
        ppp_args = (const char **) alloca(sizeof(char *) * argc + 1);
        ppp_args[0] = "pppd";
        for (i = 2 ; i < argc ; i++) {
            //argv[2] and beyond become ppp_args[1] and beyond
            ppp_args[i - 1] = argv[i];
        }
        ppp_args[i-1] = NULL;

        // child side

        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        sdb_close(STDERR_FILENO);
        sdb_close(fd);

        err = execvp("pppd", (char * const *)ppp_args);

        if (err < 0) {
            perror("execing pppd");
        }
        exit(-1);
    } else {
        // parent side

        sdb_close(fd);
        return 0;
    }
#endif /* !HAVE_WIN32_PROC */
}
#endif
static int send_shellcommand(transport_type transport, char* serial, char* buf)
{
    int fd, ret;

    for(;;) {
        fd = sdb_connect(buf);
        if(fd >= 0)
            break;
        fprintf(stderr,"- waiting for device -\n");
        sdb_sleep_ms(1000);
//        do_cmd(transport, serial, "wait-for-device", 0);
    }

    read_and_dump(fd);
    ret = sdb_close(fd);
    if (ret)
        perror("close");

    return ret;
}

static int logcat(transport_type transport, char* serial, int argc, char **argv)
{
    char buf[4096];

    char *log_tags;
    char *quoted_log_tags;

    log_tags = getenv("ANDROID_LOG_TAGS");
    quoted_log_tags = dupAndQuote(log_tags == NULL ? "" : log_tags);

    snprintf(buf, sizeof(buf),
        "shell:export ANDROID_LOG_TAGS=\"\%s\" ; exec dlogutil",
        //"shell:export ANDROID_LOG_TAGS=\"\%s\" ; exec logcat",
        quoted_log_tags);

    free(quoted_log_tags);

    argc -= 1;
    argv += 1;
    while(argc-- > 0) {
        char *quoted;

        quoted = dupAndQuote (*argv++);

        strncat(buf, " ", sizeof(buf)-1);
        strncat(buf, quoted, sizeof(buf)-1);
        free(quoted);
    }

    send_shellcommand(transport, serial, buf);
    return 0;
}
#if 0 //eric
#define SENTINEL_FILE "config" OS_PATH_SEPARATOR_STR "envsetup.make"
static int top_works(const char *top)
{
    if (top != NULL && sdb_is_absolute_host_path(top)) {
        char path_buf[PATH_MAX];
        snprintf(path_buf, sizeof(path_buf),
                "%s" OS_PATH_SEPARATOR_STR SENTINEL_FILE, top);
        return access(path_buf, F_OK) == 0;
    }
    return 0;
}

static char *find_top_from(const char *indir, char path_buf[PATH_MAX])
{
    strcpy(path_buf, indir);
    while (1) {
        if (top_works(path_buf)) {
            return path_buf;
        }
        char *s = sdb_dirstop(path_buf);
        if (s != NULL) {
            *s = '\0';
        } else {
            path_buf[0] = '\0';
            return NULL;
        }
    }
}

static char *find_top(char path_buf[PATH_MAX])
{
    char *top = getenv("ANDROID_BUILD_TOP");
    if (top != NULL && top[0] != '\0') {
        if (!top_works(top)) {
            fprintf(stderr, "sdb: bad ANDROID_BUILD_TOP value \"%s\"\n", top);
            return NULL;
        }
    } else {
        top = getenv("TOP");
        if (top != NULL && top[0] != '\0') {
            if (!top_works(top)) {
                fprintf(stderr, "sdb: bad TOP value \"%s\"\n", top);
                return NULL;
            }
        } else {
            top = NULL;
        }
    }

    if (top != NULL) {
        /* The environment pointed to a top directory that works.
         */
        strcpy(path_buf, top);
        return path_buf;
    }

    /* The environment didn't help.  Walk up the tree from the CWD
     * to see if we can find the top.
     */
    char dir[PATH_MAX];
    top = find_top_from(getcwd(dir, sizeof(dir)), path_buf);
    if (top == NULL) {
        /* If the CWD isn't under a good-looking top, see if the
         * executable is.
         */
        get_my_path(dir, PATH_MAX);
        top = find_top_from(dir, path_buf);
    }
    return top;
}

/* <hint> may be:
 * - A simple product name
 *   e.g., "sooner"
TODO: debug?  sooner-debug, sooner:debug?
 * - A relative path from the CWD to the ANDROID_PRODUCT_OUT dir
 *   e.g., "out/target/product/sooner"
 * - An absolute path to the PRODUCT_OUT dir
 *   e.g., "/src/device/out/target/product/sooner"
 *
 * Given <hint>, try to construct an absolute path to the
 * ANDROID_PRODUCT_OUT dir.
 */
static const char *find_product_out_path(const char *hint)
{
    static char path_buf[PATH_MAX];

    if (hint == NULL || hint[0] == '\0') {
        return NULL;
    }

    /* If it's already absolute, don't bother doing any work.
     */
    if (sdb_is_absolute_host_path(hint)) {
        strcpy(path_buf, hint);
        return path_buf;
    }

    /* If there are any slashes in it, assume it's a relative path;
     * make it absolute.
     */
    if (sdb_dirstart(hint) != NULL) {
        if (getcwd(path_buf, sizeof(path_buf)) == NULL) {
            fprintf(stderr, "sdb: Couldn't get CWD: %s\n", strerror(errno));
            return NULL;
        }
        if (strlen(path_buf) + 1 + strlen(hint) >= sizeof(path_buf)) {
            fprintf(stderr, "sdb: Couldn't assemble path\n");
            return NULL;
        }
        strcat(path_buf, OS_PATH_SEPARATOR_STR);
        strcat(path_buf, hint);
        return path_buf;
    }

    /* It's a string without any slashes.  Try to do something with it.
     *
     * Try to find the root of the build tree, and build a PRODUCT_OUT
     * path from there.
     */
    char top_buf[PATH_MAX];
    const char *top = find_top(top_buf);
    if (top == NULL) {
        fprintf(stderr, "sdb: Couldn't find top of build tree\n");
        return NULL;
    }
//TODO: if we have a way to indicate debug, look in out/debug/target/...
    snprintf(path_buf, sizeof(path_buf),
            "%s" OS_PATH_SEPARATOR_STR
            "out" OS_PATH_SEPARATOR_STR
            "target" OS_PATH_SEPARATOR_STR
            "product" OS_PATH_SEPARATOR_STR
            "%s", top_buf, hint);
    if (access(path_buf, F_OK) < 0) {
        fprintf(stderr, "sdb: Couldn't find a product dir "
                "based on \"-p %s\"; \"%s\" doesn't exist\n", hint, path_buf);
        return NULL;
    }
    return path_buf;
}
#endif
int sdb_commandline(int argc, char **argv)
{
    char buf[4096];
    int is_daemon = 0;
#if 0 //eric
    int no_daemon = 0;
    int persist = 0;
    char* server_port_str = NULL;
#endif
    int r;
    int quote;
    transport_type ttype = kTransportAny;
    char* serial = NULL;

#if 0 //eric
        /* If defined, this should be an absolute path to
         * the directory containing all of the various system images
         * for a particular product.  If not defined, and the sdb
         * command requires this information, then the user must
         * specify the path using "-p".
         */
    gProductOutPath = getenv("ANDROID_PRODUCT_OUT");
    if (gProductOutPath == NULL || gProductOutPath[0] == '\0') {
        gProductOutPath = NULL;
    }
    // TODO: also try TARGET_PRODUCT/TARGET_DEVICE as a hint

    serial = getenv("ANDROID_SERIAL");
#endif
    /* Validate and assign the server port */
    int server_port = DEFAULT_SDB_PORT;
#if 0 //eric
    server_port_str = getenv("ANDROID_SDB_SERVER_PORT");
    if (server_port_str && strlen(server_port_str) > 0) {
        server_port = (int) strtol(server_port_str, NULL, 0);
        if (server_port <= 0) {
            fprintf(stderr,
                    "sdb: Env var ANDROID_SDB_SERVER_PORT must be a positive number. Got \"%s\"\n",
                    server_port_str);
            return usage();
        }
    }
#endif
    /* modifiers and flags */
    while(argc > 0) {
#if 0 //eric
        if(!strcmp(argv[0],"nodaemon")) {
            no_daemon = 1;
        } else 
#endif
		if (!strcmp(argv[0], "fork-server")) {
            /* this is a special flag used only when the SDB client launches the SDB Server */
            is_daemon = 1;
#if 0 //eric
        } else if(!strcmp(argv[0],"persist")) {
            persist = 1;
        } else if(!strncmp(argv[0], "-p", 2)) {
            const char *product = NULL;
            if (argv[0][2] == '\0') {
                if (argc < 2) return usage();
                product = argv[1];
                argc--;
                argv++;
            } else {
                product = argv[1] + 2;
            }
            gProductOutPath = find_product_out_path(product);
            if (gProductOutPath == NULL) {
                fprintf(stderr, "sdb: could not resolve \"-p %s\"\n",
                        product);
                return usage();
            }
#endif
        } else if (argv[0][0]=='-' && argv[0][1]=='s') {
            if (isdigit(argv[0][2])) {
                serial = argv[0] + 2;
            } else {
                if(argc < 2 || argv[0][2] != '\0') return usage();
                serial = argv[1];
                argc--;
                argv++;
            }
        } else if (!strcmp(argv[0],"-d")) {
            ttype = kTransportUsb;
        } else if (!strcmp(argv[0],"-e")) {
            ttype = kTransportLocal;
        } else {
                /* out of recognized modifiers and flags */
            break;
        }
        argc--;
        argv++;
    }

    sdb_set_transport(ttype, serial);
    sdb_set_tcp_specifics(server_port);

    if ((argc > 0) && (!strcmp(argv[0],"server"))) {
//        if (no_daemon || is_daemon) {
        if (is_daemon) {
            r = sdb_main(is_daemon, server_port);
        } else {
            r = launch_server(server_port);
        }
        if(r) {
            fprintf(stderr,"* could not start server *\n");
        }
        return r;
    }

//top:
    if(argc == 0) {
        return usage();
    }

    /* sdb_connect() commands */

    if(!strcmp(argv[0], "devices")) {
        char *tmp;
        snprintf(buf, sizeof buf, "host:%s", argv[0]);
        tmp = sdb_query(buf);
        if(tmp) {
            printf("List of devices attached \n");
            printf("%s\n", tmp);
            return 0;
        } else {
            return 1;
        }
    }

    if(!strcmp(argv[0], "connect")) {
        char *tmp;
        if (argc != 2) {
            fprintf(stderr, "Usage: sdb connect <host>[:<port>]\n");
            return 1;
        }
        snprintf(buf, sizeof buf, "host:connect:%s", argv[1]);
        tmp = sdb_query(buf);
        if(tmp) {
            printf("%s\n", tmp);
            return 0;
        } else {
            return 1;
        }
    }

    if(!strcmp(argv[0], "disconnect")) {
        char *tmp;
        if (argc > 2) {
            fprintf(stderr, "Usage: sdb disconnect [<host>[:<port>]]\n");
            return 1;
        }
        if (argc == 2) {
            snprintf(buf, sizeof buf, "host:disconnect:%s", argv[1]);
        } else {
            snprintf(buf, sizeof buf, "host:disconnect:");
        }
        tmp = sdb_query(buf);
        if(tmp) {
            printf("%s\n", tmp);
            return 0;
        } else {
            return 1;
        }
    }

    if (!strcmp(argv[0], "emu")) {
        return sdb_send_emulator_command(argc, argv);
    }

    if(!strcmp(argv[0], "shell")) {
        int r;
        int fd;

        if(argc < 2) {
            return interactive_shell();
        }

        snprintf(buf, sizeof buf, "shell:%s", argv[1]);
        argc -= 2;
        argv += 2;
        while(argc-- > 0) {
            strcat(buf, " ");

            /* quote empty strings and strings with spaces */
            quote = (**argv == 0 || strchr(*argv, ' '));
            if (quote)
                strcat(buf, "\"");
            strcat(buf, *argv++);
            if (quote)
                strcat(buf, "\"");
        }

        for(;;) {
            fd = sdb_connect(buf);
            if(fd >= 0) {
                read_and_dump(fd);
                sdb_close(fd);
                r = 0;
            } else {
                fprintf(stderr,"error: %s\n", sdb_error());
                r = -1;
            }
#if 0 //eric
            if(persist) {
                fprintf(stderr,"\n- waiting for device -\n");
                sdb_sleep_ms(1000);
                do_cmd(ttype, serial, "wait-for-device", 0);
            } else {
#endif
            return r;
#if 0 //eric
            }
#endif
        }
    }

    if(!strcmp(argv[0], "kill-server")) {
        int fd;
        fd = _sdb_connect("host:kill");
        if(fd == -1) {
            fprintf(stderr,"* server not running *\n");
            return 1;
        }
        return 0;
    }

//    if(!strcmp(argv[0], "remount") || !strcmp(argv[0], "reboot")
//            || !strcmp(argv[0], "reboot-bootloader")
//            || !strcmp(argv[0], "tcpip") || !strcmp(argv[0], "usb")
//            || !strcmp(argv[0], "root")) {
//        char command[100];
//        if (!strcmp(argv[0], "reboot-bootloader"))
//            snprintf(command, sizeof(command), "reboot:bootloader");
//        else if (argc > 1)
//            snprintf(command, sizeof(command), "%s:%s", argv[0], argv[1]);
//        else
//            snprintf(command, sizeof(command), "%s:", argv[0]);
//        int fd = sdb_connect(command);
//        if(fd >= 0) {
//            read_and_dump(fd);
//            sdb_close(fd);
//            return 0;
//        }
//        fprintf(stderr,"error: %s\n", sdb_error());
//        return 1;
//    }

//    if(!strcmp(argv[0], "bugreport")) {
//        if (argc != 1) return usage();
//        do_cmd(ttype, serial, "shell", "bugreport", 0);
//        return 0;
//    }

    /* sdb_command() wrapper commands */

//    if(!strncmp(argv[0], "wait-for-", strlen("wait-for-"))) {
//        char* service = argv[0];
//        if (!strncmp(service, "wait-for-device", strlen("wait-for-device"))) {
//            if (ttype == kTransportUsb) {
//                service = "wait-for-usb";
//            } else if (ttype == kTransportLocal) {
//                service = "wait-for-local";
//            } else {
//                service = "wait-for-any";
//            }
//        }
//
//        format_host_command(buf, sizeof buf, service, ttype, serial);
//
//        if (sdb_command(buf)) {
//            D("failure: %s *\n",sdb_error());
//            fprintf(stderr,"error: %s\n", sdb_error());
//            return 1;
//        }
//
//        /* Allow a command to be run after wait-for-device,
//            * e.g. 'sdb wait-for-device shell'.
//            */
//        if(argc > 1) {
//            argc--;
//            argv++;
//            goto top;
//        }
//        return 0;
//    }

    if(!strcmp(argv[0], "forward")) {
        if(argc != 3) return usage();
        if (serial) {
            snprintf(buf, sizeof buf, "host-serial:%s:forward:%s;%s",serial, argv[1], argv[2]);
        } else if (ttype == kTransportUsb) {
            snprintf(buf, sizeof buf, "host-usb:forward:%s;%s", argv[1], argv[2]);
        } else if (ttype == kTransportLocal) {
            snprintf(buf, sizeof buf, "host-local:forward:%s;%s", argv[1], argv[2]);
        } else {
            snprintf(buf, sizeof buf, "host:forward:%s;%s", argv[1], argv[2]);
        }
        if(sdb_command(buf)) {
            fprintf(stderr,"error: %s\n", sdb_error());
            return 1;
        }
        return 0;
    }

    /* do_sync_*() commands */
#if 0 //eric
    if(!strcmp(argv[0], "ls")) {
        if(argc != 2) return usage();
        return do_sync_ls(argv[1]);
    }
#endif
    if(!strcmp(argv[0], "push")) {
        if(argc != 3) return usage();
        return do_sync_push(argv[1], argv[2], 0 /* no verify APK */);
    }

    if(!strcmp(argv[0], "pull")) {
        if (argc == 2) {
            return do_sync_pull(argv[1], ".");
        } else if (argc == 3) {
            return do_sync_pull(argv[1], argv[2]);
        } else {
            return usage();
        }
    }

//    if(!strcmp(argv[0], "install")) {
//        if (argc < 2) return usage();
//        return install_app(ttype, serial, argc, argv);
//    }
//
//    if(!strcmp(argv[0], "uninstall")) {
//        if (argc < 2) return usage();
//        return uninstall_app(ttype, serial, argc, argv);
//    }

//    if(!strcmp(argv[0], "sync")) {
//        char *srcarg, *android_srcpath, *data_srcpath;
//        int listonly = 0;
//
//        int ret;
//        if(argc < 2) {
//            /* No local path was specified. */
//            srcarg = NULL;
//        } else if (argc >= 2 && strcmp(argv[1], "-l") == 0) {
//            listonly = 1;
//            if (argc == 3) {
//                srcarg = argv[2];
//            } else {
//                srcarg = NULL;
//            }
//        } else if(argc == 2) {
//            /* A local path or "android"/"data" arg was specified. */
//            srcarg = argv[1];
//        } else {
//            return usage();
//        }
//        ret = find_sync_dirs(srcarg, &android_srcpath, &data_srcpath);
//        if(ret != 0) return usage();
//
//        if(android_srcpath != NULL)
//            ret = do_sync_sync(android_srcpath, "/system", listonly);
//        if(ret == 0 && data_srcpath != NULL)
//            ret = do_sync_sync(data_srcpath, "/data", listonly);
//
//        free(android_srcpath);
//        free(data_srcpath);
//        return ret;
//    }

    /* passthrough commands */

    if(!strcmp(argv[0],"get-state") ||
        !strcmp(argv[0],"get-serialno"))
    {
        char *tmp;

        format_host_command(buf, sizeof buf, argv[0], ttype, serial);
        tmp = sdb_query(buf);
        if(tmp) {
            printf("%s\n", tmp);
            return 0;
        } else {
            return 1;
        }
    }

    /* other commands */

    if(!strcmp(argv[0],"status-window")) {
        status_window(ttype, serial);
        return 0;
    }

    if(!strcmp(argv[0],"dlog")) {
        return logcat(ttype, serial, argc, argv);
    }

//    if(!strcmp(argv[0],"ppp")) {
//        return ppp(argc, argv);
//    }

    if (!strcmp(argv[0], "start-server")) {
        return sdb_connect("host:start-server");
    }

//    if (!strcmp(argv[0], "jdwp")) {
//        int  fd = sdb_connect("jdwp");
//        if (fd >= 0) {
//            read_and_dump(fd);
//            sdb_close(fd);
//            return 0;
//        } else {
//            fprintf(stderr, "error: %s\n", sdb_error());
//            return -1;
//        }
//    }

    /* "sdb /?" is a common idiom under Windows */
    if(!strcmp(argv[0], "help") || !strcmp(argv[0], "/?")) {
        help();
        return 0;
    }

    if(!strcmp(argv[0], "version")) {
        version(stdout);
        return 0;
    }

    usage();
    return 1;
}
#if 0 //eric
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
    while((argv[argc] = va_arg(ap, char*)) != 0) argc++;
    va_end(ap);

#if 0
    int n;
    fprintf(stderr,"argc = %d\n",argc);
    for(n = 0; n < argc; n++) {
        fprintf(stderr,"argv[%d] = \"%s\"\n", n, argv[n]);
    }
#endif

    return sdb_commandline(argc, argv);
}

int find_sync_dirs(const char *srcarg,
        char **android_srcdir_out, char **data_srcdir_out)
{
    char *android_srcdir, *data_srcdir;

    if(srcarg == NULL) {
        android_srcdir = product_file("system");
        data_srcdir = product_file("data");
    } else {
        /* srcarg may be "data", "system" or NULL.
         * if srcarg is NULL, then both data and system are synced
         */
        if(strcmp(srcarg, "system") == 0) {
            android_srcdir = product_file("system");
            data_srcdir = NULL;
        } else if(strcmp(srcarg, "data") == 0) {
            android_srcdir = NULL;
            data_srcdir = product_file("data");
        } else {
            /* It's not "system" or "data".
             */
            return 1;
        }
    }

    if(android_srcdir_out != NULL)
        *android_srcdir_out = android_srcdir;
    else
        free(android_srcdir);

    if(data_srcdir_out != NULL)
        *data_srcdir_out = data_srcdir;
    else
        free(data_srcdir);

    return 0;
}
#endif
static int pm_command(transport_type transport, char* serial,
                      int argc, char** argv)
{
    char buf[4096];

    snprintf(buf, sizeof(buf), "shell:pm");

    while(argc-- > 0) {
        char *quoted;

        quoted = dupAndQuote(*argv++);

        strncat(buf, " ", sizeof(buf)-1);
        strncat(buf, quoted, sizeof(buf)-1);
        free(quoted);
    }

    send_shellcommand(transport, serial, buf);
    return 0;
}

int uninstall_app(transport_type transport, char* serial, int argc, char** argv)
{
    /* if the user choose the -k option, we refuse to do it until devices are
       out with the option to uninstall the remaining data somehow (sdb/ui) */
    if (argc == 3 && strcmp(argv[1], "-k") == 0)
    {
        printf(
            "The -k option uninstalls the application while retaining the data/cache.\n"
            "At the moment, there is no way to remove the remaining data.\n"
            "You will have to reinstall the application with the same signature, and fully uninstall it.\n"
            "If you truly wish to continue, execute 'sdb shell pm uninstall -k %s'\n", argv[2]);
        return -1;
    }

    /* 'sdb uninstall' takes the same arguments as 'pm uninstall' on device */
    return pm_command(transport, serial, argc, argv);
}

#if 0 //eric
static int delete_file(transport_type transport, char* serial, char* filename)
{
    char buf[4096];
    char* quoted;

    snprintf(buf, sizeof(buf), "shell:rm ");
    quoted = dupAndQuote(filename);
    strncat(buf, quoted, sizeof(buf)-1);
    free(quoted);

    send_shellcommand(transport, serial, buf);
    return 0;
}

int install_app(transport_type transport, char* serial, int argc, char** argv)
{
    struct stat st;
    int err;
    const char *const DATA_DEST = "/data/local/tmp/%s";
    const char *const SD_DEST = "/sdcard/tmp/%s";
    const char* where = DATA_DEST;
    char to[PATH_MAX];
    char* filename = argv[argc - 1];
    const char* p;
    int i;

    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-s"))
            where = SD_DEST;
    }

    p = sdb_dirstop(filename);
    if (p) {
        p++;
        snprintf(to, sizeof to, where, p);
    } else {
        snprintf(to, sizeof to, where, filename);
    }
    if (p[0] == '\0') {
    }

    err = stat(filename, &st);
    if (err != 0) {
        fprintf(stderr, "can't find '%s' to install\n", filename);
        return 1;
    }
    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "can't install '%s' because it's not a file\n",
                filename);
        return 1;
    }

    if (!(err = do_sync_push(filename, to, 1 /* verify APK */))) {
        /* file in place; tell the Package Manager to install it */
        argv[argc - 1] = to;       /* destination name, not source location */
        pm_command(transport, serial, argc, argv);
        delete_file(transport, serial, to);
    }

    return err;
}
#endif
