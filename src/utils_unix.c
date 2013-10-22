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

#if defined(OS_DARWIN)
#import <Carbon/Carbon.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>

#include "utils.h"
#include "fdevent.h"

#define  TRACE_TAG  TRACE_SYSDEPS
#include "utils_backend.h"
#include "log.h"

#if defined(OS_DARWIN)

void _get_sdb_path(char *s, size_t max_len)
{
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
    CFDictionaryRef dict;
    dict = ProcessInformationCopyDictionary(&psn, 0xffffffff);
    CFStringRef value = (CFStringRef)CFDictionaryGetValue(dict, CFSTR("CFBundleExecutable"));
    CFStringGetCString(value, s, max_len, kCFStringEncodingUTF8);
}

#elif defined(OS_LINUX)

static void _get_sdb_path(char *exe, size_t max_len)
{
    char proc[64];
    snprintf(proc, sizeof proc, "/proc/%d/exe", getpid());
    int err = readlink(proc, exe, max_len - 1);
    if(err > 0) {
        exe[err] = '\0';
    } else {
        exe[0] = '\0';
    }
}
#endif

static int _launch_server(int server_port)
{
    char    path[PATH_MAX];
    int     fd[2];

    // set up a pipe so the child can tell us when it is ready.
    // fd[0] will be parent's end, and fd[1] will get mapped to stderr in the child.
    if (pipe(fd)) {
        fprintf(stderr, "pipe failed in launch_server, errno: %d\n", errno);
        return -1;
    }
    _get_sdb_path(path, PATH_MAX);
    pid_t pid = fork();
    if(pid < 0) return -1;

    if (pid == 0) {
        // child side of the fork

        // redirect stderr to the pipe
        // we use stderr instead of stdout due to stdout's buffering behavior.
        sdb_close(fd[0]);
        dup2(fd[1], STDERR_FILENO);
        sdb_close(fd[1]);

        sdb_close(STDOUT_FILENO);

        // child process
        int result = execl(path, "sdb", "fork-server", "server", NULL);
        // this should not return
        fprintf(stderr, "OOPS! execl returned %d, errno: %d\n", result, errno);
    } else  {
        // parent side of the fork

        char  temp[3];

        temp[0] = 'A'; temp[1] = 'B'; temp[2] = 'C';
        // wait for the "OK\n" message
        sdb_close(fd[1]);
        int ret = sdb_read(fd[0], temp, 3);
        int saved_errno = errno;
        sdb_close(fd[0]);

        if (ret < 0) {
            fprintf(stderr, "could not read ok from SDB Server, errno = %d\n", saved_errno);
            return -1;
        }
        if (ret != 3 || temp[0] != 'O' || temp[1] != 'K' || temp[2] != '\n') {
            fprintf(stderr, "SDB server didn't ACK\n" );
            return -1;
        }

        setsid();
    }
    return 0;
}


static void _start_logging(void)
{
    const char*  p = getenv(DEBUG_ENV);
    if (p == NULL) {
        return;
    }
    int fd;

    fd = unix_open("/dev/null", O_RDONLY);
    if (fd >= 0) {
        dup2(fd, 0);
        sdb_close(fd);
    }
    else {
        sdb_close(0);
    }

    fd = unix_open("/tmp/sdb.log", O_WRONLY | O_CREAT | O_APPEND, 0640);
    if(fd < 0) {
        fprintf(stderr, "fail to open '/tmp/sdb.log' logging fails\n");
        fd = unix_open("/dev/null", O_WRONLY);
        if( fd < 0 ) {
            fprintf(stderr, "fail to open /dev/null\n");
            fprintf(stderr,"--- sdb starting (pid %d) ---\n", getpid());
            return;
        }
    }

    dup2(fd, 1);
    dup2(fd, 2);
    sdb_close(fd);

    fprintf(stderr,"--- sdb starting (pid %d) ---\n", getpid());
}

static char* _ansi_to_utf8(const char *str)
{
    // Not implement!
    // If need, use iconv later event though unix system is using utf8 encoding.
    int len;
    char *utf8;

    len = strlen(str);
    utf8 = (char *)calloc(len+1, sizeof(char));
    strcpy(utf8, str);
    return utf8;
}

static void  _close_on_exec(int  fd)
{
    fcntl( fd, F_SETFD, FD_CLOEXEC );
}

static int _sdb_open( const char*  pathname, int  options )
{
    int  fd = open( pathname, options );
    if (fd < 0)
        return -1;
    _close_on_exec( fd );
    return fd;
}

static int  _sdb_creat(const char*  path, int  mode)
{
    int  fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);

    if ( fd < 0 )
        return -1;

    _close_on_exec(fd);
    return fd;
}

static int  _sdb_read(int  fd, void*  buf, size_t  len)
{
    return read(fd, buf, len);
}

static int  _sdb_write(int  fd, const void*  buf, size_t  len)
{
    return write(fd, buf, len);
}

static int  _sdb_shutdown(int fd)
{
    return shutdown(fd, SHUT_RDWR);
}

static int  _sdb_close(int fd)
{
    return close(fd);
}

static int  _sdb_mkdir(const char*  path, int mode)
{
    return mkdir(path, mode);
}

