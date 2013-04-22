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



#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sockets.h"
#include <errno.h>
#include <assert.h>

#include "properties.h"
//#include "loghack.h"
#include "sysdeps.h"
#define  TRACE_TAG   TRACE_PROPERTIES
#include "sdb.h"
#include "strutils.h"

#define HAVE_TIZEN_PROPERTY

#ifdef HAVE_TIZEN_PROPERTY

#define HAVE_PTHREADS
#include <stdio.h>
#include "threads.h"

static mutex_t  env_lock = MUTEX_INITIALIZER;

#define TIZEN_PROPERTY_FILE       "/tmp/.sdb.conf" /* tizen specific*/
#define PROPERTY_SEPARATOR "="

struct config_node {
    char *key;
    char value[PROPERTY_VALUE_MAX];
} sdbd_config[] = {
    { "service.sdb.tcp.port", "0" },
    { NULL, "" }
};

void property_save();
int read_line(const int fd, char* ptr, const unsigned int maxlen);

static void property_init(void)
{
    int fd;
    int i = 0;
    char buffer[PROPERTY_KEY_MAX+PROPERTY_VALUE_MAX+1];
    char *tok = NULL;

    fd = unix_open(TIZEN_PROPERTY_FILE, O_RDONLY);
    if (fd < 0)
        return;
    for(;;) {
        if(read_line(fd, buffer, PROPERTY_KEY_MAX+PROPERTY_VALUE_MAX+1) < 0)
            break;
        tok = strtok(buffer, PROPERTY_SEPARATOR);
        for (i = 0; sdbd_config[i].key; i++) {
            if (!strcmp(tok, sdbd_config[i].key)) {
                tok = strtok(NULL, PROPERTY_SEPARATOR);
                strncpy(sdbd_config[i].value, tok, PROPERTY_VALUE_MAX);
                D("property init key=%s, value=%s\n", sdbd_config[i].key, tok);
            }
        }

    }
    sdb_close(fd);
    D("called property_init\n");
}

void property_save()
{
    int fd;
    int i = 0;
    char buffer[PROPERTY_KEY_MAX+PROPERTY_VALUE_MAX+1];

    mutex_lock(&env_lock);
    if (access(TIZEN_PROPERTY_FILE, F_OK) == 0) // if exist
        sdb_unlink(TIZEN_PROPERTY_FILE);

    fd = unix_open(TIZEN_PROPERTY_FILE, O_WRONLY | O_CREAT | O_APPEND, 0640);
    if (fd <0 )
        return;

    for (i = 0; sdbd_config[i].key; i++) {
        sprintf(buffer,"%s%s%s\n", sdbd_config[i].key, PROPERTY_SEPARATOR, sdbd_config[i].value);
        sdb_write(fd, buffer, strlen(buffer));
    }
    sdb_close(fd);
    mutex_unlock(&env_lock);
}

int property_set(const char *key, const char *value)
{
    int i = 0;

    mutex_lock(&env_lock);
    for (i = 0; sdbd_config[i].key; i++) {
        if (!strcmp(key,sdbd_config[i].key)) {
            strncpy(sdbd_config[i].value, value, PROPERTY_VALUE_MAX);
            D("property set key=%s, value=%s\n", key, value);
            break;
        }
    }
    mutex_unlock(&env_lock);
    property_save();
    return -1;
}

int property_get(const char *key, char *value, const char *default_value)
{
    int len = 0;
    int i = 0;

    property_init();
    mutex_lock(&env_lock);
    for (i = 0; sdbd_config[i].key; i++) {
        if (!strcmp(key,sdbd_config[i].key)) {
            len = strlen(sdbd_config[i].value);
            memcpy(value, sdbd_config[i].value, len + 1);
            D("property get key=%s, value=%s\n", key, value);
            mutex_unlock(&env_lock);
            return len;
        }
    }

    if(default_value) {
        len = strlen(default_value);
        memcpy(value, default_value, len + 1);
        D("by default, property get key=%s, value=%s\n", key, value);
    }
    mutex_unlock(&env_lock);
    return len;
}

int property_list(void (*propfn)(const char *key, const char *value, void *cookie),
                  void *cookie)
{
    return 0;
}

