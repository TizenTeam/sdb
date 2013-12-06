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

#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include "fdevent.h"
#include "utils.h"
#include "sdb_client.h"
#include "file_sync_functions.h"
#include "linkedlist.h"
#include "strutils.h"
#include "file_sync_client.h"
#include "log.h"

const unsigned sync_stat = MKSYNC('S','T','A','T');
const unsigned sync_list = MKSYNC('L','I','S','T');
const unsigned sync_send = MKSYNC('S','E','N','D');
const unsigned sync_recv = MKSYNC('R','E','C','V');
const unsigned sync_dent = MKSYNC('D','E','N','T');
const unsigned sync_done = MKSYNC('D','O','N','E');
const unsigned sync_data = MKSYNC('D','A','T','A');
const unsigned sync_okay = MKSYNC('O','K','A','Y');
const unsigned sync_fail = MKSYNC('F','A','I','L');
const unsigned sync_quit = MKSYNC('Q','U','I','T');

const struct file_function LOCAL_FILE_FUNC = {
        .local = 1,
        .initialize=initialize_local,
        .finalize=finalize_local,
        ._stat=_stat_local,
        .is_dir=is_directory_common,
        .readopen=readopen_local,
        .readclose=readclose_local,
        .writeopen=writeopen_local,
        .writeclose=writeclose_local,
        .readfile=readfile_local,
        .writefile=writefile_local,
        .get_dirlist=getdirlist_local,
};

const struct file_function REMOTE_FILE_FUNC = {
        .local=0,
        .initialize=initialize_remote,
        .finalize=finalize_remote,
        ._stat=_stat_remote,
        .is_dir=is_directory_common,
        .readopen=readopen_remote,
        .readclose=readclose_remote,
        .writeopen=writeopen_remote,
        .writeclose=writeclose_remote,
        .readfile=readfile_remote,
        .writefile=writefile_remote,
        .get_dirlist=getdirlist_remote,
};

static int sync_readstat(int fd, const char *path, struct stat* st);

//return > 0 fd, = 0 success, < 0 fail.
int initialize_local(char* path) {
    D("initialize local file '%s'\n", path);
    return 0;
}

//return fd
int initialize_remote(char* path) {

    D("initialize remote file '%s'\n", path);
    int fd = sdb_connect("sync:");

    if(fd < 0) {
        return -1;
    }

    return fd;
}

void finalize_local(int fd) {
    D("finalize local fd '%d'\n", fd);
}

void finalize_remote(int fd) {
    D("finalize remote fd '%d'\n", fd);
    SYNC_MSG msg;

    msg.req.id = sync_quit;
    msg.req.namelen = 0;

    writex(fd, &msg.req, sizeof(msg.req));

    if(fd > 0) {
        sdb_close(fd);
    }
}

int _stat_local(int fd, char* path, struct stat* st, int show_error) {

    D("stat local file 'fd:%d' '%s'\n", fd, path);
    if(stat(path, st)) {
        if(show_error) {
            fprintf(stderr,"cannot stat '%s': %s\n", path, strerror(errno));
        }
        st->st_mode = 0;
        return -1;
    }

    return 1;
}

int _stat_remote(int fd, char* path, struct stat* st, int show_error) {

    D("stat remote file 'fd:%d' '%s'\n", fd, path);
    if(sync_readstat(fd, path, st)) {
        if(show_error) {
            fprintf(stderr,"cannot read mode '%s': %s\n", path, strerror(errno));
        }
        st->st_mode = 0;
        return -1;
    }

    return 1;
}

int is_directory_common(char* path, struct stat* st, int show_error) {
    if(st->st_mode == 0) {
        if(show_error) {
            fprintf(stderr,"'%s': No such file or directory\n", path);
        }
        return -1;
    }

    if(S_ISDIR(st->st_mode)) {
        return 1;
    }
    if(S_ISREG(st->st_mode) || S_ISLNK(st->st_mode)) {
        return 0;
    }

    return 2;
}

//return fd.
int readopen_local(int fd, char* srcp, struct stat* st) {

    D("read open local file 'fd:%d' '%s'\n", fd, srcp);
    if(S_ISREG(st->st_mode)) {
        fd = sdb_open(srcp, O_RDONLY);

        if(fd < 0) {
            fprintf(stderr,"cannot open local file '%s': %s\n", srcp, strerror(errno));
            return -1;
        }

        return fd;
    }

    return fd;
}

