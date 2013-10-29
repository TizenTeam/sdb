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

#include "linkedlist.h"
#include "sdb_map.h"


#if defined(OS_WINDOWS)

struct sdb_handle {
	union {
		HANDLE      file_handle;
		SOCKET      socket;
	} u;
	int fd;
	int is_socket;
};

typedef struct sdb_handle SDB_HANDLE;

struct sdb_socket_handle {
	SDB_HANDLE handle;
	int event_location;
};

typedef struct sdb_socket_handle SDB_SOCK_HANDLE;

#define  BIP_BUFFER_SIZE   4096
#define  MAX_LOOPER_HANDLES  WIN32_MAX_FHS
#define IS_SOCKET_HANDLE(handle) (handle->is_socket == 1)

SDB_HANDLE* sdb_handle_map_get(int _key);
void sdb_handle_map_put(int _key, SDB_HANDLE* value);
void sdb_handle_map_remove(int _key);

#else
#endif // end of unix

struct utils_os_backend {
    // human-readable name
    const char *name;

    int (*launch_server)(void);
    void (*start_logging)(void);
    char* (*ansi_to_utf8)(const char *str);
    int (*sdb_open)(const char* path, int options);
    int (*sdb_creat)(const char* path, int mode);
    int (*sdb_read)(int fd, void* buf, size_t len);
    int (*sdb_write)(int fd, const void* buf, size_t len);
    int (*sdb_shutdown)(int fd);
    int (*sdb_transport_close)(int fd);
    int (*sdb_close)(int fd);
    int (*sdb_mkdir)(const char* path, int mode);
    void (*close_on_exec)(int fd);
    int (*sdb_socket_accept)(int serverfd);
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
    int (*sdb_host_connect)(const char *host, int port, int type);
    int (*sdb_port_listen)(uint32_t inet, int port, int type);

};

extern const struct utils_os_backend* utils_backend;
extern const struct utils_os_backend utils_unix_backend;
extern const struct utils_os_backend utils_windows_backend;

#endif /* _SDB_BACKEND_UTILS_H */