#elif defined(HAVE_LIBC_SYSTEM_PROPERTIES)

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
//#include <sys/_system_properties.h>

int property_set(const char *key, const char *value)
{
    return __system_property_set(key, value);
}

int property_get(const char *key, char *value, const char *default_value)
{
    int len;

    len = __system_property_get(key, value);
    if(len > 0) {
        return len;
    }

    if(default_value) {
        len = strlen(default_value);
        memcpy(value, default_value, len + 1);
    }
    return len;
}

int property_list(void (*propfn)(const char *key, const char *value, void *cookie),
                  void *cookie)
{
    char name[PROP_NAME_MAX];
    char value[PROP_VALUE_MAX];
    const prop_info *pi;
    unsigned n;

    for(n = 0; (pi = __system_property_find_nth(n)); n++) {
        __system_property_read(pi, name, value);
        propfn(name, value, cookie);
    }
    return 0;
}

#elif defined(HAVE_SYSTEM_PROPERTY_SERVER)

/*
 * The Linux simulator provides a "system property server" that uses IPC
 * to set/get/list properties.  The file descriptor is shared by all
 * threads in the process, so we use a mutex to ensure that requests
 * from multiple threads don't get interleaved.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

static pthread_once_t gInitOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t gPropertyFdLock = PTHREAD_MUTEX_INITIALIZER;
static int gPropFd = -1;

/*
 * Connect to the properties server.
 *
 * Returns the socket descriptor on success.
 */
static int connectToServer(const char* fileName)
{
    int sock = -1;
    int cc;

    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        D("UNIX domain socket create failed (errno=%d)\n", errno);
        return -1;
    }

    /* connect to socket; fails if file doesn't exist */
    strcpy(addr.sun_path, fileName);    // max 108 bytes
    addr.sun_family = AF_UNIX;
    cc = connect(sock, (struct sockaddr*) &addr, SUN_LEN(&addr));
    if (cc < 0) {
        // ENOENT means socket file doesn't exist
        // ECONNREFUSED means socket exists but nobody is listening
        D("AF_UNIX connect failed for '%s': %s\n",
            fileName, strerror(errno));
        sdb_close(sock);
        return -1;
    }

    return sock;
}

/*
 * Perform one-time initialization.
 */
static void init(void)
{
    assert(gPropFd == -1);

    gPropFd = connectToServer(SYSTEM_PROPERTY_PIPE_NAME);
    if (gPropFd < 0) {
        D("not connected to system property server\n");
    } else {
        D("Connected to system property server\n");
    }
}

int property_get(const char *key, char *value, const char *default_value)
{
    char sendBuf[1+PROPERTY_KEY_MAX];
    char recvBuf[1+PROPERTY_VALUE_MAX];
    int len = -1;

    D("PROPERTY GET [%s]\n", key);

    pthread_once(&gInitOnce, init);
    if (gPropFd < 0) {
        /* this mimics the behavior of the device implementation */
        if (default_value != NULL) {
            strcpy(value, default_value);
            len = strlen(value);
        }
        return len;
    }

    if (strlen(key) >= PROPERTY_KEY_MAX) return -1;

    memset(sendBuf, 0xdd, sizeof(sendBuf));    // placate valgrind

    sendBuf[0] = (char) kSystemPropertyGet;
    strcpy(sendBuf+1, key);

    pthread_mutex_lock(&gPropertyFdLock);
    if (sdb_write(gPropFd, sendBuf, sizeof(sendBuf)) != sizeof(sendBuf)) {
        pthread_mutex_unlock(&gPropertyFdLock);
        return -1;
    }
    if (sdb_read(gPropFd, recvBuf, sizeof(recvBuf)) != sizeof(recvBuf)) {
        pthread_mutex_unlock(&gPropertyFdLock);
        return -1;
    }
    pthread_mutex_unlock(&gPropertyFdLock);

    /* first byte is 0 if value not defined, 1 if found */
    if (recvBuf[0] == 0) {
        if (default_value != NULL) {
            strcpy(value, default_value);
            len = strlen(value);
        } else {
            /*
             * If the value isn't defined, hand back an empty string and
             * a zero length, rather than a failure.  This seems wrong,
             * since you can't tell the difference between "undefined" and
             * "defined but empty", but it's what the device does.
             */
            value[0] = '\0';
            len = 0;
        }
    } else if (recvBuf[0] == 1) {
        strcpy(value, recvBuf+1);
        len = strlen(value);
    } else {
        D("Got strange response to property_get request (%d)\n",
            recvBuf[0]);
        assert(0);
        return -1;
    }
    D("PROP [found=%d def='%s'] (%d) [%s]: [%s]\n",
        recvBuf[0], default_value, len, key, value);

    return len;
}