static int _sdb_socket_accept(int  serverfd)
{
    int fd;
    struct sockaddr addr;
    socklen_t alen = sizeof(addr);

    fd = accept(serverfd, &addr, &alen);

    if (fd >= 0) {
        _close_on_exec(fd);
    }

    return fd;
}

static int _unix_socketpair( int  d, int  type, int  protocol, int sv[2] )
{
    return socketpair( d, type, protocol, sv );
}

static int _sdb_socketpair( int  sv[2] )
{
    int  rc;

    rc = _unix_socketpair( AF_UNIX, SOCK_STREAM, 0, sv );
    if (rc < 0)
        return -1;

    _close_on_exec( sv[0] );
    _close_on_exec( sv[1] );
    return 0;
}

static void  _sdb_sleep_ms( int  mseconds )
{
    usleep( mseconds*1000 );
}

static char*  _sdb_dirstart(const char*  path)
{
    return strchr(path, '/');
}

static char*  _sdb_dirstop(const char*  path)
{
    return strrchr(path, '/');
}

static int _sdb_socket_setbufsize( int   fd, int  bufsize )
{
    int opt = bufsize;
    return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt));
}

static void _disable_tcp_nagle(int fd)
{
    int  on = 1;
    setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, (void*)&on, sizeof(on) );
}

static int  _sdb_thread_create( sdb_thread_t  *pthread, sdb_thread_func_t  start, void*  arg )
{
    pthread_attr_t   attr;

    pthread_attr_init (&attr);
    pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED); // don't need to call join

    return pthread_create( pthread, &attr, start, arg );
}

static int _sdb_mutex_lock(sdb_mutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

static int _sdb_mutex_unlock(sdb_mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

static int _sdb_cond_init(sdb_cond_t *cond, const void *unused) {
    return pthread_cond_init(cond, unused);
}

static int _sdb_cond_wait(sdb_cond_t *cond, sdb_mutex_t *mutex) {
    return pthread_cond_wait(cond, mutex);
}

static int _sdb_cond_broadcast(sdb_cond_t *cond) {
    return pthread_cond_broadcast(cond);
}

static void  _sdb_sysdeps_init(void)
{
}

static int _sdb_host_connect(const char *host, int port, int type)
{
    struct hostent *hp;
    struct sockaddr_in addr;
    int s;

    // FIXME: might take a long time to get information
    if ((hp = gethostbyname(host)) == NULL) {
        LOG_ERROR("failed to get hostname:%s(%d)\n", host, port);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = hp->h_addrtype;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);

    if (type == SOCK_STREAM) {
        s = socket(AF_INET, type, 0);
    } else {
        s = socket(AF_INET, type, IPPROTO_UDP);
    }
    if (s < 0) {
        LOG_ERROR("failed to create socket to %s(%d)\n", host, port);
        return -1;
    }

    if(connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        sdb_close(s);
        return -1;
    }

    return s;

}

static int _sdb_port_listen(uint32_t inet, int port, int type)
{
    struct sockaddr_in addr;
    int s, n;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(inet);
    addr.sin_port = htons(port);

    if ((s = socket(AF_INET, type, 0)) < 0) {
        LOG_ERROR("failed to create socket to %u(%d)\n", inet, port);
        return -1;
    }

    n = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));

    if(bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        sdb_close(s);
        return -1;
    }

    // only listen if tcp mode
    if (type == SOCK_STREAM) {
        if (listen(s, LISTEN_BACKLOG) < 0) {
            sdb_close(s);
            return -1;
        }
    }

    return s;
}


const struct utils_os_backend utils_unix_backend = {
    .name = "unix utils",
    .launch_server = _launch_server,
    .start_logging = _start_logging,
    .ansi_to_utf8 = _ansi_to_utf8,
    .sdb_open = _sdb_open,
    .sdb_creat = _sdb_creat,
    .sdb_read = _sdb_read,
    .sdb_write = _sdb_write,
    .sdb_shutdown = _sdb_shutdown,
    .sdb_close = _sdb_close,
    .close_on_exec = _close_on_exec,
    .sdb_mkdir = _sdb_mkdir,
    .sdb_socket_accept = _sdb_socket_accept,
    .sdb_socketpair = _sdb_socketpair,
    .sdb_sleep_ms = _sdb_sleep_ms,
    .sdb_dirstart = _sdb_dirstart,
    .sdb_dirstop = _sdb_dirstop,
    .sdb_socket_setbufsize = _sdb_socket_setbufsize,
    .disable_tcp_nagle = _disable_tcp_nagle,
    // simple pthread implementations
    .sdb_thread_create = _sdb_thread_create,
    .sdb_mutex_lock = _sdb_mutex_lock,
    .sdb_mutex_unlock = _sdb_mutex_unlock,
    .sdb_cond_init = _sdb_cond_init,
    .sdb_cond_wait = _sdb_cond_wait,
    .sdb_cond_broadcast = _sdb_cond_broadcast,
    .sdb_sysdeps_init = _sdb_sysdeps_init,
    .sdb_host_connect = _sdb_host_connect,
    .sdb_port_listen = _sdb_port_listen
};
