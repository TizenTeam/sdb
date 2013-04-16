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

#include "sysdeps.h"

#ifdef HAVE_TERMIO_H
#include <termios.h>
#endif

#define  TRACE_TAG  TRACE_SDB
#include "sdb.h"
#include "sdb_client.h"
#include "file_sync_service.h"

static int do_cmd(transport_type ttype, char* serial, char *cmd, ...);

void get_my_path(char *s, size_t maxLen);
int find_sync_dirs(const char *srcarg,
        char **android_srcdir_out, char **data_srcdir_out);
int install_app(transport_type transport, char* serial, int argc, char** argv);
int uninstall_app_sdb(const char *app_id);
int install_app_sdb(const char *srcpath);
int uninstall_app(transport_type transport, char* serial, int argc, char** argv);
int get_pkgtype_file_name(const char* file_name);
int get_pkgtype_from_app_id(const char* app_id);
int sdb_command2(const char* cmd);
int launch_app(transport_type transport, char* serial, int argc, char** argv);
void version_sdbd(transport_type ttype, char* serial);

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
    " -d                            - direct command to the only connected USB device\n"
    "                                 return an error if more than one USB device is present.\n"
    " -e                            - direct command to the only running emulator.\n"
    "                                 return an error if more than one emulator is running.\n"
    " -s <serial number>            - direct command to the USB device or emulator with\n"
    "                                 the given serial number.\n"
    " devices                       - list all connected devices\n"
    " connect <host>[:<port>]       - connect to a device via TCP/IP\n"
    "                                 Port 26101 is used by default if no port number is specified.\n"
    " disconnect [<host>[:<port>]]  - disconnect from a TCP/IP device.\n"
    "                                 Port 26101 is used by default if no port number is specified.\n"
    "                                 Using this command with no additional arguments\n"
    "                                 will disconnect from all connected TCP/IP devices.\n"
    "\n"
    " commands:\n"
    "  sdb push <local> <remote> [--with-utf8]\n"
    "                               - copy file/dir to device\n"
    "                                 (--with-utf8 means to create the remote file with utf-8 character encoding)\n"
    "  sdb pull <remote> [<local>]  - copy file/dir from device\n"
    "  sdb shell                    - run remote shell interactively\n"
    "  sdb shell <command>          - run remote shell \n"
    "  sdb dlog [<filter-spec>]     - view device log\n"
    "  sdb install <pkg_path>       - push package file and install it\n"
    "  sdb uninstall <appid>        - uninstall this app from the device\n"
    "  sdb forward <local> <remote> - forward socket connections\n"

    "                                 forward spec is : \n"
    "                                   tcp:<port>\n"
    "  sdb help                     - show this help message\n"
    "  sdb version                  - show version num\n"
    "\n"
    "  sdb start-server             - ensure that there is a server running\n"
    "  sdb kill-server              - kill the server if it is running\n"
    "  sdb get-state                - print: offline | bootloader | device\n"
    "  sdb get-serialno             - print: <serial-number>\n"
    "  sdb status-window            - continuously print device status for a specified device\n"
//    "  sdb usb                      - restarts the sdbd daemon listing on USB\n"
//    "  sdb tcpip                    - restarts the sdbd daemon listing on TCP\n"
    "  sdb root <on|off>            - switch to root or developer account mode\n"
    "                                 'on' means to root mode, and vice versa"
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
        D("read_and_dump(): pre sdb_read(fd=%d)\n", fd);
        len = sdb_read(fd, buf, 4096);
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