int property_set(const char *key, const char *value)
{
    char sendBuf[1+PROPERTY_KEY_MAX+PROPERTY_VALUE_MAX];
    char recvBuf[1];
    int result = -1;

    D("PROPERTY SET [%s]: [%s]\n", key, value);

    pthread_once(&gInitOnce, init);
    if (gPropFd < 0)
        return -1;

    if (strlen(key) >= PROPERTY_KEY_MAX) return -1;
    if (strlen(value) >= PROPERTY_VALUE_MAX) return -1;

    memset(sendBuf, 0xdd, sizeof(sendBuf));    // placate valgrind

    sendBuf[0] = (char) kSystemPropertySet;
    strcpy(sendBuf+1, key);
    strcpy(sendBuf+1+PROPERTY_KEY_MAX, value);

    pthread_mutex_lock(&gPropertyFdLock);
    if (sdb_write(gPropFd, sendBuf, sizeof(sendBuf)) != sizeof(sendBuf)) {
        pthread_mutex_unlock(&gPropertyFdLock);
        return -1;
    }
    if (sdb_read(gPropFd, recvBuf, sizeof(recvBuf)) != sizeof(recvBuf)) {
        pthread_mutex_unlock(&gPropertyFdLock);
        return -1;
    }
    pthread_mutex_unlock(&gPropertyFdLock);

    if (recvBuf[0] != 1)
        return -1;
    return 0;
}

int property_list(void (*propfn)(const char *key, const char *value, void *cookie),
                  void *cookie)
{
    D("PROPERTY LIST\n");
    pthread_once(&gInitOnce, init);
    if (gPropFd < 0)
        return -1;

    return 0;
}

#else

/* SUPER-cheesy place-holder implementation for Win32 */
#define HAVE_PTHREADS /*tizen specific */
#include <stdio.h>
#include "threads.h"

static mutex_t  env_lock = MUTEX_INITIALIZER;

int property_get(const char *key, char *value, const char *default_value)
{
    char ename[PROPERTY_KEY_MAX + 6];
    char *p;
    int len;

    len = strlen(key);
    if(len >= PROPERTY_KEY_MAX) return -1;
    memcpy(ename, "PROP_", 5);
    memcpy(ename + 5, key, len + 1);

    mutex_lock(&env_lock);

    p = getenv(ename);
    if(p == 0) p = "";
    len = strlen(p);
    if(len >= PROPERTY_VALUE_MAX) {
        len = PROPERTY_VALUE_MAX - 1;
    }

    if((len == 0) && default_value) {
        len = strlen(default_value);
        memcpy(value, default_value, len + 1);
    } else {
        memcpy(value, p, len);
        value[len] = 0;
    }

    mutex_unlock(&env_lock);
    D("get [key=%s value='%s:%s']\n", key, ename, value);
    return len;
}


int property_set(const char *key, const char *value)
{
    char ename[PROPERTY_KEY_MAX + 6];
    char *p;
    int len;
    int r;

    if(strlen(value) >= PROPERTY_VALUE_MAX) return -1;

    len = strlen(key);
    if(len >= PROPERTY_KEY_MAX) return -1;
    memcpy(ename, "PROP_", 5);
    memcpy(ename + 5, key, len + 1);

    mutex_lock(&env_lock);
#ifdef HAVE_MS_C_RUNTIME
    {
        char  temp[256];
        snprintf( temp, sizeof(temp), "%s=%s", ename, value);
        putenv(temp);
        r = 0;
    }
#else
    r = setenv(ename, value, 1);
#endif
    mutex_unlock(&env_lock);
    D("set [key=%s value='%s%s']\n", key, ename, value);
    return r;
}

int property_list(void (*propfn)(const char *key, const char *value, void *cookie),
                  void *cookie)
{
    return 0;
}

#endif
