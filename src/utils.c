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
#include <string.h>
#include <limits.h>
#include <sys/time.h>

#include "utils.h"
#include "utils_backend.h"

#include "fdevent.h"
#include "sdb_constants.h"
#include "sdb.h"

#if defined(OS_WINDOWS)
const struct utils_os_backend* utils_backend = &utils_windows_backend;
#elif defined(OS_DARWIN)
const struct utils_os_backend* utils_backend = &utils_unix_backend;
#elif defined(OS_LINUX)
const struct utils_os_backend* utils_backend = &utils_unix_backend;
#else
#error "unsupported OS"
#endif

int is_directory(char* path) {
    struct stat st;

    if(stat(path, &st)) {
        fprintf(stderr,"cannot stat '%s': %s\n", path, strerror(errno));
        return -1;
    }

    if(S_ISDIR(st.st_mode)) {
        return 1;
    }

    return 0;
}

int mkdirs(char *name)
{
    int ret;
    char *x = name + 1;

    for(;;) {
        x = sdb_dirstart(x);
        if(x == 0) return 1;
        *x = 0;
        ret = sdb_mkdir(name, 0775);
        if(ret < 0 && (errno != EEXIST || is_directory(name) != 1)) {
            *x = OS_PATH_SEPARATOR;
            return ret;
        }
        *x = OS_PATH_SEPARATOR;
        x++;
    }
    return 1;
}

void append_separator(char* result_path, char* path) {

    int path_len = strlen(path);
    if(path_len > 0 && (path)[path_len -1] != '/') {
        snprintf(result_path, PATH_MAX, "%s/",path);
    }
    else {
        snprintf(result_path, PATH_MAX, "%s",path);
    }
}

void append_file(char* result_path, char* dir, char* append_dir) {
    char* tmp_append;

    int len = strlen(append_dir);
    if(len > 0) {
        if(append_dir[0] == '/') {
            tmp_append = append_dir + 1;
        }
        else {
            tmp_append = append_dir;
        }
    }
    else {
        tmp_append = (char*)EMPTY_STRING;
    }

    int dir_len = strlen(dir);
    if(dir_len > 0 && dir[dir_len -1] != '/') {
        snprintf(result_path, PATH_MAX, "%s/%s",dir, tmp_append);
    }
    else {
        snprintf(result_path, PATH_MAX, "%s%s",dir, tmp_append);
    }
}

char* get_filename(char* full_name) {
    char *name = sdb_dirstop(full_name);
    if(name == 0) {
        name = full_name;
    } else {
        name++;
    }

    return name;
}

void append_args(char *dest, int argc, const char** argv, size_t n) {
    int full_cmd_len = strlen(dest);
    int i;
    for(i = 1; i < argc; i++) {
        int arglen = strlen(argv[i]);
        int size_checker = n - full_cmd_len - arglen - 1;
        if(size_checker <0) {
            break;
        }

        strncat(dest, " ", n - full_cmd_len);
        ++full_cmd_len;
        strncat(dest, argv[i], n - full_cmd_len);
        full_cmd_len += arglen;
    }
}

long long NOW()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((long long) tv.tv_usec) +
        1000000LL * ((long long) tv.tv_sec);
}

/** duplicate string and quote all \ " ( ) chars + space character. */
void dup_quote(char* result_string, const char *source, int max_len)
{
    int i = 0;
    for (; i < max_len-1; i++) {
        if(*source == '\0') {
            break;
        }
        char* quoted = (char*)QUOTE_CHAR;
        while(*quoted != '\0') {
            if(*source == *quoted++) {
                i++;
                *result_string++ = '\\';
                break;
            }
        }
        *result_string++ = *source;
        source++;
    }

    if( i != max_len) {
        *result_string = '\0';
    }
    else {
        *(--result_string) = '\0';
    }
}

