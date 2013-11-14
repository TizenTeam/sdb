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
#include <stdlib.h>
#include "log.h"
#include "strutils.h"

static size_t total_mem = 0;

// added size of size_t to know how many bytes have been allocated.

void *s_malloc(size_t size) {
    void *ptr = malloc(size + sizeof(size_t));

    if (ptr == NULL) {
        LOG_FATAL("cannot allocate memory:%d bytes\n", (int ) size);
    }
    *(size_t*) ptr = size;

    total_mem += size;

    LOG_DEBUG("memory allocated:%u bytes / %u bytes\n", size, total_mem);

    return ptr + sizeof(size_t);
}

void *s_realloc(void *ptr, size_t new_size) {
    void *new_ptr;
    size_t org_size;

    if (ptr == NULL) {
        LOG_FATAL("null argument in!!\n");
    }

    new_ptr = realloc(ptr - sizeof(size_t), new_size + sizeof(size_t));
    if (new_ptr == NULL) {
        LOG_FATAL("cannot allocate new memory:%d bytes\n", (int ) new_size);
    }
    org_size = *(size_t*) (ptr - sizeof(size_t));
    *(size_t*) new_ptr = new_size;
    total_mem += new_size - org_size;

    LOG_DEBUG("memory allocated:%u bytes -> %u bytes / %u bytes\n", org_size, new_size, total_mem);

    return new_ptr + sizeof(size_t);
}

void s_free(void *ptr) {
    size_t size;

    if (ptr == NULL) {
        return;
    }

    size = *(size_t*) (ptr - sizeof(size_t));
    total_mem -= size;

    free(ptr - sizeof(size_t));

    LOG_DEBUG("memory freed:%u bytes / %u\n", size, total_mem);

}

char *s_strdup(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    int len = strlen(str) + 1;

    char *ptr = s_malloc(len);

    s_strncpy(ptr, str, len);
    return ptr;
}
