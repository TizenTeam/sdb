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

#ifndef _SDB_BACKEND_UTILS_H
#define _SDB_BACKEND_UTILS_H

#if defined(OS_WINDOWS)

typedef const struct FHClassRec_*   FHClass;

typedef struct FHRec_*          FH;

typedef struct EventHookRec_*  EventHook;

typedef struct FHClassRec_
{
    void (*_fh_init) ( FH  f );
    int  (*_fh_close)( FH  f );
    int  (*_fh_lseek)( FH  f, int  pos, int  origin );
    int  (*_fh_read) ( FH  f, void*  buf, int  len );
    int  (*_fh_write)( FH  f, const void*  buf, int  len );
    void (*_fh_hook) ( FH  f, int  events, EventHook  hook );

} FHClassRec;

/* used to emulate unix-domain socket pairs */
typedef struct SocketPairRec_*  SocketPair;

typedef struct FHRec_
{
    FHClass    clazz;
    int        used;
    int        eof;
    union {
        HANDLE      handle;
        SOCKET      socket;
        SocketPair  pair;
    } u;

    HANDLE    event;
    int       mask;

    char  name[32];

} FHRec;

#define  fh_handle  u.handle
#define  fh_socket  u.socket
#define  fh_pair    u.pair

#define  BIP_BUFFER_SIZE   4096
#define  WIN32_FH_BASE    100
#define  WIN32_MAX_FHS    128

typedef struct BipBufferRec_
{
    int                a_start;
    int                a_end;
    int                b_end;
    int                fdin;
    int                fdout;
    int                closed;
    int                can_write;  /* boolean */
    HANDLE             evt_write;  /* event signaled when one can write to a buffer  */
    int                can_read;   /* boolean */
    HANDLE             evt_read;   /* event signaled when one can read from a buffer */
    CRITICAL_SECTION  lock;
    unsigned char      buff[ BIP_BUFFER_SIZE ];

} BipBufferRec, *BipBuffer;

typedef struct EventLooperRec_*  EventLooper;

typedef struct EventHookRec_
{
    EventHook    next;
    FH           fh;
    HANDLE       h;
    int          wanted;   /* wanted event flags */
    int          ready;    /* ready event flags  */
    void*        aux;
    void        (*prepare)( EventHook  hook );
    int         (*start)  ( EventHook  hook );
    void        (*stop)   ( EventHook  hook );
    int         (*check)  ( EventHook  hook );
    int         (*peek)   ( EventHook  hook );
} EventHookRec;

#define  MAX_LOOPER_HANDLES  WIN32_MAX_FHS

typedef struct EventLooperRec_
{
    EventHook    hooks;
    HANDLE       htab[ MAX_LOOPER_HANDLES ];
    int          htab_count;

} EventLooperRec;

int _fh_to_int( FH  f );
FH _fh_from_int( int   fd );

#else
#endif // end of unix

struct utils_os_backend {
    // human-readable name
    const char *name;

    int (*launch_server)(int server_port);
    void (*start_logging)(void);
    char* (*ansi_to_utf8)(const char *str);
    int (*sdb_open)(const char* path, int options);
    int (*sdb_open_mode)(const char* path, int options, int mode);
    int (*unix_open)(const char* path, int options, ...);
    int (*sdb_creat)(const char* path, int mode);
    int (*sdb_read)(int fd, void* buf, size_t len);
    int (*sdb_write)(int fd, const void* buf, size_t len);
    int (*sdb_lseek)(int fd, int pos, int where);
    int (*sdb_shutdown)(int fd);
    int (*sdb_close)(int fd);
    int (*sdb_unlink)(const char* path);
    int (*sdb_mkdir)(const char* path, int mode);
    void (*close_on_exec)(int fd);
    int (*sdb_socket_accept)(int serverfd, struct sockaddr* addr, socklen_t *addrlen);
    int (*sdb_socketpair)(int sv[2]);
    void (*sdb_sleep_ms)(int mseconds);
    char* (*sdb_dirstart)(const char* path);
    char* (*sdb_dirstop)(const char* path);
    int (*sdb_socket_setbufsize)(int fd, int bufsize);
    void (*disable_tcp_nagle)(int fd);
    // simple implementation of pthread
    int (*sdb_thread_create)(sdb_thread_t *pthread, sdb_thread_func_t start, void* arg);
    int (*sdb_mutex_lock)(sdb_mutex_t *mutex);
    int (*sdb_mutex_unlock)(sdb_mutex_t *mutex);

    /*
     * Copyright (C) 2009 Andrzej K. Haczewski <ahaczewski@gmail.com>
     * Implement simple condition variable for Windows threads, based on ACE
     * See original implementation: http://github.com/git
     */
    int (*sdb_cond_init)(sdb_cond_t *cond, const void *unused);
    int (*sdb_cond_wait)(sdb_cond_t *cond, sdb_mutex_t *mutex);
    int (*sdb_cond_broadcast)(sdb_cond_t *cond);
    void (*sdb_sysdeps_init)(void);
    // helpers for sockets
    int (*socket_loopback_client)(int port, int type);
    int (*socket_network_client)(const char *host, int port, int type);
    int (*socket_loopback_server)(int port, int type);
    int (*socket_inaddr_any_server)(int port, int type);

};

extern const struct utils_os_backend* utils_backend;
extern const struct utils_os_backend utils_unix_backend;
extern const struct utils_os_backend utils_windows_backend;

#endif /* _SDB_BACKEND_UTILS_H */
