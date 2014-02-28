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
* See the License for the specific language governing permissions and
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
        print_error(1, ERR_SITU_SYNC_OPEN_CHANNEL, ERR_REASON_GENERAL_CONNECTION_FAIL, strerror(errno));
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

int _stat_local(int fd, char* path, struct stat* st, int print_err) {

    D("stat local file 'fd:%d' '%s'\n", fd, path);
    if(stat(path, st)) {
        if(print_err) {
            print_error(0, ERR_SITU_SYNC_STAT_FILE, strerror(errno), path);
        }
        st->st_mode = 0;
        return -1;
    }

    return 0;
}

int _stat_remote(int fd, char* path, struct stat* st, int print_err) {

    SYNC_MSG msg;
    int len = strlen(path);
    msg.req.id = sync_stat;
    msg.req.namelen = htoll(len);

    if(writex(fd, &msg.req, sizeof(msg.req)) ||
       writex(fd, path, len)) {
        print_error(1, ERR_SITU_SYNC_STAT_FILE, ERR_REASON_GENERAL_CONNECTION_FAIL, path, strerror(errno));
    }

    if(readx(fd, &msg.stat, sizeof(msg.stat))) {
        print_error(1, ERR_SITU_SYNC_STAT_FILE, ERR_REASON_GENERAL_CONNECTION_FAIL, path, strerror(errno));
    }

    if(msg.stat.id != sync_stat) {
        char expected[5];
        char result[5];
        MKCHAR(expected, sync_stat);
        MKCHAR(result, msg.stat.id);

        print_error(1, ERR_SITU_SYNC_STAT_FILE, ERR_REASON_GENERAL_PROTOCOL_WRONG_ID, path, expected, result);
    }
    st->st_mode = ltohl(msg.stat.mode);

    /**
     * FIXME
     * SDBD does not send error reason if remote stat fails.
     * We cannot know the reason before we change sync protocol.
     */
    if(!st->st_mode) {
        if(print_err) {
            print_error(0, ERR_SITU_SYNC_STAT_FILE, ERR_REASON_GENERAL_UNKNOWN, path);
        }
        return -1;
    }
    st->st_size = ltohl(msg.stat.size);
    D("remote stat: mode %u, size %u\n", st->st_mode, st->st_size);
    return 0;

}

int is_directory_common(char* path, struct stat* st) {
    if(S_ISDIR(st->st_mode)) {
        return 1;
    }
    if(S_ISREG(st->st_mode) || S_ISLNK(st->st_mode)) {
        return 0;
    }

    return -1;
}

//return fd.
int readopen_local(int fd, char* srcp, struct stat* st) {

    D("read open local file 'fd:%d' '%s'\n", fd, srcp);
    fd = sdb_open(srcp, O_RDONLY);

    if(fd < 0) {
        print_error(0, ERR_SITU_SYNC_OPEN_FILE, strerror(errno), srcp);
        return -1;
    }

    return fd;
}

int readopen_remote(int fd, char* srcp, struct stat* st) {
    D("read open remote file 'fd:%d' '%s'\n", fd, srcp);
    SYNC_MSG msg;
    int len;

    len = strlen(srcp);
    if(len > SYNC_CHAR_MAX) {
        print_error(0, ERR_SITU_SYNC_OPEN_FILE, ERR_REASON_GENERAL_PROTOCOL_DATA_OVERRUN, srcp, len, SYNC_CHAR_MAX);
        return -1;
    }

    msg.req.id = sync_recv;
    msg.req.namelen = htoll(len);

    if(writex(fd, &msg.req, sizeof(msg.req)) || writex(fd, srcp, len)) {
        print_error(1, ERR_SITU_SYNC_OPEN_FILE, ERR_REASON_GENERAL_CONNECTION_FAIL, srcp, strerror(errno));
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
        print_error(0, ERR_SITU_SYNC_CREATE_FILE, strerror(errno), dstp);
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

    if(len > SYNC_CHAR_MAX) {
        print_error(0, ERR_SITU_SYNC_CREATE_FILE, ERR_REASON_GENERAL_PROTOCOL_DATA_OVERRUN, dstp, len, SYNC_CHAR_MAX);
        return -1;
    }

    r = strlen(tmp);
    total_len = len + r;

    msg.req.id = sync_send;
    msg.req.namelen = htoll(total_len);

    if(writex(fd, &msg.req, sizeof(msg.req)) ||
            writex(fd, dstp, len) ||
            writex(fd, tmp, r)) {
        print_error(1, ERR_SITU_SYNC_OPEN_FILE, ERR_REASON_GENERAL_CONNECTION_FAIL, dstp, strerror(errno));
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
        print_error(1, ERR_SITU_SYNC_CLOSE_FILE, ERR_REASON_GENERAL_CONNECTION_FAIL, dstp, strerror(errno));
    }

    if(readx(fd, &msg.status, sizeof(msg.status))) {
        print_error(1, ERR_SITU_SYNC_CLOSE_FILE, ERR_REASON_GENERAL_CONNECTION_FAIL, dstp, strerror(errno));
    }

    if(msg.status.id != sync_okay) {
        char buf[256];
        if(msg.status.id == sync_fail) {
            int len = ltohl(msg.status.msglen);
            if(len > 255) {
                len = 255;
            }
            if(!readx(fd, buf, len)) {
                print_error(0, ERR_SITU_SYNC_CLOSE_FILE, buf, dstp);
                return -1;
            }
            print_error(1, ERR_SITU_SYNC_CLOSE_FILE, ERR_REASON_GENERAL_UNKNOWN, dstp);
        }
        char expected[5];
        char result[5];
        MKCHAR(expected, sync_fail);
        MKCHAR(result, msg.status.id);

        print_error(1, ERR_SITU_SYNC_CLOSE_FILE, ERR_REASON_GENERAL_PROTOCOL_WRONG_ID, dstp, expected, result);
    }

    return fd;
}

// 0: finish normally.
// 2: continue load but don't write.
//-1: fail
// 1: write and continue load
// 3: write and stop
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
            print_error(0, ERR_SITU_SYNC_READ_FILE, strerror(errno), srcpath);
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
            print_error(0, ERR_SITU_SYNC_READ_FILE, strerror(errno), srcpath);
            return -1;
        }
        sbuf->data[len] = '\0';
        sbuf->size = htoll(len + 1);

        //write and stop.
        return 3;
    }
