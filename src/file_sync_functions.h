/*
* SDB - Smart Development Bridge
*
* Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
*
* Contact:
* Ho Namkoong <ho.namkoong@samsung.com>
* Yoonki Park <yoonki.park@samsung.com>
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions andㄴ
* limitations under the License.
*
* Contributors:
* - S-Core Co., Ltd
*
*/

#ifndef FILE_SYNC_FUNCTIONS_H_
#define FILE_SYNC_FUNCTIONS_H_

#include "file_sync_service.h"
#include "linkedlist.h"

struct copy_info {
    char* src;
    char* dst;
};
typedef struct copy_info COPY_INFO;

struct _file_buf {
    unsigned id;
    unsigned size;
    char data[SYNC_DATA_MAX];
};
typedef struct _file_buf FILE_BUFFER;

int initialize_local(char* path, void** extargv);
int initialize_remote(char* path, void** extargv);

void finalize_local(int fd);
void finalize_remote(int fd);

int _stat_local(int fd, char* path, void** _stat, int show_error);
int _stat_remote(int fd, char* path, void** stat, int show_error);

int is_directory_local(char* path, void* stat, int show_error);
int is_directory_remote(char* path, void* stat, int show_error);

int readopen_local(int fd, char* srcp, void* srcstat);
int readopen_remote(int fd, char* srcp, void* srcstat);

int readclose_local(int lfd);
int readclose_remote(int fd);

int writeopen_local(int fd, char* dstp, void* stat);
int writeopen_remote(int fd, char* dstp, void* stat);

int writeclose_local(int fd, char*dstp, void* stat);
int writeclose_remote(int fd, char* dstp, void* stat);

int readfile_local(int lfd, char* srcpath, void* stat, FILE_BUFFER* sbuf);
int readfile_remote(int fd, char* srcpath, void* stat, FILE_BUFFER* buffer);

int writefile_local(int fd, char* dstp, FILE_BUFFER* sbuf, unsigned* total_bytes);
int writefile_remote(int fd, char* dstp, FILE_BUFFER* sbuf, unsigned* total_bytes);

int getdirlist_local(int fd, char* src_dir, char* dst_dir, LIST_NODE** dirlist);
int getdirlist_remote(int fd, char* src_dir, char* dst_dir, LIST_NODE** dirlist);

struct file_function {
    int(*initialize)(char* path, void** extargv);
    void(*finalize)(int fd);
    int(*_stat)(int fd, char* path, void** stat, int show_error);
    int(*is_dir)(char* path, void* stat, int show_error);
    int(*readopen)(int fd, char* dstp, void* stat);
    int(*readclose)(int fd);
    int(*writeopen)(int fd, char* dstp, void* stat);
    int(*writeclose)(int fd, char* dstp, void* stat);
    int(*readfile)(int fd, char* path, void* stat, FILE_BUFFER* buf);
    int(*writefile)(int fd, char* path, FILE_BUFFER* buf, unsigned* total_bytes);
    int(*get_dirlist)(int fd, char* src_dir, char* dst_dir, LIST_NODE** dirlist);
};
typedef struct file_function FILE_FUNC;

const FILE_FUNC LOCAL_FILE_FUNC;
const FILE_FUNC REMOTE_FILE_FUNC;

#endif /* FILE_SYNC_FUNCTIONS_H_ */
