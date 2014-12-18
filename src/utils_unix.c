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
#include "sdb_messages.h"

static int _launch_server(void)
{
    pid_t   pid;

    switch (pid = fork()) {
        case -1:
            LOG_ERROR("failed to fork process: %s\n", strerror(errno));
            return -1;
            // never reached!
        case 0: {
            char path[128] = {0,};

#if defined(OS_DARWIN)
            ProcessSerialNumber psn;
            GetCurrentProcess(&psn);
            CFDictionaryRef dict;
            dict = ProcessInformationCopyDictionary(&psn, kProcessDictionaryIncludeAllInformationMask); //0xffffffff
            CFStringRef buf = (CFStringRef)CFDictionaryGetValue(dict, CFSTR("CFBundleExecutable"));
            CFStringGetCString(buf, path, sizeof(path), kCFStringEncodingUTF8);

#elif defined(OS_LINUX)
            ssize_t len = 0;
            char buf[128] = {0,};
            snprintf(buf, sizeof(buf), "/proc/%d/exe", getpid());
            if ((len = readlink(buf, path, sizeof(path) - 1)) != -1) {
                path[len] = '\0';
            }
#endif
            // before fork exec, be session leader.
            setsid();

            execl(path, "sdb", "fork-server", "server", NULL);
            LOG_ERROR("failed to execute process: %s\n", strerror(errno));
            _exit(-1);
        }
        default:
            // wait for sec due to for booting up sdb server
            sdb_sleep_ms(3000);
            break;
    }

    return 0;
}


static void _start_logging(void)
{
    const char*  p = getenv(DEBUG_ENV);
    if (p == NULL && !getenv(TRACE_PACKET)) {
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
        print_error(SDB_MESSAGE_ERROR ,ERR_GENERAL_LOG_FAIL, F(ERR_SYNC_OPEN_FAIL, "/tmp/sdb/log"));
        fd = unix_open("/dev/null", O_WRONLY);
        if( fd < 0 ) {
            print_error(SDB_MESSAGE_ERROR ,ERR_GENERAL_LOG_FAIL, F(ERR_SYNC_OPEN_FAIL, "/dev/null"));
            LOG_DEBUG("--- sdb starting (pid %d) ---\n", getpid());
            return;
        }
    }

    dup2(fd, 1);
    dup2(fd, 2);
    sdb_close(fd);

    LOG_DEBUG("--- sdb starting (pid %d) ---\n", getpid());
    return;

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
    int ret = fcntl( fd, F_SETFD, FD_CLOEXEC );
    if (ret == -1)
        LOG_ERROR("failed to set the file descriptor '%d': %s",fd ,strerror(errno));
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
    int ret = setsockopt( fd, IPPROTO_TCP, TCP_NODELAY, (void*)&on, sizeof(on) );
    if (ret == -1)
        LOG_ERROR("failed to set the file descriptor '%d': %s\n", fd, strerror(errno));
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

    int connect_timeout = 1;

    if(connect_nonb(s, (struct sockaddr *) &addr, sizeof(addr), connect_timeout) < 0) {
            sdb_close(s);
            return -1;
    }
    return s;

}

int connect_nonb(int sockfd, const struct sockaddr *saptr, socklen_t salen, int nsec)
{
    int flags, n, error;
    socklen_t len;
    fd_set rset, wset;
    struct timeval tval;

    flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    error = 0;
    if ((n = connect(sockfd, (struct sockaddr *) saptr, salen)) < 0)
        if (errno != EINPROGRESS)
            return (-1);

    /* Do whatever we want while the connect is taking place. */

    if (n == 0)
        goto done;
    /* connect completed immediately */

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    wset = rset;
    tval.tv_sec = nsec;
    tval.tv_usec = 0;
    if ((n = select(sockfd + 1, &rset, &wset, NULL, nsec ? &tval : NULL))
            == 0) {
        sdb_close(sockfd); /* timeout */
        errno = ETIMEDOUT;
        return (-1);
    }
    if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
        len = sizeof(error);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
            return (-1); /* Solaris pending error */
    } else
        D("select error: sockfd not set\n");

    done: fcntl(sockfd, F_SETFL, flags); /* restore file status flags */

    if (error) {
        sdb_close(sockfd); /* just in case */
        errno = error;
        return (-1);
    }
    return (0);
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
    int ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));
    if (ret == -1)
        LOG_ERROR("failed to set the file descriptor '%d': %s\n", s, strerror(errno));

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
    .sdb_transport_close = _sdb_close,
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