/**************************************************/
/***           OS dependent helpers             ***/
/**************************************************/
int launch_server(int server_port) {
    return utils_backend->launch_server(server_port);
}

void start_logging(void) {
    return utils_backend->start_logging();
}

char* ansi_to_utf8(const char *str) {
    return utils_backend->ansi_to_utf8(str);
}

int sdb_open(const char* path, int options) {
    return utils_backend->sdb_open(path, options);
}

int sdb_open_mode(const char* pathname, int options, int mode) {
    return utils_backend->sdb_open_mode(pathname, options, mode);
}

int unix_open(const char* path, int options, ...) {
    return utils_backend->unix_open(path, options);
}

int sdb_creat(const char* path, int mode) {
    return utils_backend->sdb_creat(path, mode);
}

int sdb_read(int fd, void* buf, size_t len) {
    return utils_backend->sdb_read(fd, buf, len);
}

int sdb_write(int fd, const void* buf, size_t len) {
    return utils_backend->sdb_write(fd, buf, len);
}

int sdb_lseek(int fd, int pos, int where) {
    return utils_backend->sdb_lseek(fd, pos, where);
}

int sdb_shutdown(int fd) {
    return utils_backend->sdb_shutdown(fd);
}

int sdb_close(int fd) {
    return utils_backend->sdb_close(fd);
}

int sdb_unlink(const char* path) {
    return utils_backend->sdb_unlink(path);
}

int sdb_mkdir(const char* path, int mode) {
    return utils_backend->sdb_mkdir(path, mode);
}

void close_on_exec(int fd) {
    return utils_backend->close_on_exec(fd);
}

int sdb_socket_accept(int serverfd, struct sockaddr* addr, socklen_t *addrlen) {
    return utils_backend->sdb_socket_accept(serverfd, addr, addrlen);
}

int sdb_socketpair(int sv[2]) {
    return utils_backend->sdb_socketpair(sv);
}

void sdb_sleep_ms(int mseconds) {
    return utils_backend->sdb_sleep_ms(mseconds);
}

char* sdb_dirstart(const char* path) {
    return utils_backend->sdb_dirstart(path);
}

char* sdb_dirstop(const char* path) {
    return utils_backend->sdb_dirstop(path);
}

int sdb_socket_setbufsize(int fd, int bufsize) {
    return utils_backend->sdb_socket_setbufsize(fd, bufsize);
}

void disable_tcp_nagle(int fd) {
    return utils_backend->disable_tcp_nagle(fd);
}

int sdb_thread_create(sdb_thread_t *pthread, sdb_thread_func_t start, void* arg) {
    return utils_backend->sdb_thread_create(pthread, start, arg);
}

int sdb_mutex_lock(sdb_mutex_t *mutex) {
    return utils_backend->sdb_mutex_lock(mutex);
}

int sdb_mutex_unlock(sdb_mutex_t *mutex) {
    return utils_backend->sdb_mutex_unlock(mutex);
}

int sdb_cond_init(sdb_cond_t *cond, const void *unused) {
    return utils_backend->sdb_cond_init(cond, unused);
}

int sdb_cond_wait(sdb_cond_t *cond, sdb_mutex_t *mutex) {
    return utils_backend->sdb_cond_wait(cond, mutex);
}

int sdb_cond_broadcast(sdb_cond_t *cond) {
    return utils_backend->sdb_cond_broadcast(cond);
}

void sdb_sysdeps_init(void) {
    return utils_backend->sdb_sysdeps_init();
}

int socket_loopback_client(int port, int type) {
    return utils_backend->socket_loopback_client(port, type);
}

int socket_network_client(const char *host, int port, int type) {
    return utils_backend->socket_network_client(host, port, type);
}

int socket_loopback_server(int port, int type) {
    return utils_backend->socket_loopback_server(port, type);
}

int socket_inaddr_any_server(int port, int type) {
    return utils_backend->socket_inaddr_any_server(port, type);
}
