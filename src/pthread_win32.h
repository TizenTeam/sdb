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
#ifndef _PTHREAD_WIN32_H
#define _PTHREAD_WIN32_H

//#include <windows.h>

// implementing posix thread on win32

typedef struct {
    HANDLE handle;
    void *(*start_routine)(void*);
    void *arg;
    DWORD tid;
} pthread_t;

typedef struct {
    LONG waiters;
    int was_broadcast;
    CRITICAL_SECTION waiters_lock;
    HANDLE sema;
    HANDLE continue_broadcast;
} pthread_cond_t;


typedef CRITICAL_SECTION        pthread_mutex_t;
typedef  pthread_mutex_t        CRITICAL_SECTION;

#define  SDB_MUTEX_DEFINE(x)    sdb_mutex_t   x

#endif /* _PTHREAD_WIN32_H */
