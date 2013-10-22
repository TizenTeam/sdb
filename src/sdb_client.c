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
#include <limits.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fdevent.h"
#include "sdb_constants.h"
#include "utils.h"

#define  TRACE_TAG  TRACE_SDB
#include "sdb_client.h"
#include "log.h"

static int switch_socket_transport(int fd, void** extra_args);
static int __inline__ write_msg_size(int fd, int size, int host_fd);

void sendokmsg(int fd, const char *msg)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "OKAY%04x%s", (unsigned)strlen(msg), msg);
    writex(fd, buf, strlen(buf));
}

void sendfailmsg(int fd, const char *reason)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), "FAIL%04x%s", (unsigned)strlen(reason), reason);
    writex(fd, buf, strlen(buf));
}

int send_service_with_length(int fd, const char* service, int host_fd) {

    int len;
    len = strlen(service);

    if(len < 1) {
        if(host_fd == 0) {
            fprintf(stderr,"error: service name is empty\n");
        }
        else {
            sendfailmsg(host_fd, "error: service name is empty\n");
        }
        return -1;
    }
    else if (len > 1024) {
        if(host_fd == 0) {
            fprintf(stderr,"error: service name too long\n");
        }
        else {
            sendfailmsg(host_fd, "error: service name too long\n");
        }
        return -1;
    }

    if(write_msg_size(fd, len, host_fd) < 0) {
        D("fail to write msg size\n");
        if(host_fd != 0) {
            sendfailmsg(host_fd, "fail to write msg size\n");
        }
        return -1;
    }

    if(writex(fd, service, len)) {
        D("error: write failure during connection\n");
        if(host_fd == 0) {
            fprintf(stderr,"error: write failure during connection\n");
        }
        else {
            sendfailmsg(host_fd, "error: write failure during connection\n");
        }
        return -1;
    }

    return 0;
}

static int switch_socket_transport(int fd, void** extra_args)
{
    char* serial = (char *)extra_args[0];
    transport_type ttype = *(transport_type*)extra_args[1];

    char service[64];

    get_host_prefix(service, sizeof service, ttype, serial, transport);

    if(send_service_with_length(fd, service, 0) < 0) {
        sdb_close(fd);
        return -1;
    }

    D("Switch transport in progress\n");

    if(sdb_status(fd, 0)) {
        sdb_close(fd);
        D("Switch transport failed\n");
        return -1;
    }
    D("Switch transport success\n");
    return 0;
}

int sdk_launch_exist(void* extargv) {
    const char* SDK_LAUNCH_QUERY = "shell:ls /usr/sbin/sdk_launch";
    D("query the existence of sdk_launch\n");
    int fd = sdb_connect(SDK_LAUNCH_QUERY, extargv);

    if(fd < 0) {
        D("fail to query the sdbd version\n");
        return fd;
    }

    char query_result[PATH_MAX];
    char* result_ptr = query_result;
    int max_len = PATH_MAX;
    int len;

    while(fd >= 0) {
        len = sdb_read(fd, result_ptr, max_len);
        if(len == 0) {
            break;
        }

        if(len < 0) {
            if(errno == EINTR) {
                continue;
            }
            break;
        }
        max_len -= len;
        result_ptr += len;
        fflush(stdout);
    }

    const char* expected_result = "/usr/sbin/sdk_launch";

    if(!strncmp(expected_result, query_result, strlen(expected_result))) {
        return 1;
    }
    return 0;
}

int sdb_higher_ver(int first, int middle, int last, void* extargv) {

    const char* VERSION_QUERY = "shell:rpm -q sdbd";
    D("query the sdbd version\n");
    int fd = sdb_connect(VERSION_QUERY, extargv);

    if(fd < 0) {
        D("fail to query the sdbd version\n");
        return fd;
    }

    char ver[PATH_MAX];
    int max_len = PATH_MAX;
    char* result_ptr = ver;
    int len;

    D("read sdb version\n");
    while(fd >= 0) {
        len = sdb_read(fd, result_ptr, max_len);
        if(len == 0) {
            break;
        }

        if(len < 0) {
            if(errno == EINTR) {
                continue;
            }
            break;
        }
        max_len -= len;
        result_ptr += len;
        fflush(stdout);
    }

    int version;
    char* ver_num = NULL;

    ver_num = strchr(ver, '-') + 1;

    char* null = NULL;
    null = strchr(ver_num, '-');

    if(null == NULL) {
        goto error;
    }
    *null = '\0';

    D("sdbd version: %s\n", ver_num);

    null = strchr(ver_num, '.');
    if(null == NULL) {
        goto error;
    }

    *null = '\0';
    version = atoi(ver_num);
    if(version > first) {
        return 1;
    }
    if(version < first) {
        return 0;
    }
    ver_num = ++null;

    null = strchr(ver_num, '.');
    if(null == NULL) {
        goto error;
    }

    version = atoi(ver_num);
    *null = '\0';
    if(version > middle) {
        return 1;
    }
    if(version < middle) {
        return 0;
    }
    ver_num = ++null;

    version = atoi(ver_num);
    if(version > last) {
        return 1;
    }
    return 0;

error:
    LOG_ERROR("wrong version format %s", ver);
    return -1;
}