int readopen_remote(int fd, char* srcp, struct stat* st) {
    D("read open remote file 'fd:%d' '%s'\n", fd, srcp);
    SYNC_MSG msg;
    int len;

    len = strlen(srcp);
    if(len > SYNC_CHAR_MAX) {
        fprintf(stderr,"protocol failure while opening remote file '%s' for reading. request should not exceeds %d\n", srcp, SYNC_CHAR_MAX);
        return -1;
    }

    msg.req.id = sync_recv;
    msg.req.namelen = htoll(len);

    if(writex(fd, &msg.req, sizeof(msg.req)) || writex(fd, srcp, len)) {
        fprintf(stderr,"exception occurred while sending read request to remote file'%s': %s\n", srcp, strerror(errno));
        return -1;
    }
    return fd;
}

int readclose_local(int lfd) {
    D("read close local file 'fd:%d'\n", lfd);
    if(lfd > 0) {
        sdb_close(lfd);
    }
    return lfd;
}

int readclose_remote(int fd) {
    D("read close remote file 'fd:%d'\n", fd);
    return fd;
}

int writeopen_local(int fd, char* dstp, struct stat* st) {
    D("write open local file 'fd:%d' '%s'\n", fd, dstp);
    unix_unlink(dstp);
    mkdirs(dstp);
    fd = sdb_creat(dstp, 0644);

    if(fd < 0) {
        fprintf(stderr,"cannot create '%s': %s\n", dstp, strerror(errno));
        return -1;
    }

    return fd;
}

//return fd.
int writeopen_remote(int fd, char* dstp, struct stat* st) {
    D("write open remote file 'fd:%d' '%s'\n", fd, dstp);
    SYNC_MSG msg;

    int len, r;
    int total_len;
    char tmp[64];

    snprintf(tmp, sizeof(tmp), ",%d", st->st_mode);
    len = strlen(dstp);
    r = strlen(tmp);
    total_len = len + r;

    if(total_len > PATH_MAX) {
        fprintf(stderr,"protocol failure. buffer [%s%s] exceeds limits: %d\n", dstp, tmp, PATH_MAX);
        return -1;
    }

    msg.req.id = sync_send;
    msg.req.namelen = htoll(total_len);

    if(writex(fd, &msg.req, sizeof(msg.req)) ||
            writex(fd, dstp, len) ||
            writex(fd, tmp, r)) {
        fprintf(stderr,"exception occurred while sending read msg: %s\n", dstp);
        return -1;
    }

    return fd;
}

int writeclose_local(int fd, char*dstp, struct stat* st) {
    D("write close local file 'fd:%d' '%s'\n", fd, dstp);
    if(fd > 0) {
        sdb_close(fd);
    }
    return fd;
}

int writeclose_remote(int fd, char* dstp, struct stat* st) {
    D("write close remote file 'fd:%d' '%s'\n", fd, dstp);
    SYNC_MSG msg;
    msg.data.id = sync_done;
    msg.data.size = htoll(st->st_mtime);

    if(writex(fd, &msg.data, sizeof(msg.data))) {
        fprintf(stderr,"exception occurred while sending close msg: %s\n", dstp);
        return -1;
    }

    if(readx(fd, &msg.status, sizeof(msg.status))) {
        fprintf(stderr,"exception occurred while receiving close msg: %s\n", dstp);
        return -1;
    }

    if(msg.status.id != sync_okay) {
        char buf[256];
        if(msg.status.id == sync_fail) {
            int len = ltohl(msg.status.msglen);
            if(len > 255) {
                len = 255;
            }
            if(readx(fd, buf, len)) {
                fprintf(stderr, "cannot close remote file '%s' and its failed msg.\n", dstp);
                return -1;
            }
            buf[len] = 0;
        } else {
            strcpy(buf, "unknown reason");
        }

        fprintf(stderr,"cannot close remote file '%s' with failed msg '%s'.\n", dstp, buf);
        return -1;
    }

    return fd;
}

