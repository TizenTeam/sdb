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

#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <stdio.h>

#include "sdb_constants.h"
#include "file_sync_client.h"
#include "file_sync_functions.h"
#include "utils.h"
#include "strutils.h"
#include "fdevent.h"
#include "log.h"

static __inline__ void finalize(int srcfd, int dstfd, FILE_FUNC* srcF, FILE_FUNC* dstF);

void create_copy_info(COPY_INFO** info, char* srcp, char* dstp) {
    *info = (COPY_INFO*)malloc(sizeof(COPY_INFO));
    (*info)->src = srcp;
    (*info)->dst = dstp;
}

static __inline__ void finalize(int srcfd, int dstfd, FILE_FUNC* srcF, FILE_FUNC* dstF) {
    srcF->finalize(srcfd);
    dstF->finalize(dstfd);
}

static int file_copy(int src_fd, int dst_fd, char* srcp, char* dstp, FILE_FUNC* srcF, FILE_FUNC* dstF, unsigned* total_bytes, struct stat* src_stat, char* copy_flag) {
    D("file is copied from 'fd:%d' '%s' to 'fd:%d' '%s'\n", src_fd, srcp, dst_fd, dstp);

    unsigned file_byte = src_stat->st_size;
    char* file_name = get_filename(srcp);

    unsigned flag_size = 1;
    char byte_flag[3] = {' ', 'B', '\0'};
    unsigned written_byte = 0;
    int store_byte = 0;

    if(file_byte > 1024 && file_byte <= 1048576) {
        flag_size = 1024;
        byte_flag[0] = 'K';
        byte_flag[1] = 'B';
    }
    else if(file_byte > 1048576) {
        flag_size = 1048576;
        byte_flag[0] = 'M';
        byte_flag[1] = 'B';
    }

    /**
     * local should be opend first
     * because if remote opens successfully, and fail to open local,
     * remote channel is created and protocol confliction is made
     * it is ok to other pull or push process if remote connection is not made
     */
    if(srcF->local) {
        src_fd = srcF->readopen(src_fd, srcp, src_stat);
        if(src_fd < 0) {
            return -1;
        }
        dst_fd = dstF->writeopen(dst_fd, dstp, src_stat);
        if(dst_fd < 0) {
            srcF->readclose(src_fd);
            return -1;
        }
    }
    else {
        dst_fd = dstF->writeopen(dst_fd, dstp, src_stat);
        if(dst_fd < 0) {
            return -1;
        }
        src_fd = srcF->readopen(src_fd, srcp, src_stat);
        if(src_fd < 0) {
            dstF->writeclose(dst_fd, dstp, src_stat);
            return -1;
        }
    }

    FILE_BUFFER srcbuf;
    srcbuf.id = sync_data;

    while(1) {
        int ret = srcF->readfile(src_fd, srcp, src_stat, &srcbuf);
        if(ret == 0) {
            break;
        }
        else if(ret == 1) {
            if(dstF->writefile(dst_fd, dstp, &srcbuf, &written_byte)) {
                goto error;
            }
        }
        else if(ret == 2) {
            continue;
        }
        else if(ret == 3) {
            if(dstF->writefile(dst_fd, dstp, &srcbuf, &written_byte)) {
                goto error;
            }
            break;
        }
        else {
            goto error;
        }
        //TODO pull / push progress bar
        int percent = 0;
        if( file_byte > 100) {
            percent = written_byte / (file_byte / 100);
        }
        int progress_byte = written_byte / flag_size;

        if(store_byte != progress_byte) {
            fprintf(stderr,"%s %30s\t%3d%%\t%7d%s\r\r", copy_flag, file_name, percent, progress_byte, byte_flag);
            store_byte = progress_byte;
        }
    }
    if(srcF->readclose(src_fd) < 0 || dstF->writeclose(dst_fd, dstp, src_stat) < 0) {
        return -1;
    }

    fprintf(stderr,"%s %30s\t100%%\t%7d%s\n", copy_flag, file_name, file_byte / flag_size, byte_flag);
    *total_bytes = *total_bytes + written_byte;
    return 0;

error:
    srcF->readclose(src_fd);
    dstF->writeclose(dst_fd, dstp, src_stat);
    return -1;
}

static void free_copyinfo(void* data) {
    COPY_INFO* info = (COPY_INFO*)data;
    if(info != NULL) {
        if(info->src != NULL) {
            free(info->src);
            info->src = NULL;
        }
        if(info->dst != NULL) {
            free(info->dst);
            info->dst = NULL;
        }
        free(info);
        info = NULL;
    }
}