int sdb_status(int fd, int host_fd)
{
    unsigned char buf[5];

    if(readx(fd, buf, 4)) {
        if(host_fd == 0) {
            fprintf(stderr,"error: protocol fault (no status)\n");
        }
        else {
            sendfailmsg(host_fd, "error: protocol fault (no status)\n");
        }
        return -1;
    }

    if(!memcmp(buf, "OKAY", 4)) {
        return 0;
    }

    if(memcmp(buf, "FAIL", 4)) {
        if(host_fd == 0) {
            fprintf(stderr,"error: protocol fault (status %02x %02x %02x %02x?!)\n",
                    buf[0], buf[1], buf[2], buf[3]);
        }
        else {
            char err_msg[255];
            snprintf(err_msg, sizeof(err_msg), "error: protocol fault (status %02x %02x %02x %02x?!)\n",
                    buf[0], buf[1], buf[2], buf[3]);
            sendfailmsg(host_fd, err_msg);
        }
        return -1;
    }

    int len = read_msg_size(fd);
    if(len < 0) {
        if(host_fd == 0) {
            fprintf(stderr,"error: protocol fault (status len)\n");
        }
        else {
            sendfailmsg(host_fd, "error: protocol fault (status len)\n");
        }
        return -1;
    }
    if(len > 254) len = 254;


    char error[255];
    if(readx(fd, error, len)) {
        if(host_fd == 0) {
            fprintf(stderr,"error: protocol fault (status read)\n");
        }
        else {
            sendfailmsg(host_fd, "error: protocol fault (status read)\n");
        }
        return -1;
    }
    error[len] = '\0';
    if(host_fd == 0) {
        fprintf(stderr,"error msg: %s\n", error);
    }
    else {
        char err_msg[255];
        snprintf(err_msg, sizeof(err_msg), "error msg: %s\n", error);
        sendfailmsg(host_fd, err_msg);
    }
    return -1;
}

/**
 * First check whether host service or transport service,
 * If transport service, send transport prefix. Then, do the service.
 * If host service, do the service. does not have to get transport.
 */
int _sdb_connect(const char *service, void** ext_args)
{
    int fd;

    D("_sdb_connect: %s\n", service);

    int server_port = *(int*)ext_args[2];

    fd = sdb_host_connect("127.0.0.1", server_port, SOCK_STREAM);
    if(fd < 0) {
        D("error: cannot connect to daemon\n");
        return -2;
    }

    //If service is not host, send transport_prefix
    if (memcmp(service,"host",4) != 0 && switch_socket_transport(fd, ext_args)) {
        return -1;
    }

    if(send_service_with_length(fd, service, 0) < 0) {
        sdb_close(fd);
        return -1;
    }

    if(sdb_status(fd, 0)) {
        sdb_close(fd);
        return -1;
    }

    D("_sdb_connect: return fd %d\n", fd);
    return fd;
}

int read_msg_size(int fd) {
    char buf[5];

    if(readx(fd, buf, 4)) {
        return -1;
    }

    buf[4] = 0;
    return strtoul(buf, NULL, 16);
}

static int __inline__ write_msg_size(int fd, int size, int host_fd) {
    char tmp[5];
    snprintf(tmp, sizeof tmp, "%04x", size);

    if(writex(fd, tmp, 4)) {
        D("error: write msg size failure\n");
        if(host_fd == 0) {
            fprintf(stderr,"error: write msg size failure\n");
        }
        else {
            sendfailmsg(host_fd, "error: write msg size failure\n");
        }
        return -1;
    }
    return 1;
}