#endif


    //fail
    print_error(0, ERR_SITU_SYNC_READ_FILE, ERR_REASON_SYNC_NOT_FILE, srcpath, srcpath);
    return -1;
}

int readfile_remote(int fd, char* srcpath, struct stat* st, FILE_BUFFER* buffer) {
    D("read remote file 'fd:%d' '%s'\n", fd, srcpath);
    SYNC_MSG msg;
    unsigned id;

    if(readx(fd, &(msg.data), sizeof(msg.data))) {
        print_error(1, ERR_SITU_SYNC_READ_FILE, ERR_REASON_GENERAL_CONNECTION_FAIL, srcpath, strerror(errno));
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
            len = buffer->size;
            if(len > 256) {
                len = 255;
            }
            if(!readx(fd, buffer->data, len)) {
                buffer->data[len] = 0;
                print_error(0, ERR_SITU_SYNC_READ_FILE, buffer->data, srcpath);
                return -1;
            }
            print_error(1, ERR_SITU_SYNC_READ_FILE, ERR_REASON_GENERAL_UNKNOWN, srcpath);
        }
        char expected[5];
        char result[5];
        MKCHAR(expected, sync_fail);
        MKCHAR(result, id);

        print_error(1, ERR_SITU_SYNC_READ_FILE, ERR_REASON_GENERAL_PROTOCOL_WRONG_ID, srcpath, expected, result);
    }
    //fail
    if(buffer->size > SYNC_DATA_MAX) {
        print_error(1, ERR_SITU_SYNC_READ_FILE, ERR_REASON_GENERAL_PROTOCOL_DATA_OVERRUN, srcpath, buffer->size, SYNC_DATA_MAX);
    }

    //fail
    if(readx(fd, buffer->data, buffer->size)) {
        print_error(1, ERR_SITU_SYNC_READ_FILE, ERR_REASON_GENERAL_CONNECTION_FAIL, srcpath, strerror(errno));
    }

    //write and continue load
    return 1;
}

int writefile_local(int fd, char* dstp, FILE_BUFFER* sbuf, SYNC_INFO* sync_info) {
    D("write local file 'fd:%d' '%s'\n", fd, dstp);
    char* data = sbuf->data;
    unsigned len = sbuf->size;

    if(writex(fd, data, len)) {
        /**
         * remote channel is already opend
         * if local write fails, protocol conflict happens unless we receive sync_done from remote
         */
        print_error(1, ERR_SITU_SYNC_WRITE_FILE, ERR_REASON_GENERAL_CONNECTION_FAIL, dstp, strerror(errno));
    }

    sync_info->total_bytes += len;
    return 0;
}

int writefile_remote(int fd, char* dstp, FILE_BUFFER* sbuf, SYNC_INFO* sync_info) {
    D("write remote file 'fd:%d' '%s'\n", fd, dstp);
    int size = ltohl(sbuf->size);

    if(writex(fd, sbuf, sizeof(unsigned)*2 + size)) {
        print_error(1, ERR_SITU_SYNC_WRITE_FILE, ERR_REASON_GENERAL_CONNECTION_FAIL, dstp, strerror(errno));
    }

    sync_info->total_bytes += size;
    return 0;
}