//4: finish skipped
//0: finish normally.
//2: continue load but don't write.
//-1:fail
//1: write and continue load
//3: write and stop
int readfile_local(int lfd, char* srcpath, struct stat* st, FILE_BUFFER* sbuf) {
    D("read local file 'fd:%d' '%s'\n", lfd, srcpath);

    if (S_ISREG(st->st_mode)) {
        int ret;

        ret = sdb_read(lfd, sbuf->data, SYNC_DATA_MAX);
        if (!ret) {
            //Finish normally.
            return 0;
        }
        if (ret < 0) {
            //continue load but don't write
            if (errno == EINTR) {
                return 2;
            }
            //fail.
            fprintf(stderr, "cannot read local file '%s': %s\n", srcpath, strerror(errno));
            return -1;
        }

         sbuf->size = htoll(ret);
        //write and continue load.
        return 1;
    }
#ifdef HAVE_SYMLINKS
    else if (S_ISLNK(st->st_mode)) {
        int len;

        len = readlink(srcpath, sbuf->data, SYNC_DATA_MAX-1);
        //fail
        if(len < 0) {
            fprintf(stderr, "cannot read local link '%s': %s\n", srcpath, strerror(errno));
            return -1;
        }
        sbuf->data[len] = '\0';
        sbuf->size = htoll(len + 1);

        //write and stop.
        return 3;
    }
#endif

    fprintf(stderr,"protocol failure\n");

    //fail
    return -1;
}

int readfile_remote(int fd, char* srcpath, struct stat* st, FILE_BUFFER* buffer) {
    D("read remote file 'fd:%d' '%s'\n", fd, srcpath);
    SYNC_MSG msg;
    unsigned id;

    if(readx(fd, &(msg.data), sizeof(msg.data))) {
        fprintf(stderr, "cannot read remote file status '%s': %s\n", srcpath, strerror(errno));
        return -1;
    }
    id = msg.data.id;
    buffer->size = ltohl(msg.data.size);

    if(id == sync_done) {
        //Finish normally.
        return 0;
    }
    //fail
    if(id != sync_data) {
        int len = 0;
        if(id == sync_fail) {
            int len = buffer->size;
            if(len > 256) {
                len = 255;
            }
            if(readx(fd, buffer->data, len)) {
                fprintf(stderr, "cannot read remote file '%s' and its failed msg. %s\n", srcpath, strerror(errno));
                return -1;
            }
        }
        else {
            len = 4;
            memcpy(buffer->data, &id, len);
        }
        buffer->data[len] = 0;

        fprintf(stderr, "cannot read remote file '%s' with its failed msg '%s'. %s\n", srcpath, buffer->data, strerror(errno));
        return 4;
    }
    //fail
    if(buffer->size > SYNC_DATA_MAX) {
        fprintf(stderr,"data overrun\n");
        return -1;
    }

    //fail
    if(readx(fd, buffer->data, buffer->size)) {
        fprintf(stderr, "cannot read remote file '%s': %s\n", srcpath, strerror(errno));
        return -1;
    }

    //write and continue load
    return 1;
}

int writefile_local(int fd, char* dstp, FILE_BUFFER* sbuf, unsigned* total_bytes) {
    D("write local file 'fd:%d' '%s'\n", fd, dstp);
    char* data = sbuf->data;
    unsigned len = sbuf->size;

    if(writex(fd, data, len)) {
        fprintf(stderr,"cannot write '%s': %s\n", dstp, strerror(errno));
        return -1;
    }

    *total_bytes += len;
    return 0;
}

int writefile_remote(int fd, char* dstp, FILE_BUFFER* sbuf, unsigned* total_bytes) {
    D("write remote file 'fd:%d' '%s'\n", fd, dstp);
    int size = ltohl(sbuf->size);

    if(writex(fd, sbuf, sizeof(unsigned)*2 + size)) {
        fprintf(stderr, "cannot write remote file '%s': %s\n", dstp, strerror(errno));
        return -1;
    }

    *total_bytes += size;
    return 0;
}

