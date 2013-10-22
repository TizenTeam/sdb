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

#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <stdio.h>

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

    //unsigned file_byte = src_stat->st_size;
    unsigned written_byte = 0;

    src_fd = srcF->readopen(src_fd, srcp, src_stat);
    if(src_fd < 0) {
        return -1;
    }

    dst_fd = dstF->writeopen(dst_fd, dstp, src_stat);
    if(dst_fd < 0) {
        return -1;
    }

    FILE_BUFFER srcbuf;
    srcbuf.id = sync_data;

    while(1) {
        int ret = srcF->readfile(src_fd, srcp, src_stat, &srcbuf);
        if(ret == 0) {
            break;
        }
        else if(ret == 1) {
            ret = dstF->writefile(dst_fd, dstp, &srcbuf, &written_byte);
            if(ret < 0) {
                srcF->readclose(src_fd);
                dstF->writeclose(dst_fd, dstp, src_stat);
                return -1;
            }
        }
        else if(ret == 2) {
            continue;
        }
        else if(ret == 3) {
            ret = dstF->writefile(dst_fd, dstp, &srcbuf, &written_byte);
            if(ret < 0) {
                srcF->readclose(src_fd);
                dstF->writeclose(dst_fd, dstp, src_stat);
                return -1;
            }
            break;
        }
        else {
            srcF->readclose(src_fd);
            dstF->writeclose(dst_fd, dstp, src_stat);
            if(ret == 4) {
                return 0;
            }
            return -1;
        }
        //TODO pull / push progress bar
        //fprintf(stderr,"%s [%u / %u]: %s -> %s \r", copy_flag, written_byte, file_byte, srcp, dstp);
    }
    if(srcF->readclose(src_fd) < 0 || dstF->writeclose(dst_fd, dstp, src_stat) < 0) {
        return -1;
    }

    fprintf(stderr,"%s: %s -> %s \n", copy_flag, srcp, dstp);
    *total_bytes = *total_bytes + written_byte;
    return 1;
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

int do_sync_copy(char* srcp, char* dstp, FILE_FUNC* srcF, FILE_FUNC* dstF, int is_utf8, void** ext_argv) {

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
    src_fd = srcF->initialize(srcp, ext_argv);
    dst_fd = dstF->initialize(dstp, ext_argv);
    if(src_fd < 0 || dst_fd < 0) {
        return 1;
    }

    struct stat src_stat;
    struct stat dst_stat;

    if(srcF->_stat(src_fd, srcp, &src_stat, 1) < 0) {
        finalize(src_fd, dst_fd, srcF, dstF);
        return 1;
    }
    int src_dir = srcF->is_dir(srcp, &src_stat, 1);
    int dst_dir = 0;

    if(dstF->_stat(dst_fd, dstp, &dst_stat, 0) >= 0) {
        dst_dir = dstF->is_dir(dstp, &dst_stat, 0);
    }
    else {
        int dst_len = strlen(dstp);
        if( dstp[dst_len - 1] == '/' || dstp[dst_len - 1] == '\\') {
            dst_dir = 1;
        }
    }

    if(src_dir == -1 || dst_dir == -1) {
        LOG_ERROR("src_dir: %d, dst_dir %d\n", src_dir, dst_dir);
        finalize(src_fd, dst_fd, srcF, dstF);
        return 1;
    }
    if(src_dir == 0) {
        /* if we're copying a local file to a remote directory,
        ** we *really* want to copy to remotedir + "/" + localfilename
        */
        char full_dstpath[PATH_MAX];
        if(dst_dir == 1) {
            char* src_filename = get_filename(srcp);
            append_file(full_dstpath, dstp, src_filename);

            if(is_utf8 != 0) {
                dstp = ansi_to_utf8(full_dstpath);
            }
            else {
                dstp = full_dstpath;
            }
        }
        int result = file_copy(src_fd, dst_fd, srcp, dstp, srcF, dstF, &total_bytes, &src_stat, copy_flag);

        if(result == 1) {
            pushed++;
        }
        else {
            fprintf(stderr,"skipped: %s -> %s\n", srcp, dstp);
            skiped++;
        }
    }
    else if(src_dir == 2) {
        skiped++;
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
            if(srcF->get_dirlist(src_fd, _info->src, _info->dst, &entry_list) < 0) {
                finalize(src_fd, dst_fd, srcF, dstF);
                return 1;
            }
            LIST_NODE* curptr = entry_list;

            while(curptr != NULL) {
                COPY_INFO* info = (COPY_INFO*)curptr->data;
                char* src_p = (char*)info->src;
                char* dst_p = (char*)info->dst;

                if(srcF->_stat(src_fd, src_p, &src_stat, 1) < 0) {
                    finalize(src_fd, dst_fd, srcF, dstF);
                    return 1;
                }

                src_dir = srcF->is_dir(src_p, &src_stat, 1);
                if(src_dir < 0) {
                    finalize(src_fd, dst_fd, srcF, dstF);
                    return 1;
                }
                if(src_dir == 1) {
                    append(&dir_list, info);
                }
                else {
                    if(src_dir == 0) {
                        int result = file_copy(src_fd, dst_fd, src_p, dst_p, srcF, dstF, &total_bytes, &src_stat, copy_flag);

                        if(result == 1) {
                            pushed++;
                        }
                        else {
                            fprintf(stderr,"skipped: %s -> %s\n", src_p, dst_p);
                            skiped++;
                        }
                    }
                    else if(src_dir == 2) {
                        skiped++;
                    }
                    free(src_p);
                    free(dst_p);
                    free(info);
                }
                curptr = curptr->next_ptr;
            }
            free_list(entry_list, no_free);
            remove_first(&dir_list, free_copyinfo);
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
}