int getdirlist_local(int fd, char* src_dir, char* dst_dir, LIST_NODE** dirlist, SYNC_INFO* sync_info) {
    D("get list of local file 'fd:%d' '%s'\n", fd, src_dir);
    DIR* d;

    d = opendir(src_dir);
    if(d == 0) {
        print_error(0, ERR_SITU_SYNC_GET_DIRLIST, strerror(errno), src_dir);
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

        int len = strlen(src_dir) + strlen(file_name) + 2;
        char* src_full_path = (char*)malloc(sizeof(char)*len);
        append_file(src_full_path, src_dir, file_name, len);

        len = strlen(dst_dir) + strlen(file_name) + 2;
        char* dst_full_path = (char*)malloc(sizeof(char)*len);
        append_file(dst_full_path, dst_dir, file_name, len);

        struct stat src_stat;
        if(!_stat_local(fd, src_full_path, &src_stat, 1)) {
            COPY_INFO* info;
            create_copy_info(&info, src_full_path, dst_full_path, &src_stat);
            prepend(dirlist, info);
        }
        else {
            fprintf(stderr,"skipped: %s -> %s\n", src_full_path, dst_full_path);
            sync_info->skipped++;
            free(src_full_path);
            free(dst_full_path);
        }
    }

    closedir(d);
    return 0;
}

int getdirlist_remote(int fd, char* src_dir, char* dst_dir, LIST_NODE** dirlist, SYNC_INFO* sync_info) {
    D("get list of remote file 'fd:%d' '%s'\n", fd, src_dir);
    SYNC_MSG msg;
    int len;

    len = strlen(src_dir);

    if(len > SYNC_CHAR_MAX) {
        print_error(0, ERR_SITU_SYNC_GET_DIRLIST, ERR_REASON_GENERAL_PROTOCOL_DATA_OVERRUN, src_dir, len, SYNC_CHAR_MAX);
        return -1;
    }

    msg.req.id = sync_list;
    msg.req.namelen = htoll(len);

    if(writex(fd, &msg.req, sizeof(msg.req)) ||
       writex(fd, src_dir, len)) {
        print_error(1, ERR_SITU_SYNC_GET_DIRLIST, ERR_REASON_GENERAL_CONNECTION_FAIL, src_dir, strerror(errno));
    }

    while(1) {
        if(readx(fd, &msg.dent, sizeof(msg.dent))) {
            print_error(1, ERR_SITU_SYNC_GET_DIRLIST, ERR_REASON_GENERAL_CONNECTION_FAIL, src_dir, strerror(errno));
        }
        if(msg.dent.id == sync_done) {
            LOG_INFO("getting list of remote file 'fd:%d' '%s' is done\n", fd, src_dir);
            return 0;
        }
        if(msg.dent.id != sync_dent) {
            char expected[5];
            char result[5];
            MKCHAR(expected, sync_dent);
            MKCHAR(result, msg.dent.id);

            print_error(1, ERR_SITU_SYNC_GET_DIRLIST, ERR_REASON_GENERAL_PROTOCOL_WRONG_ID, src_dir, expected, result);
        }
        len = ltohl(msg.dent.namelen);
        if(len > 256) {
            fprintf(stderr,"error: name of a file in the remote directory '%s' exceeds 256\n", src_dir);
            fprintf(stderr,"skipped: %s/? -> %s/?\n", src_dir, dst_dir);
            sync_info->skipped++;
            continue;
        }

        char file_name[257];
        if(readx(fd, file_name, len)) {
            print_error(1, ERR_SITU_SYNC_GET_DIRLIST, ERR_REASON_GENERAL_CONNECTION_FAIL, src_dir, strerror(errno));
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

        len = strlen(src_dir) + strlen(file_name) + 2;
        char* src_full_path = (char*)malloc(sizeof(char)*len);
        append_file(src_full_path, src_dir, file_name, len);

        len = strlen(dst_dir) + strlen(file_name) + 2;
        char* dst_full_path = (char*)malloc(sizeof(char)*len);
        append_file(dst_full_path, dst_dir, file_name, len);

        struct stat st;
        st.st_mode = ltohl(msg.dent.mode);
        /**
         * FIXME
         * SDBD does not send error reason if remote stat fails.
         * We cannot know the reason before we change sync protocol.
         */
        if(!st.st_mode) {
            print_error(0, ERR_SITU_SYNC_STAT_FILE, ERR_REASON_GENERAL_UNKNOWN, file_name);
            fprintf(stderr,"skipped: %s -> %s\n", src_full_path, dst_full_path);
            sync_info->skipped++;
            continue;
        }
        st.st_size = ltohl(msg.dent.size);

        COPY_INFO* info;
        create_copy_info(&info, src_full_path, dst_full_path, &st);
        prepend(dirlist, info);
    }
    return 0;
}