int getdirlist_local(int fd, char* src_dir, char* dst_dir, LIST_NODE** dirlist) {
    D("get list of local file 'fd:%d' '%s'\n", fd, src_dir);
    DIR* d;

    d = opendir(src_dir);
    if(d == 0) {
        fprintf(stderr,"cannot open '%s': %s\n", src_dir, strerror(errno));
        readclose_local(fd);
        return -1;
    }
    struct dirent* de;

    while((de = readdir(d))) {
        char* file_name = de->d_name;

        if(file_name[0] == '.') {
            if(file_name[1] == 0) {
                continue;
            }
            if((file_name[1] == '.') && (file_name[2] == 0)) {
                continue;
            }
        }

        char* src_full_path = (char*)malloc(sizeof(char)*PATH_MAX);
        append_file(src_full_path, src_dir, file_name);

        char* dst_full_path = (char*)malloc(sizeof(char)*PATH_MAX);
        append_file(dst_full_path, dst_dir, file_name);

        COPY_INFO* info;
        create_copy_info(&info, src_full_path, dst_full_path);
        prepend(dirlist, info);
    }

    closedir(d);
    return fd;
}

int getdirlist_remote(int fd, char* src_dir, char* dst_dir, LIST_NODE** dirlist) {
    D("get list of remote file 'fd:%d' '%s'\n", fd, src_dir);
    SYNC_MSG msg;
    int len;

    len = strlen(src_dir);

    if(len > SYNC_CHAR_MAX) {
        fprintf(stderr,"protocol failure while getting dirlist of remote file '%s'. request should not exceeds %d\n", src_dir, SYNC_CHAR_MAX);
        return -1;
    }

    msg.req.id = sync_list;
    msg.req.namelen = htoll(len);

    if(writex(fd, &msg.req, sizeof(msg.req)) ||
       writex(fd, src_dir, len)) {
        fprintf(stderr,"cannot request directory entry: '%s'\n", src_dir);
        return -1;
    }

    while(1) {
        if(readx(fd, &msg.dent, sizeof(msg.dent))) {
            fprintf(stderr,"cannot read dirlist: '%s'\n", src_dir);
            return -1;
        }
        if(msg.dent.id == sync_done) {
            return fd;
        }
        if(msg.dent.id != sync_dent) {
            fprintf(stderr,"received dent msg '%d' is not DENT\n", msg.dent.id);
            return -1;
        }
        len = ltohl(msg.dent.namelen);
        if(len > 256) {
            fprintf(stderr,"some file in the remote '%s' exceeds 256\n", src_dir);
            return -1;
        }

        char file_name[257];
        if(readx(fd, file_name, len)) {
            fprintf(stderr,"cannot read file in the remote directory '%s'\n", src_dir);
            return -1;
        }
        file_name[len] = 0;

        if(file_name[0] == '.') {
            if(file_name[1] == 0) {
                continue;
            }
            if((file_name[1] == '.') && (file_name[2] == 0)) {
                continue;
            }
        }

        char* src_full_path = (char*)malloc(sizeof(char)*PATH_MAX);
        append_file(src_full_path, src_dir, file_name);

        char* dst_full_path = (char*)malloc(sizeof(char)*PATH_MAX);
        append_file(dst_full_path, dst_dir, file_name);

        COPY_INFO* info;
        create_copy_info(&info, src_full_path, dst_full_path);
        prepend(dirlist, info);
    }
    D("getting list of remote file 'fd:%d' '%s' is done\n", fd, src_dir);
    return fd;
}

static int sync_readstat(int fd, const char *path, struct stat* st) {
    SYNC_MSG msg;
    int len = strlen(path);
    msg.req.id = sync_stat;
    msg.req.namelen = htoll(len);

    if(writex(fd, &msg.req, sizeof(msg.req)) ||
       writex(fd, path, len)) {
        LOG_ERROR("fail to send request ID_STAT with name length %d\n", len);
        return -1;
    }

    if(readx(fd, &msg.stat, sizeof(msg.stat))) {
        LOG_ERROR("fail to read response of ID_STAT with name length %d\n", len);
        return -1;
    }
    if(msg.stat.id != sync_stat) {
        return -1;
    }
    st->st_mode = ltohl(msg.stat.mode);

    if(!st->st_mode) {
        LOG_ERROR("fail to stat remote file: '%s'", path);
        return -1;
    }
    st->st_size = ltohl(msg.stat.size);
    D("remote stat: mode %u, size %u\n", st->st_mode, st->st_size);
    return 0;
}
