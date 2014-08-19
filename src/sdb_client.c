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
#include "strutils.h"

#define  TRACE_TAG  TRACE_SDB
#include "sdb_client.h"
#include "log.h"
#include "sdb_messages.h"

static int switch_socket_transport(int fd);
static int __inline__ write_msg_size(int fd, int size, int host_fd);

char* target_serial = NULL;
transport_type target_ttype = kTransportAny;

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
            print_error(SDB_MESSAGE_ERROR, ERR_GENERAL_EMPTY_SERVICE_NAME, NULL);
        }
        return -1;
    }
    else if (len > 1024) {
        if(host_fd == 0) {
            print_error(SDB_MESSAGE_ERROR, ERR_GENERAL_TOO_LONG_SERVICE_NAME, NULL);
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
            char buf[10];
            sprintf(buf, "%d", fd);
            print_error(SDB_MESSAGE_ERROR, F(ERR_SYNC_WRITE_FAIL, buf),NULL);
        }
        else {
            sendfailmsg(host_fd, "error: write failure during connection\n");
        }
        return -1;
    }

    return 0;
}

static int switch_socket_transport(int fd)
{
    char service[64];

    get_host_prefix(service, sizeof service, target_ttype, target_serial, transport);

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

int sdk_launch_exist() {
    const char* SDK_LAUNCH_QUERY = "shell:ls /usr/sbin/sdk_launch";
    D("query the existence of sdk_launch\n");
    int fd = sdb_connect(SDK_LAUNCH_QUERY);

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

int sdb_higher_ver(int first, int middle, int last) {

    const char* VERSION_QUERY = "shell:rpm -q sdbd";
    D("query the sdbd version\n");
    int fd = sdb_connect(VERSION_QUERY);

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
            print_error(SDB_MESSAGE_ERROR, "protocol fault", "no status");
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
            print_error(SDB_MESSAGE_ERROR, "protocol fault", F("status %02x %02x %02x %02x?!", buf[0], buf[1], buf[2], buf[3]));
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
            print_error(SDB_MESSAGE_ERROR, "protocol fault", "status len");
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
            print_error(SDB_MESSAGE_ERROR, "protocol fault", "status read");
        }
        else {
            sendfailmsg(host_fd, "error: protocol fault (status read)\n");
        }
        return -1;
    }
    error[len] = '\0';
    if(host_fd == 0) {
        fprintf(stderr,"%s\n", error);
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
int _sdb_connect(const char *service)
{
    int fd;

    D("_sdb_connect: %s\n", service);

    if (!strcmp(service, "host:start-server")) {
        return 0;
    }

    fd = sdb_host_connect("127.0.0.1", DEFAULT_SDB_PORT, SOCK_STREAM);
    if(fd < 0) {
        D("error: cannot connect to daemon\n");
        return -2;
    }

    //If service is not host, send transport_prefix
    if (memcmp(service,"host",4) != 0 && switch_socket_transport(fd)) {
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
            print_error(SDB_MESSAGE_ERROR, ERR_GENERAL_WRITE_MESSAGE_SIZE_FAIL, NULL);
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
int sdb_connect(const char *service)
{
    // check version before sending a sdb command
    int fd = _sdb_connect("host:version");

    D("sdb_connect: service %s\n", service);

    if (fd >= 0) {
        int len = read_msg_size(fd);
        char buf[SDB_VERSION_MAX_LENGTH] = {0,};
        int restarting = 0;
        if (len < 0) {
            sdb_close(fd);
            return -1;
        }
        if (readx(fd, buf, len) != 0 ){
            sdb_close(fd);
            return -1;
        }
        sdb_close(fd);
        char *tokens[3];
        size_t cnt = tokenize(buf, ".", tokens, 3);

        if (cnt == 3) { // since tizen2.2.1 rc 15
            int major = strtoul(tokens[0], 0, 10);
            int minor = strtoul(tokens[1], 0, 10);
            int patch = strtoul(tokens[2], 0, 10);
            if (major != SDB_VERSION_MAJOR || minor != SDB_VERSION_MINOR || patch != SDB_VERSION_PATCH ) {
                fprintf(stdout,
                        "* sdb (%s) already running, and restarting sdb (%d.%d.%d) again *\n",
                        buf, SDB_VERSION_MAJOR, SDB_VERSION_MINOR,
                        SDB_VERSION_PATCH);
                restarting = 1;
            }
        } else {
            int ver = 0;
            if (sscanf(buf, "%04x", &ver) != 1) {
                LOG_ERROR("version format is wrong:%s\n", buf);
                // restart anyway!
                restarting = 1;
            } else {
                if (ver != SDB_VERSION_PATCH) {
                    fprintf(stdout,
                            "* another version of sdb already running, and restarting sdb (%d.%d.%d) again *\n",
                            SDB_VERSION_MAJOR, SDB_VERSION_MINOR, SDB_VERSION_PATCH);
                    restarting = 1;
                }
            }
        }
        if (restarting) {
            if (cnt) {
                free_strings(tokens, cnt);
            }
            int fd2 = _sdb_connect("host:kill");
            sdb_close(fd2);
            sdb_sleep_ms(2000);
            goto launch_server;
        }
    }

    if(fd == -2) {
        fprintf(stdout,"* server not running. starting it now on port %d *\n", DEFAULT_SDB_PORT);
launch_server:
        if(launch_server()) {
            print_error(SDB_MESSAGE_ERROR, ERR_GENERAL_START_SERVER_FAIL, NULL);
            return -1;
        } else {
            fprintf(stdout,"* server started successfully *\n");
        }
    }

    fd = _sdb_connect(service);
    if(fd == -2) {
        print_error(SDB_MESSAGE_ERROR, ERR_GENERAL_SERVER_NOT_RUN, NULL);
    }

    D("sdb_connect: return fd %d\n", fd);
    return fd;
}


int sdb_command(const char *service)
{
    int fd = sdb_connect(service);
    if(fd < 0) {
        return -1;
    }

    if(sdb_status(fd, 0)) {
        sdb_close(fd);
        return -1;
    }

    return 0;
}

char *sdb_query(const char *service)
{
    char *tmp;

    D("sdb_query: %s\n", service);
    int fd = sdb_connect(service);
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
