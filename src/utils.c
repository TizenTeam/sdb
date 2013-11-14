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
#include "log.h"

#include "fdevent.h"
#include "sdb_constants.h"

#define   TRACE_TAG  TRACE_SDB

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
    for(i = 0; i < argc; i++) {
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
int launch_server(void) {
    return utils_backend->launch_server();
}

void start_logging(void) {
    return utils_backend->start_logging();
}

char* ansi_to_utf8(const char *str) {
    return utils_backend->ansi_to_utf8(str);
}

int sdb_open(const char* path, int options) {
	LOG_INFO("path %s, options %d\n", path, options);
    return utils_backend->sdb_open(path, options);
}

int unix_open(const char* path, int options, ...) {
    int mode;
    va_list args;

    va_start( args, options);
    mode = va_arg( args, int );
    va_end( args);

    return open(path, options, mode);
}

int sdb_creat(const char* path, int mode) {
	LOG_INFO("path %s mode %d\n", path, mode);
    return utils_backend->sdb_creat(path, mode);
}

int sdb_read(int fd, void* buf, size_t len) {
    return utils_backend->sdb_read(fd, buf, len);
}

int sdb_write(int fd, const void* buf, size_t len) {
    return utils_backend->sdb_write(fd, buf, len);
}

int sdb_shutdown(int fd) {
    return utils_backend->sdb_shutdown(fd);
}

//In Windows, just close the socket and not free the SDB_HANDLE because it may be used in transport_thread
int sdb_transport_close(int fd) {
    return utils_backend->sdb_transport_close(fd);
}

int sdb_close(int fd) {
    return utils_backend->sdb_close(fd);
}

int unix_unlink(const char* path) {
    int rc = unlink(path);

    if (rc == -1 && errno == EACCES) {
        rc = chmod(path, S_IREAD | S_IWRITE);
        if (rc == 0) {
            rc = unlink(path);
        }
    }
    return rc;
}

int sdb_mkdir(const char* path, int mode) {
    return utils_backend->sdb_mkdir(path, mode);
}

void close_on_exec(int fd) {
    return utils_backend->close_on_exec(fd);
}

int sdb_socket_accept(int serverfd) {
	LOG_INFO("FD(%d)\n");
    return utils_backend->sdb_socket_accept(serverfd);
}

int sdb_socketpair(int sv[2]) {
	LOG_INFO("\n");
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

int sdb_mutex_lock(sdb_mutex_t *mutex, char* name) {
    if(name != NULL) {
        D("lock %s\n", name);
    }
    return utils_backend->sdb_mutex_lock(mutex);
}

int sdb_mutex_unlock(sdb_mutex_t *mutex, char *name) {
    if(name != NULL) {
        D("unlock %s\n", name);
    }
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

int sdb_host_connect(const char *host, int port, int type) {
	LOG_INFO("host %s, port %d\n", host, port);
    return utils_backend->sdb_host_connect(host, port, type);
}

int sdb_port_listen(uint32_t inet, int port, int type) {
	LOG_INFO("port %d, type %d\n", port, type);
    return utils_backend->sdb_port_listen(inet, port, type);
}

