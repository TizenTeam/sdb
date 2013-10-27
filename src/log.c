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
#include <errno.h>
#include <string.h>
#include <stdarg.h> // for using va_list
#include "log.h"
#include "utils.h"

int loglevel_mask;

static struct {
    char*  name;
    LogLevel level;
} log_levels[] = {
    { "all", 0 },
    { "fatal", SDBLOG_FATAL },
    { "error", SDBLOG_ERROR },
    { "debug", SDBLOG_DEBUG },
    { "info", SDBLOG_INFO },
    { "fixme", SDBLOG_FIXME },
    { NULL, 0 }
};

void logging_hex(char* hex, char* asci) {

    int hex_len = strnlen(hex, 4096);

    char* hex_ptr = hex;

    fprintf(stderr, "HEX:\n");
    while(hex_len > 512) {
        char hex_tmp = hex_ptr[512];
        hex_ptr[512] = '\0';

        fprintf(stderr, "%s", hex_ptr);
        hex_len = hex_len - 512;
        hex_ptr[512] = hex_tmp;
        hex_ptr = hex_ptr + 512;
    }

    fprintf(stderr, "%s\n", hex_ptr);


    int asci_len = strnlen(asci, 4096);
    char* asci_ptr = asci;

    fprintf(stderr, "ASCI:\n");
    while(asci_len > 512) {
        char asci_tmp = asci_ptr[512];
        asci_ptr[512] = '\0';

        fprintf(stderr, "%s", asci_ptr);
        asci_len = asci_len - 512;
        asci_ptr[512] = asci_tmp;
        asci_ptr = asci_ptr + 512;
    }

    fprintf(stderr, "%s\n", asci_ptr);


}

void logging(LogLevel level, const char *filename, const char *funcname, int line_number, const char *fmt, ...) {
    char *name = NULL;
    char mbuf[1024];
    char fbuf[1024];
    va_list args;

    va_start(args, fmt);

    switch (level) {
        case SDBLOG_FATAL:
            name = log_levels[SDBLOG_FATAL].name;
            break;
        case SDBLOG_ERROR:
            name = log_levels[SDBLOG_ERROR].name;
            break;
        case SDBLOG_INFO:
            name = log_levels[SDBLOG_INFO].name;
            break;
        case SDBLOG_DEBUG:
            name = log_levels[SDBLOG_DEBUG].name;;
            break;
        case SDBLOG_FIXME:
            name = log_levels[SDBLOG_FIXME].name;
            break;
        default:
            name = log_levels[SDBLOG_INFO].name;
            break;
    }
    snprintf(fbuf, sizeof(fbuf), "[%s][%s:%s():%d]%s", name, filename, funcname, line_number, fmt);
    vsnprintf(mbuf, sizeof(mbuf), fbuf, args);
    sdb_mutex_lock(&D_lock, NULL);
    fprintf(stderr, "%s", mbuf);
    sdb_mutex_unlock(&D_lock, NULL);
    fflush(stderr);
    va_end(args);
}

static void log_parse(char* args) {
    char *level, *levels, *next;

    levels = strdup(args);
    if (levels == NULL) {
        return;
    }
    int i=0;
    for (level = levels; level; level = next) {
        next = strchr(level, ',' );
        if (next != NULL) {
            *next++ = 0;
        }

        for (i = 0; log_levels[i].name != NULL; i++) {
            if (!strcmp(level, log_levels[i].name))
            {
                if (!strcmp("all",log_levels[i].name)) {
                    loglevel_mask = ~0;
                    free(levels);
                    return;
                }
                loglevel_mask |= 1 << log_levels[i].level;
                break;
            }
        }
    }
    free(levels);
}

void  log_init(void)
{
    char*  sdb_debug = NULL;

    if ((sdb_debug = getenv(DEBUG_ENV))) {
        log_parse(sdb_debug);
    }
}