/**
 * First, check the host version.
 * Then, send the service using _sdb_connect
 */
int sdb_connect(const char *service, void** ext_args)
{
    // first query the sdb server's version
    int fd = _sdb_connect("host:version", ext_args);
    int server_port = *(int*)ext_args[2];

    D("sdb_connect: service %s\n", service);
    if(fd == -2) {
        fprintf(stdout,"* daemon not running. starting it now on port %d *\n",
                server_port);
    start_server:
        if(launch_server(server_port)) {
            fprintf(stderr,"* failed to start daemon *\n");
            return -1;
        } else {
            fprintf(stdout,"* daemon started successfully *\n");
        }
        /* give the server some time to start properly and detect devices */
        sdb_sleep_ms(3000);
        // fall through to _sdb_connect
    } else {
        // if server was running, check its version to make sure it is not out of date
        int version = SDB_SERVER_VERSION - 1;

        // if we have a file descriptor, then parse version result
        if(fd >= 0) {
            int n = read_msg_size(fd);
            char buf[100];
            if(n < 0 || readx(fd, buf, n) || sscanf(buf, "%04x", &version) != 1) {
                goto error;
            }
            sdb_close(fd);
        } else {
                return fd;
        }

        if(version != SDB_SERVER_VERSION) {
            printf("client version: %d, server version: %d\nsdb server is out of date.  killing...\n", version, SDB_SERVER_VERSION);
            fd = _sdb_connect("host:kill", ext_args);
            sdb_close(fd);

            /* XXX can we better detect its death? */
            sdb_sleep_ms(2000);
            goto start_server;
        }
    }

    // if the command is start-server, we are done.
    if (!strcmp(service, "host:start-server"))
        return 0;

    fd = _sdb_connect(service, ext_args);
    if(fd == -2) {
        fprintf(stderr,"** daemon still not running");
    }
    D("sdb_connect: return fd %d\n", fd);

    return fd;
error:
    sdb_close(fd);
    return -1;
}


int sdb_command(const char *service, void** extra_args)
{
    int fd = sdb_connect(service, extra_args);
    if(fd < 0) {
        return -1;
    }

    if(sdb_status(fd, 0)) {
        sdb_close(fd);
        return -1;
    }

    return 0;
}

char *sdb_query(const char *service, void** extra_args)
{
    char *tmp;

    D("sdb_query: %s\n", service);
    int fd = sdb_connect(service, extra_args);
    if(fd < 0) {
        return 0;
    }

    int n = read_msg_size(fd);
    if(n < 0 || n > 1024) {
        goto oops;
    }

    tmp = malloc(n + 1);
    if(tmp == 0) goto oops;

    if(readx(fd, tmp, n) == 0) {
        tmp[n] = 0;
        sdb_close(fd);
        return tmp;
    }
    free(tmp);

oops:
    sdb_close(fd);
    return 0;
}

void get_host_prefix(char* prefix, int size, transport_type ttype, const char* serial, HOST_TYPE host_type) {
    if(serial) {
        if(host_type == host) {
            snprintf(prefix, size, "%s%s:", PREFIX_HOST_SERIAL, serial);
        }
        else if(host_type == transport) {
            snprintf(prefix, size, "%s%s", PREFIX_TRANSPORT_SERIAL, serial);
        }
    }
    else {
        char* temp_prefix = NULL;
        if(ttype == kTransportUsb) {
            if(host_type == host) {
                temp_prefix = (char*)PREFIX_HOST_USB;
            }
            else if(host_type == transport) {
                temp_prefix = (char*)PREFIX_TRANSPORT_USB;
            }
        }
        else if(ttype == kTransportLocal) {
            if(host_type == host) {
                temp_prefix = (char*)PREFIX_HOST_LOCAL;
            }
            else if(host_type == transport) {
                temp_prefix = (char*)PREFIX_TRANSPORT_LOCAL;
            }
        }
        else if(ttype == kTransportAny) {
            if(host_type == host) {
                temp_prefix = (char*)PREFIX_HOST_ANY;
            }
            else if(host_type == transport) {
                temp_prefix = (char*)PREFIX_TRANSPORT_ANY;
            }
        }
        snprintf(prefix, size, "%s", temp_prefix);
    }
}