static void copy_to_file(int inFd, int outFd) {
    const size_t BUFSIZE = 32 * 1024;
    char* buf = (char*) malloc(BUFSIZE);
    int len;
    long total = 0;

    D("copy_to_file(%d -> %d)\n", inFd, outFd);
    for (;;) {
        len = sdb_read(inFd, buf, BUFSIZE);
        if (len == 0) {
            D("copy_to_file() : read 0 bytes; exiting\n");
            break;
        }
        if (len < 0) {
            if (errno == EINTR) {
                D("copy_to_file() : EINTR, retrying\n");
                continue;
            }
            D("copy_to_file() : error %d\n", errno);
            break;
        }
        sdb_write(outFd, buf, len);
        total += len;
    }
    D("copy_to_file() finished after %lu bytes\n", total);
    free(buf);
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
        D("stdin_read_thread(): pre unix_read(fdi=%d,...)\n", fdi);
        r = unix_read(fdi, buf, 1024);
        D("stdin_read_thread(): post unix_read(fdi=%d,...)\n", fdi);
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
#if 0 /* tizen specific */
int sdb_download_buffer(const char *service, const void* data, int sz,
                        unsigned progress)
{
    char buf[4096];
    unsigned total;
    int fd;
    const unsigned char *ptr;

    sprintf(buf,"%s:%d", service, sz);
    fd = sdb_connect(buf);
    if(fd < 0) {
        fprintf(stderr,"error: %s\n", sdb_error());
        return -1;
    }

    int opt = CHUNK_SIZE;
    opt = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt));

    total = sz;
    ptr = data;

    if(progress) {
        char *x = strrchr(service, ':');
        if(x) service = x + 1;
    }

    while(sz > 0) {
        unsigned xfer = (sz > CHUNK_SIZE) ? CHUNK_SIZE : sz;
        if(writex(fd, ptr, xfer)) {
            sdb_status(fd);
            fprintf(stderr,"* failed to write data '%s' *\n", sdb_error());
            return -1;
        }
        sz -= xfer;
        ptr += xfer;
        if(progress) {
            printf("sending: '%s' %4d%%    \r", service, (int)(100LL - ((100LL * sz) / (total))));
            fflush(stdout);
        }
    }
    if(progress) {
        printf("\n");
    }

    if(readx(fd, buf, 4)){
        fprintf(stderr,"* error reading response *\n");
        sdb_close(fd);
        return -1;
    }
    if(memcmp(buf, "OKAY", 4)) {
        buf[4] = 0;
        fprintf(stderr,"* error response '%s' *\n", buf);
        sdb_close(fd);
        return -1;
    }

    sdb_close(fd);
    return 0;
}

int sdb_download(const char *service, const char *fn, unsigned progress)
{
    void *data;
    unsigned sz;

    data = load_file(fn, &sz);
    if(data == 0) {
        fprintf(stderr,"* cannot read '%s' *\n", service);
        return -1;
    }

    int status = sdb_download_buffer(service, data, sz, progress);
    free(data);
    return status;
}
#endif
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

