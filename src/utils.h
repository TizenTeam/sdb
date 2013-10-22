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

#ifndef _SDB_UTILS_H
#define _SDB_UTILS_H

// TODO: move to c file
#if defined(OS_WINDOWS)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include "pthread_win32.h"

#else
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#endif

//append strings with white space at most n+1 character. (last char is \0)
void append_args(char *dest, int argc, const char** argv, size_t n);
//get file name from full path. For example, /usr/opt/app -> app, app -> app
char* get_filename(char* full_name);
//append separator to the end of path.
//If return value is 1, it allocates new memory and appends separator at the end of path. Therefore, users should free memory.
void append_separator(char* result_path, char* path);
//returns 1 if path is directory, returns 0 if path is file, returns -1 if error occurred.
int is_directory(char* path);
int mkdirs(char *name);
void append_file(char* result_path, char* dir, char* append_dir);
long long NOW();
void dup_quote(char* result_string, const char *source, int max_len);

// OS dependent helpers

#if defined(OS_WINDOWS)
#define OS_PATH_SEPARATOR '\\'
#define OS_PATH_SEPARATOR_STR "\\"

#define  lstat    stat   /* no symlinks on Win32 */
#define  S_ISLNK(m)   0   /* no symlinks on Win32 */

extern int unix_read(int fd, void* buf, size_t len);

#else
#define OS_PATH_SEPARATOR '/'
#define OS_PATH_SEPARATOR_STR "/"

#define  unix_read   sdb_read
#define  SDB_MUTEX_DEFINE(m)      sdb_mutex_t   m = PTHREAD_MUTEX_INITIALIZER
#endif
// thread helpers
typedef  pthread_t                sdb_thread_t;
typedef  pthread_mutex_t          sdb_mutex_t;
#define  sdb_cond_t               pthread_cond_t
#define  sdb_mutex_init           pthread_mutex_init
#define  sdb_cond_signal          pthread_cond_signal
#define  sdb_cond_destroy         pthread_cond_destroy
#define  sdb_mutex_destroy        pthread_mutex_destroy

typedef void*  (*sdb_thread_func_t)( void*  arg );

void sdb_sysdeps_init(void);
int sdb_thread_create(sdb_thread_t *pthread, sdb_thread_func_t start, void* arg);
int sdb_mutex_lock(sdb_mutex_t *mutex, char* lock_name);
int sdb_mutex_unlock(sdb_mutex_t *mutex, char* lock_name);
int sdb_cond_init(sdb_cond_t *cond, const void *unused);
int sdb_cond_wait(sdb_cond_t *cond, sdb_mutex_t *mutex);
int sdb_cond_broadcast(sdb_cond_t *cond);

#define  SDB_MUTEX(x)   sdb_mutex_t  x;

SDB_MUTEX(dns_lock)
SDB_MUTEX(transport_lock)
SDB_MUTEX(wakeup_select_lock)
SDB_MUTEX(usb_lock)
SDB_MUTEX(D_lock)

int launch_server();
void start_logging(void);
char* ansi_to_utf8(const char *str);
int sdb_open(const char* path, int options);
int sdb_open_mode(const char* path, int options, int mode);
int unix_open(const char* path, int options, ...);
int sdb_creat(const char* path, int mode);
int sdb_read(int fd, void* buf, size_t len);
int sdb_write(int fd, const void* buf, size_t len);
int sdb_shutdown(int fd);
int sdb_close(int fd);
int unix_unlink(const char* path);
int sdb_mkdir(const char* path, int mode);
void close_on_exec(int fd);
int sdb_socket_accept(int serverfd);
int sdb_socketpair(int sv[2]);
void sdb_sleep_ms(int mseconds);
char* sdb_dirstart(const char* path);
char* sdb_dirstop(const char* path);
int sdb_socket_setbufsize(int fd, int bufsize);
void disable_tcp_nagle(int fd);

// sockets
#define LISTEN_BACKLOG 4

int sdb_host_connect(const char *host, int port, int type);
int sdb_port_listen(uint32_t inet, int port, int type);

#define DEVICEMAP_SEPARATOR ":"
#define DEVICENAME_MAX 256
#define VMS_PATH OS_PATH_SEPARATOR_STR "vms" OS_PATH_SEPARATOR_STR
#define DEFAULT_DEVICENAME "<unknown>"

#endif /* _SDB_UTILS_H */