int do_sync_copy(char* srcp, char* dstp, FILE_FUNC* srcF, FILE_FUNC* dstF, int is_utf8) {

    char copy_flag[7];
    if(srcF->local) {
        snprintf(copy_flag, sizeof(copy_flag), "%s", "pushed");
    }
    else {
        snprintf(copy_flag, sizeof(copy_flag), "%s", "pulled");
    }

    D("copy %s to the %s\n", srcp, dstp);
    unsigned total_bytes = 0;
    long long start_time = NOW();

    int src_fd = 0;
    int dst_fd = 0;

    int pushed = 0;
    int skiped = 0;
    src_fd = srcF->initialize(srcp);
    dst_fd = dstF->initialize(dstp);
    if(src_fd < 0 || dst_fd < 0) {
        return 1;
    }

    struct stat src_stat;
    struct stat dst_stat;

    if(srcF->_stat(src_fd, srcp, &src_stat, 1)) {
        goto error;
    }

    int src_dir = srcF->is_dir(srcp, &src_stat);

    if(src_dir == -1) {
        fprintf(stderr, ERR_REASON_SYNC_NOT_FILE, srcp);
        goto error;
    }

    int dst_dir = 0;

    if(!dstF->_stat(dst_fd, dstp, &dst_stat, 0)) {
        dst_dir = dstF->is_dir(dstp, &dst_stat);
    }
    else{
        int dst_len = strlen(dstp);
        if( dstp[dst_len - 1] == '/' || dstp[dst_len - 1] == '\\') {
            dst_dir = 1;
        }
    }

    if(dst_dir == -1) {
        fprintf(stderr, ERR_REASON_SYNC_NOT_FILE, dstp);
        goto error;
    }

    if(src_dir == 0) {
        /* if we're copying a local file to a remote directory,
        ** we *really* want to copy to remotedir + "/" + localfilename
        */
        char full_dstpath[PATH_MAX];
        if(dst_dir == 1) {
            char* src_filename = get_filename(srcp);
            append_file(full_dstpath, dstp, src_filename, PATH_MAX);

            if(is_utf8 != 0) {
                dstp = ansi_to_utf8(full_dstpath);
            }
            else {
                dstp = full_dstpath;
            }
        }
        if(!file_copy(src_fd, dst_fd, srcp, dstp, srcF, dstF, &total_bytes, &src_stat, copy_flag)) {
            pushed++;
        }
        else {
            goto error;
        }
    }
    //copy directory
    else {
        LIST_NODE* dir_list = NULL;
        //for free later, do strncpy
        int len = strlen(srcp);
        char* _srcp = (char*)malloc(sizeof(char)*len + 1);
        s_strncpy(_srcp, srcp, len+1);

        len = strlen(dstp);
        char* _dstp = (char*)malloc(sizeof(char)*len + 1);
        s_strncpy(_dstp, dstp, len+1);

        COPY_INFO* __info;
        create_copy_info(&__info, _srcp, _dstp);
        append(&dir_list, __info);

        while(dir_list != NULL) {
            LIST_NODE* entry_list = NULL;
            COPY_INFO* _info = (COPY_INFO*)dir_list->data;
            if(srcF->get_dirlist(src_fd, _info->src, _info->dst, &entry_list)) {
                fprintf(stderr,"skipped: %s -> %s\n", _info->src, _info->dst);
                skiped++;
                free_list(entry_list, NULL);
                remove_first(&dir_list, free_copyinfo);
                continue;
            }
            remove_first(&dir_list, free_copyinfo);
            LIST_NODE* curptr = entry_list;

            while(curptr != NULL) {
                COPY_INFO* info = (COPY_INFO*)curptr->data;
                curptr = curptr->next_ptr;
                char* src_p = (char*)info->src;
                char* dst_p = (char*)info->dst;

                if(srcF->_stat(src_fd, src_p, &src_stat, 1)) {
                    goto skip_in;
                }

                src_dir = srcF->is_dir(src_p, &src_stat);
                if(src_dir < 0) {
                    fprintf(stderr,ERR_REASON_SYNC_NOT_FILE, src_p);
                    goto skip_in;
                }
                if(src_dir == 1) {
                    append(&dir_list, info);
                    continue;
                }
                else {
                    if(!file_copy(src_fd, dst_fd, src_p, dst_p, srcF, dstF, &total_bytes, &src_stat, copy_flag)) {
                        pushed++;
                        free(info);
                        free(src_p);
                        free(dst_p);
                        continue;
                    }
                }
skip_in:
                fprintf(stderr,"skipped: %s -> %s\n", src_p, dst_p);
                skiped++;
                free(info);
                free(src_p);
                free(dst_p);
            }
            free_list(entry_list, no_free);
        }
    }

    fprintf(stderr,"%d file(s) %s. %d file(s) skipped.\n",
            pushed, copy_flag, skiped);

    long long end_time = NOW() - start_time;

    if(end_time != 0) {
        fprintf(stderr,"%-30s   %lld KB/s (%lld bytes in %lld.%03llds)\n",
                srcp,
                ((((long long) total_bytes) * 1000000LL) / end_time) / 1024LL,
                (long long) total_bytes, (end_time / 1000000LL), (end_time % 1000000LL) / 1000LL);
    }

    finalize(src_fd, dst_fd, srcF, dstF);
    return 0;

error:
    finalize(src_fd, dst_fd, srcF, dstF);
    return 1;
}
