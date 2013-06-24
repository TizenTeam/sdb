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

static __inline__ void finalize(int srcfd, int dstfd, FILE_FUNC* srcF, FILE_FUNC* dstF);

__inline__ void create_copy_info(COPY_INFO** info, char* srcp, char* dstp) {
    *info = (COPY_INFO*)malloc(sizeof(COPY_INFO));
    (*info)->src = srcp;
    (*info)->dst = dstp;
}

static __inline__ void finalize(int srcfd, int dstfd, FILE_FUNC* srcF, FILE_FUNC* dstF) {
    srcF->finalize(srcfd);
    dstF->finalize(dstfd);
}

static int file_copy(int src_fd, int dst_fd, char* srcp, char* dstp, FILE_FUNC* srcF, FILE_FUNC* dstF, unsigned* total_bytes) {
    void* srcstat;
    if(srcF->_stat(src_fd, srcp, &srcstat, 1) < 0) {
        return -1;
    }

    src_fd = srcF->readopen(src_fd, srcp, srcstat);
    if(src_fd < 0) {
        return -1;
    }

    dst_fd = dstF->writeopen(dst_fd, dstp, srcstat);
    if(dst_fd < 0) {
        return -1;
    }

    FILE_BUFFER srcbuf;
    srcbuf.id = ID_DATA;

    while(1) {
        int ret = srcF->readfile(src_fd, srcp, srcstat, &srcbuf);
        if(ret == 0) {
            break;
        }
        else if(ret == 1) {
            ret = dstF->writefile(dst_fd, dstp, &srcbuf, total_bytes);
            if(ret < 0) {
                srcF->readclose(src_fd);
                dstF->writeclose(dst_fd, dstp, srcstat);
                return -1;
            }
        }
        else if(ret == 2) {
            continue;
        }
        else if(ret == 3) {
            ret = dstF->writefile(dst_fd, dstp, &srcbuf, total_bytes);
            if(ret < 0) {
                srcF->readclose(src_fd);
                dstF->writeclose(dst_fd, dstp, srcstat);
                return -1;
            }
            break;
        }
        else {
            srcF->readclose(src_fd);
            dstF->writeclose(dst_fd, dstp, srcstat);
            if(ret == 4) {
                return 0;
            }
            return -1;
        }
    }
    srcF->readclose(src_fd);
    dstF->writeclose(dst_fd, dstp, srcstat);
    free(srcstat);
    return 1;
}

static void free_copyinfo(void* data) {
    COPY_INFO* info = (COPY_INFO*)data;
    if(info != NULL) {
        if(info->src != NULL) {
            free(info->src);
        }
        if(info->dst != NULL) {
            free(info->dst);
        }
        free(info);
    }
}

int do_sync_copy(char* srcp, char* dstp, FILE_FUNC* srcF, FILE_FUNC* dstF, int is_utf8, void** ext_argv) {

    unsigned total_bytes = 0;
    long long start_time = NOW();

    int src_fd = 0;
    int dst_fd = 0;

    void* srcstat = NULL;
    void* dststat = NULL;
    int pushed = 0;
    int skiped = 0;
    src_fd = srcF->initialize(srcp, ext_argv);
    dst_fd = dstF->initialize(dstp, ext_argv);
    if(src_fd < 0 || dst_fd < 0) {
        return 1;
    }
    if(srcF->_stat(src_fd, srcp, &srcstat, 1) < 0) {
        finalize(src_fd, dst_fd, srcF, dstF);
        return 1;
    }
    int src_dir = srcF->is_dir(srcp, srcstat, 1);
    int dst_dir = 1;

    if(dstF->_stat(dst_fd, dstp, &dststat, 0) >= 0) {
        dst_dir = dstF->is_dir(dstp, dststat, 0);
    }
    free(dststat);
    free(srcstat);
    if(src_dir == -1 || dst_dir == -1) {
        finalize(src_fd, dst_fd, srcF, dstF);
        return 1;
    }
    //copy file
    else if(src_dir == 0) {
        /* if we're copying a local file to a remote directory,
        ** we *really* want to copy to remotedir + "/" + localfilename
        */
        if(dst_dir == 1) {
            char* src_filename = get_filename(srcp);
            char full_dstpath[PATH_MAX];
            append_file(full_dstpath, dstp, src_filename);

            if(is_utf8 != 0) {
                dstp = ansi_to_utf8(full_dstpath);
            }
            else {
                dstp = full_dstpath;
            }
        }
        int result = file_copy(src_fd, dst_fd, srcp, dstp, srcF, dstF, &total_bytes);
        if(result < 0) {
            finalize(src_fd, dst_fd, srcF, dstF);
            return 1;
        }
        if(result == 1) {
            pushed++;
        }
        else {
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

                if(srcF->_stat(src_fd, src_p, &srcstat, 1) < 0) {
                    finalize(src_fd, dst_fd, srcF, dstF);
                    return 1;
                }

                src_dir = srcF->is_dir(src_p, srcstat, 1);
                free(srcstat);
                if(src_dir < 0) {
                    finalize(src_fd, dst_fd, srcF, dstF);
                    return 1;
                }
                if(src_dir == 1) {
                    append(&dir_list, info);
                }
                else {
                    if(src_dir == 0) {
                        fprintf(stderr,"push: %s -> %s\n", src_p, dst_p);
                        int result = file_copy(src_fd, dst_fd, src_p, dst_p, srcF, dstF, &total_bytes);
                        if(result < 0) {
                            finalize(src_fd, dst_fd, srcF, dstF);
                            return 1;
                        }
                        if(result == 1) {
                            pushed++;
                        }
                        else {
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

    char command[6] = {'p', 'u', 's', 'h', 'e', 'd'};
    if(srcF == &REMOTE_FILE_FUNC) {
        strncpy(command, "pulled", sizeof command);
    }

    fprintf(stderr,"%d file(s) %s. %d file(s) skipped.\n",
            pushed, command, skiped);

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
