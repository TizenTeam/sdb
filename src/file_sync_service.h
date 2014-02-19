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

#ifndef _FILE_SYNC_SERVICE_H_
#define _FILE_SYNC_SERVICE_H_

#define htoll(x) (x)
#define ltohl(x) (x)
#define MKSYNC(a,b,c,d) ( (d << 24) | (c << 16) | (b << 8) | a )
#define MKCHAR(buf, a)     \
            buf[0] = (char)(a & 0x000000ff); \
            buf[1] = (char)((a & 0x0000ff00) >> 8); \
            buf[2] = (char)((a & 0x00ff0000) >> 16); \
            buf[3] = (char)((a & 0xff000000) >> 24); \
            buf[4] = '\0' \

extern const unsigned sync_stat;
extern const unsigned sync_list;
extern const unsigned sync_send;
extern const unsigned sync_recv;
extern const unsigned sync_dent;
extern const unsigned sync_done;
extern const unsigned sync_data;
extern const unsigned sync_okay;
extern const unsigned sync_fail;
extern const unsigned sync_quit;

typedef struct req REQ;
struct req {
    unsigned id;
    unsigned namelen;
};

typedef struct sdb_stat SDB_STAT;
struct sdb_stat {
    unsigned id;
    unsigned mode;
    unsigned size;
    unsigned time;
};

typedef struct dent DENT;
struct dent {
    unsigned id;
    unsigned mode;
    unsigned size;
    unsigned time;
    unsigned namelen;
};

typedef struct data DATA;
struct data {
    unsigned id;
    unsigned size;
};

typedef struct status STATUS;
struct status {
    unsigned id;
    unsigned msglen;
};

typedef union syncmsg SYNC_MSG;
union syncmsg {
    REQ req;
    SDB_STAT stat;
    DENT dent;
    DATA data;
    STATUS status;
};

#define SYNC_DATA_MAX (64*1024)
#define SYNC_CHAR_MAX 1024

#endif