static int send_shellcommand(transport_type transport, char* serial, char* buf)
{
    int fd, ret;

    for(;;) {
        fd = sdb_connect(buf);
        if(fd >= 0)
            break;
        fprintf(stderr,"- waiting for device -\n");
        sdb_sleep_ms(1000);
        do_cmd(transport, serial, "wait-for-device", 0);
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

    snprintf(buf, sizeof(buf),
        "shell:/usr/bin/dlogutil");

/*
    if (!strcmp(argv[0],"longcat")) {
        strncat(buf, " -v long", sizeof(buf)-1);
    }
*/

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

static int mkdirs(char *path)
{
    int ret;
    char *x = path + 1;

    for(;;) {
        x = sdb_dirstart(x);
        if(x == 0) return 0;
        *x = 0;
        ret = sdb_mkdir(path, 0775);
        *x = OS_PATH_SEPARATOR;
        if((ret < 0) && (errno != EEXIST)) {
            return ret;
        }
        x++;
    }
    return 0;
}

static int backup(int argc, char** argv) {
    char buf[4096];
    char default_name[32];
    const char* filename = strcpy(default_name, "./backup.ab");
    int fd, outFd;
    int i, j;

    /* find, extract, and use any -f argument */
    for (i = 1; i < argc; i++) {
        if (!strcmp("-f", argv[i])) {
            if (i == argc-1) {
                fprintf(stderr, "sdb: -f passed with no filename\n");
                return usage();
            }
            filename = argv[i+1];
            for (j = i+2; j <= argc; ) {
                argv[i++] = argv[j++];
            }
            argc -= 2;
            argv[argc] = NULL;
        }
    }

    /* bare "sdb backup" or "sdb backup -f filename" are not valid invocations */
    if (argc < 2) return usage();

    sdb_unlink(filename);
    mkdirs((char *)filename);
    outFd = sdb_creat(filename, 0640);
    if (outFd < 0) {
        fprintf(stderr, "sdb: unable to open file %s\n", filename);
        return -1;
    }

    snprintf(buf, sizeof(buf), "backup");
    for (argc--, argv++; argc; argc--, argv++) {
        strncat(buf, ":", sizeof(buf) - strlen(buf) - 1);
        strncat(buf, argv[0], sizeof(buf) - strlen(buf) - 1);
    }

    D("backup. filename=%s buf=%s\n", filename, buf);
    fd = sdb_connect(buf);
    if (fd < 0) {
        fprintf(stderr, "sdb: unable to connect for backup\n");
        sdb_close(outFd);
        return -1;
    }

    printf("Now unlock your device and confirm the backup operation.\n");
    copy_to_file(fd, outFd);

    sdb_close(fd);
    sdb_close(outFd);
    return 0;
}

static int restore(int argc, char** argv) {
    const char* filename;
    int fd, tarFd;

    if (argc != 2) return usage();

    filename = argv[1];
    tarFd = sdb_open(filename, O_RDONLY);
    if (tarFd < 0) {
        fprintf(stderr, "sdb: unable to open file %s\n", filename);
        return -1;
    }

    fd = sdb_connect("restore:");
    if (fd < 0) {
        fprintf(stderr, "sdb: unable to connect for backup\n");
        sdb_close(tarFd);
        return -1;
    }

    printf("Now unlock your device and confirm the restore operation.\n");
    copy_to_file(tarFd, fd);

    sdb_close(fd);
    sdb_close(tarFd);
    return 0;
}

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

int sdb_commandline(int argc, char **argv)
{
    char buf[4096];
    int no_daemon = 0;
    int is_daemon = 0;
    int is_server = 0;
    int persist = 0;
    int r;
    int quote;
    transport_type ttype = kTransportAny;
    char* serial = NULL;
    char* server_port_str = NULL;

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

    /* Validate and assign the server port */
    server_port_str = getenv("ANDROID_SDB_SERVER_PORT");
    int server_port = DEFAULT_SDB_PORT;
    if (server_port_str && strlen(server_port_str) > 0) {
        server_port = (int) strtol(server_port_str, NULL, 0);
        if (server_port <= 0) {
            fprintf(stderr,
                    "sdb: Env var ANDROID_SDB_SERVER_PORT must be a positive number. Got \"%s\"\n",
                    server_port_str);
            return usage();
        }
    }

    /* modifiers and flags */
    while(argc > 0) {
        if(!strcmp(argv[0],"server")) {
            is_server = 1;
        } else if(!strcmp(argv[0],"nodaemon")) {
            no_daemon = 1;
        } else if (!strcmp(argv[0], "fork-server")) {
            /* this is a special flag used only when the SDB client launches the SDB Server */
            is_daemon = 1;
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


//    sdb_set_transport(ttype, serial);
//    sdb_set_tcp_specifics(server_port);

    if (is_server) {
        if (no_daemon || is_daemon) {
            r = sdb_main(is_daemon, server_port);
        } else {
            r = launch_server(server_port);
        }
        if(r) {
            fprintf(stderr,"* could not start server *\n");
        }
        return r;
    }

top:
    if(argc == 0) {
        return usage();
    }

    // first, get the uniq device from the partial serial with prefix matching
    if (serial) {
        char *tmp;
        snprintf(buf, sizeof(buf), "host:serial-match:%s", serial);
        tmp = sdb_query(buf);
        if (tmp) {
            //printf("connect to device: %s\n", tmp);
            serial = strdup(tmp);
            sdb_set_transport(ttype, serial);
            sdb_set_tcp_specifics(server_port);
        } else {
            return 1;
        }
    } else {
        sdb_set_transport(ttype, serial);
        sdb_set_tcp_specifics(server_port);
    }
    /* sdb_connect() commands */

    if(!strcmp(argv[0], "devices")) {
        char *tmp;
        snprintf(buf, sizeof buf, "host:%s", argv[0]);
        tmp = sdb_query(buf);
        if(tmp) {
            printf("List of devices attached \n");
            printf("%s", tmp);
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

    if(!strcmp(argv[0], "shell") || !strcmp(argv[0], "hell")) {
        int r;
        int fd;

        char h = (argv[0][0] == 'h');

        if (h) {
            printf("\x1b[41;33m");
            fflush(stdout);
        }

        if(argc < 2) {
            D("starting interactive shell\n");
            r = interactive_shell();
            if (h) {
                printf("\x1b[0m");
                fflush(stdout);
            }
            return r;
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
            D("interactive shell loop. buff=%s\n", buf);
            fd = sdb_connect(buf);
            if(fd >= 0) {
                D("about to read_and_dump(fd=%d)\n", fd);
                read_and_dump(fd);
                D("read_and_dump() done.\n");
                sdb_close(fd);
                r = 0;
            } else {
                fprintf(stderr,"error: %s\n", sdb_error());
                r = -1;
            }

            if(persist) {
                fprintf(stderr,"\n- waiting for device -\n");
                sdb_sleep_ms(1000);
                do_cmd(ttype, serial, "wait-for-device", 0);
            } else {
                if (h) {
                    printf("\x1b[0m");
                    fflush(stdout);
                }
                D("interactive shell loop. return r=%d\n", r);
                return r;
            }
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
#if 0 /* tizen specific */
    if(!strcmp(argv[0], "sideload")) {
        if(argc != 2) return usage();
        if(sdb_download("sideload", argv[1], 1)) {
            return 1;
        } else {
            return 0;
        }
    }
#endif
    if(!strcmp(argv[0], "root")) {
        char command[100];

        if (argc != 2) {
            return usage();
        }
        if (strcmp(argv[1], "on") != 0 && strcmp(argv[1], "off") != 0) {
            return usage();
        }
        snprintf(command, sizeof(command), "%s:%s", argv[0], argv[1]);
        int fd = sdb_connect(command);
        if(fd >= 0) {
            read_and_dump(fd);
            sdb_close(fd);
            return 0;
        }
        //TODO: it may be here due to connection fail not version mis-match
        fprintf(stderr, "root command requires at least version 2.1.0\n");
        return 1;
    }

    if(!strcmp(argv[0], "bugreport")) {
        if (argc != 1) return usage();
        do_cmd(ttype, serial, "shell", "bugreport", 0);
        return 0;
    }

    /* sdb_command() wrapper commands */

    if(!strncmp(argv[0], "wait-for-", strlen("wait-for-"))) {
        char* service = argv[0];
        if (!strncmp(service, "wait-for-device", strlen("wait-for-device"))) {
            if (ttype == kTransportUsb) {
                service = "wait-for-usb";
            } else if (ttype == kTransportLocal) {
                service = "wait-for-local";
            } else {
                service = "wait-for-any";
            }
        }

        format_host_command(buf, sizeof buf, service, ttype, serial);

        if (sdb_command(buf)) {
            D("failure: %s *\n",sdb_error());
            fprintf(stderr,"error: %s\n", sdb_error());
            return 1;
        }

        /* Allow a command to be run after wait-for-device,
            * e.g. 'sdb wait-for-device shell'.
            */
        if(argc > 1) {
            argc--;
            argv++;
            goto top;
        }
        return 0;
    }

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

    if(!strcmp(argv[0], "ls")) {
        if(argc != 2) return usage();
        return do_sync_ls(argv[1]);
    }

    if(!strcmp(argv[0], "push")) {
        int i=0;
        int utf8=0;

        if(argc < 3) return usage();
        if (argv[argc-1][0] == '-') {
            if (!strcmp(argv[argc-1], "--with-utf8")) {
                utf8 = 1;
                argc = argc - 1;
            } else {
                return usage();
            }
        }
        for (i=1; i<argc-1; i++) {
            do_sync_push(argv[i], argv[argc-1], 0 /* no verify APK */, utf8);
        }
        return 0;
    }

    if(!strcmp(argv[0], "install")) {
        if(argc == 2) {
            return install_app_sdb(argv[1]);
        }

        return usage();
    }

    if(!strcmp(argv[0], "uninstall")) {
        if(argc == 2) {
            return uninstall_app_sdb(argv[1]);
        }

        return usage();
    }

    if(!strcmp(argv[0], "launch")) {
        //if (argc < 2) return usage();
        //TODO : check argument validations
        return launch_app(ttype, serial, argc, argv);
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

    if(!strcmp(argv[0], "install")) {
        if (argc < 2) return usage();
        return install_app(ttype, serial, argc, argv);
    }

    if(!strcmp(argv[0], "uninstall")) {
        if (argc < 2) return usage();
        return uninstall_app(ttype, serial, argc, argv);
    }

    if(!strcmp(argv[0], "sync")) {
        char *srcarg, *android_srcpath, *data_srcpath;
        int listonly = 0;

        int ret;
        if(argc < 2) {
            /* No local path was specified. */
            srcarg = NULL;
        } else if (argc >= 2 && strcmp(argv[1], "-l") == 0) {
            listonly = 1;
            if (argc == 3) {
                srcarg = argv[2];
            } else {
                srcarg = NULL;
            }
        } else if(argc == 2) {
            /* A local path or "android"/"data" arg was specified. */
            srcarg = argv[1];
        } else {
            return usage();
        }
        ret = find_sync_dirs(srcarg, &android_srcpath, &data_srcpath);
        if(ret != 0) return usage();

        if(android_srcpath != NULL)
            ret = do_sync_sync(android_srcpath, "/system", listonly);
        if(ret == 0 && data_srcpath != NULL)
            ret = do_sync_sync(data_srcpath, "/data", listonly);

        free(android_srcpath);
        free(data_srcpath);
        return ret;
    }

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

    if(!strcmp(argv[0],"ppp")) {
        return ppp(argc, argv);
    }

    if (!strcmp(argv[0], "start-server")) {
        return sdb_connect("host:start-server");
    }

    if (!strcmp(argv[0], "backup")) {
        return backup(argc, argv);
    }

    if (!strcmp(argv[0], "restore")) {
        return restore(argc, argv);
    }

    if (!strcmp(argv[0], "jdwp")) {
        int  fd = sdb_connect("jdwp");
        if (fd >= 0) {
            read_and_dump(fd);
            sdb_close(fd);
            return 0;
        } else {
            fprintf(stderr, "error: %s\n", sdb_error());
            return -1;
        }
    }

    /* "sdb /?" is a common idiom under Windows */
    if(!strcmp(argv[0], "help") || !strcmp(argv[0], "/?")) {
        help();
        return 0;
    }

    if(!strcmp(argv[0], "version")) {
        if (ttype == kTransportUsb || ttype == kTransportLocal) {
            version_sdbd(ttype, serial);
        } else {
            version(stdout);
        }
        return 0;
    }

    usage();
    return 1;
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

int sdb_command2(const char* cmd) {
    int result = sdb_connect(cmd);

    if(result < 0) {
        return result;
    }

    D("about to read_and_dump(fd=%d)\n", result);
    read_and_dump(result);
    D("read_and_dump() done.\n");
    sdb_close(result);

    return 0;
}

int install_app_sdb(const char *srcpath) {
    D("Install start\n");
    const char * APP_DEST = "/opt/usr/apps/tmp/%s";
    const char* filename = sdb_dirstop(srcpath);
    char destination[PATH_MAX];

    if (filename) {
        filename++;
        snprintf(destination, sizeof destination, APP_DEST, filename);
    } else {
        snprintf(destination, sizeof destination, APP_DEST, srcpath);
    }

    int tpk = get_pkgtype_file_name(srcpath);
    if (tpk == -1) {
        fprintf(stderr, "error: unknown package type\n");
        return -1;
    }
    D("Push file: %s to %s\n", srcpath, destination);
    int result = do_sync_push(srcpath, destination, 0, 0);

    if(result < 0) {
        fprintf(stderr, "error: %s\n", sdb_error());
        return -1;
    }

    const char* SHELL_INSTALL_CMD ="shell:/usr/bin/pkgcmd -i -t %s -p %s -q";
    char full_cmd[PATH_MAX];

    if(tpk == 1) {
        snprintf(full_cmd, sizeof full_cmd, SHELL_INSTALL_CMD, "tpk", destination);
    }
    else if(tpk == 0){
        snprintf(full_cmd, sizeof full_cmd, SHELL_INSTALL_CMD, "wgt", destination);
    }
    else {
        return tpk;
    }
    D("Install command: %s\n", full_cmd);
    result = sdb_command2(full_cmd);

    if(result < 0) {
        fprintf(stderr, "error: %s\n", sdb_error());
        return result;
    }

    const char* SHELL_REMOVE_CMD = "shell:rm %s";
    snprintf(full_cmd, sizeof full_cmd, SHELL_REMOVE_CMD, destination);
    D("Remove file command: %s\n", full_cmd);
    result = sdb_command2(full_cmd);

    if(result < 0) {
        fprintf(stderr, "error: %s\n", sdb_error());
        return result;
    }

    return 0;
}

int uninstall_app_sdb(const char *appid) {
    const char* SHELL_UNINSTALL_CMD ="shell:/usr/bin/pkgcmd -u -t %s -n %s -q";
    char full_cmd[PATH_MAX];
    int result = 0;
    int tpk = get_pkgtype_from_app_id(appid);
    if(tpk == 1) {
        snprintf(full_cmd, sizeof full_cmd, SHELL_UNINSTALL_CMD, "tpk", appid);
    }
    else if(tpk == 0){
        snprintf(full_cmd, sizeof full_cmd, SHELL_UNINSTALL_CMD, "wgt", appid);
    }
    else {
        return tpk;
    }
    result = sdb_command2(full_cmd);

    if(result < 0) {
        fprintf(stderr, "error: %s\n", sdb_error());
        return result;
    }

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

// Returns 0 if pkg type is wgt. Returns 1 if pkg type is tpk. Returns minus if exception happens.
int get_pkgtype_file_name(const char* file_name) {

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
int get_pkgtype_from_app_id(const char* app_id) {

    char* GET_PKG_TYPE_CMD = "shell:/usr/bin/pkgcmd -l | grep %s | awk '{print $2}'";
    char full_cmd[PATH_MAX];
    snprintf(full_cmd, sizeof full_cmd, GET_PKG_TYPE_CMD, app_id);

    int result = sdb_connect(full_cmd);
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

static int check_file(const char* filename)
{
    struct stat st;

    if (filename == NULL) {
        return 0;
    }

    if (stat(filename, &st) != 0) {
        fprintf(stderr, "can't find '%s' to install\n", filename);
        return 1;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "can't install '%s' because it's not a file\n", filename);
        return 1;
    }

    return 0;
}

int install_app(transport_type transport, char* serial, int argc, char** argv)
{
    static const char *const DATA_DEST = "/data/local/tmp/%s";
    static const char *const SD_DEST = "/sdcard/tmp/%s";
    const char* where = DATA_DEST;
    char apk_dest[PATH_MAX];
    char verification_dest[PATH_MAX];
    char* apk_file;
    char* verification_file = NULL;
    int file_arg = -1;
    int err;
    int i;
    int verify_apk = 1;

    for (i = 1; i < argc; i++) {
        if (*argv[i] != '-') {
            file_arg = i;
            break;
        } else if (!strcmp(argv[i], "-i")) {
            // Skip the installer package name.
            i++;
        } else if (!strcmp(argv[i], "-s")) {
            where = SD_DEST;
        } else if (!strcmp(argv[i], "--algo")) {
            verify_apk = 0;
            i++;
        } else if (!strcmp(argv[i], "--iv")) {
            verify_apk = 0;
            i++;
        } else if (!strcmp(argv[i], "--key")) {
            verify_apk = 0;
            i++;
        }
    }

    if (file_arg < 0) {
        fprintf(stderr, "can't find filename in arguments\n");
        return 1;
    } else if (file_arg + 2 < argc) {
        fprintf(stderr, "too many files specified; only takes APK file and verifier file\n");
        return 1;
    }

    apk_file = argv[file_arg];

    if (file_arg != argc - 1) {
        verification_file = argv[file_arg + 1];
    }

    if (check_file(apk_file) || check_file(verification_file)) {
        return 1;
    }

    snprintf(apk_dest, sizeof apk_dest, where, get_basename(apk_file));
    if (verification_file != NULL) {
        snprintf(verification_dest, sizeof(verification_dest), where, get_basename(verification_file));

        if (!strcmp(apk_dest, verification_dest)) {
            fprintf(stderr, "APK and verification file can't have the same name\n");
            return 1;
        }
    }

    err = do_sync_push(apk_file, apk_dest, verify_apk, 0);
    if (err) {
        goto cleanup_apk;
    } else {
        argv[file_arg] = apk_dest; /* destination name, not source location */
    }

    if (verification_file != NULL) {
        err = do_sync_push(verification_file, verification_dest, 0 /* no verify APK */, 0);
        if (err) {
            goto cleanup_apk;
        } else {
            argv[file_arg + 1] = verification_dest; /* destination name, not source location */
        }
    }

    pm_command(transport, serial, argc, argv);

cleanup_apk:
    if (verification_file != NULL) {
        delete_file(transport, serial, verification_dest);
    }

    delete_file(transport, serial, apk_dest);

    return err;
}

int launch_app(transport_type transport, char* serial, int argc, char** argv)
{
    static const char *const SHELL_LAUNCH_CMD = "shell:/usr/bin/sdk_launch_app ";
    char full_cmd[PATH_MAX];
    int i;
    int result = 0;

    snprintf(full_cmd, sizeof full_cmd, "%s", SHELL_LAUNCH_CMD);

    //TODO: check argument validation

    for (i=1 ; i<argc ; i++) {
        strncat(full_cmd, " ", sizeof(full_cmd)-strlen(" ")-1);
        strncat(full_cmd, argv[i], sizeof(full_cmd)-strlen(argv[i])-1);
    }

    D("launch command: %s\n", full_cmd);
    result = sdb_command2(full_cmd);

    if(result < 0) {
        fprintf(stderr, "error: %s\n", sdb_error());
        return result;
    }

    if(result < 0) {
        fprintf(stderr, "error: %s\n", sdb_error());
        return result;
    }
    sdb_close(result);
    return 0;

}

void version_sdbd(transport_type ttype, char* serial) {
    char* VERSION_QUERY ="shell:rpm -qa | grep sdbd";
    send_shellcommand(ttype, serial, VERSION_QUERY);
}
