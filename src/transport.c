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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "fdevent.h"
#include "utils.h"
#include "transport.h"
#include "sockets.h"
#include "sdb_constants.h"
#include "strutils.h"
#include "memutils.h"
#include "listener.h"
#include "log.h"

#define   TRACE_TAG  TRACE_TRANSPORT

static void transport_unref(TRANSPORT *t);
static void handle_packet(PACKET *p, TRANSPORT *t);
static void parse_banner(char *banner, TRANSPORT *t);
static void wakeup_select(T_PACKET* t_packet);
static void  update_transports(void);
static void run_transport_close(TRANSPORT* t);
static void encoding_packet(PACKET* p);
static int check_header(PACKET *p);
static int check_data(PACKET *p);
static void  dump_hex( const unsigned char*  ptr, size_t  len);

LIST_NODE* transport_list = NULL;

SDB_MUTEX_DEFINE( transport_lock );
SDB_MUTEX_DEFINE( wakeup_select_lock );

#ifdef _WIN32 /* FIXME : move to sysdeps.h later */
int asprintf( char **, char *, ... );
int vasprintf( char **, char *, va_list );

int vasprintf( char **sptr, char *fmt, va_list argv )
{
    int wanted = vsnprintf( *sptr = NULL, 0, fmt, argv );

    if( (wanted > 0) && ((*sptr = malloc( 1 + wanted )) != NULL) )
      return vsprintf( *sptr, fmt, argv );

    return wanted;
}

int asprintf( char **sptr, char *fmt, ... )
{
    int retval;

    va_list argv;
    va_start( argv, fmt );
    retval = vasprintf( sptr, fmt, argv );
    va_end( argv );

    return retval;
}
#endif

#define MAX_DUMP_HEX_LEN 30
//#define MAX_DUMP_HEX_LEN 4096
static void  dump_hex( const unsigned char*  ptr, size_t  len)
{
    if(SDB_TRACING) {
        char hex_str[]= "0123456789abcdef";

        if(len > MAX_DUMP_HEX_LEN) {
            len = MAX_DUMP_HEX_LEN;
        }

        int  i;
        char hex[len*2 + 1];
        for (i = 0; i < len; i++) {
            hex[i*2 + 0] = hex_str[ptr[i] >> 4];
            hex[i*2 + 1] = hex_str[ptr[i] & 0x0F];
        }
        hex[len*2] = '\0';

        char asci[len + 1];
        for (i = 0; i < len; i++) {
            if ((int)ptr[i] >= 32 && (int)ptr[i] <= 127) {
                asci[i] = ptr[i];
            }
            else {
                asci[i] = '.';
            }
        }
        asci[len] = '\0';

        DR("HEX:'%s', ASCI:'%s'\n", hex, asci);
//        LOG_HEX(hex, asci);
    }
}

void
kick_transport(TRANSPORT*  t)
{
    if (t && !t->kicked)
    {
        int  kicked;

        sdb_mutex_lock(&transport_lock, "transport kick_transport");
        kicked = t->kicked;
        if (!kicked)
            t->kicked = 1;
        sdb_mutex_unlock(&transport_lock, "transport kick_transport");

        if (!kicked)
            t->kick(t);
    }
}

static void run_transport_close(TRANSPORT* t)
{
    D("T(%s)\n", t->serial);
    LIST_NODE* curptr = listener_list;

    while(curptr != NULL) {
        LISTENER* l = curptr->data;
        curptr = curptr->next_ptr;

        if(l->transport == t) {
            D("LN(%s) being closed by T(%s)\n", l->local_name, t->serial);
            remove_node(&listener_list, l->node, free_listener);
        }
    }

    curptr = local_socket_list;
    while(curptr != NULL) {
        SDB_SOCKET* s = curptr->data;
        curptr = curptr->next_ptr;

        if(s->transport == t) {
            D("LS(%X) FD(%d) being closed by T(%s)\n", s->local_id, s->fd, t->serial);
            local_socket_close(s);
        }
    }
}


void dump_packet(const char* name, const char* func, PACKET* p)
{
    if(SDB_TRACING) {
        unsigned  cmd = p->msg.command;
        char command[9];

        if(cmd == A_CLSE) {
            snprintf(command, sizeof command, "%s", "A_CLSE");
        }
        else if(cmd == A_CNXN) {
            snprintf(command, sizeof command, "%s", "A_CNXN");
        }
        else if(cmd == A_OPEN) {
            snprintf(command, sizeof command, "%s", "A_OPEN");
        }
        else if(cmd == A_OKAY) {
            snprintf(command, sizeof command, "%s", "A_OKAY");
        }
        else if(cmd == A_WRTE) {
            snprintf(command, sizeof command, "%s", "A_WRTE");
        }
        else if(cmd == A_TCLS) {
            snprintf(command, sizeof command, "%s", "A_TCLS");
        }
        else {
            //unrecongnized command dump the hexadecimal value.
            snprintf(command, sizeof command, "%08x", cmd);
        }

        D("T(%s) %s: [%s] arg0=%X arg1=%X (len=%d) (total_msg_len=%d)\n",
            name, func, command, p->msg.arg0, p->msg.arg1, p->msg.data_length, p->len);
        dump_hex(p->data, p->msg.data_length);
    }
}

static void encoding_packet(PACKET* p) {
    unsigned char *x;
    unsigned sum;
    unsigned count;

    p->msg.magic = p->msg.command ^ 0xffffffff;

    count = p->msg.data_length;
    x = (unsigned char *) p->data;
    sum = 0;
    while(count-- > 0){
        sum += *x++;
    }
    p->msg.data_check = sum;
}

void send_packet(PACKET *p, TRANSPORT *t)
{
    if(t != NULL && t->connection_state != CS_OFFLINE) {
        encoding_packet(p);

        D("%s: transport got packet, sending to remote\n", t->serial);
        t->write_to_remote(p, t);
    }
    else {
        if (t == NULL) {
            D("Transport is null \n");
            errno = 0;
            LOG_FATAL("Transport is null\n");
        }
        else {
            D("%s: transport ignoring packet while offline\n", t->serial);
        }
    }
}

static __inline__ void wakeup_select(T_PACKET* t_packet) {
    sdb_mutex_lock(&wakeup_select_lock, "wakeup_select");
    writex(fdevent_wakeup_send, &t_packet, sizeof(t_packet));
    sdb_mutex_unlock(&wakeup_select_lock, "wakeup_select");
}

static void handle_packet(PACKET *p, TRANSPORT *t)
{
    unsigned int cmd = p->msg.command;
    T_PACKET* t_packet = malloc(sizeof(T_PACKET));
    t_packet->t = t;
    t_packet->p = NULL;

    //below commands should be done in main thread. packet is used in wakeup_select_func. Do not put a packet
    if(cmd == A_WRTE || cmd == A_CLSE || cmd == A_CNXN || cmd == A_OKAY || cmd == A_STAT) {
        ++(t->req);
        t_packet->p = p;
        wakeup_select(t_packet);
        return;
    }
    else if(cmd == A_OPEN) {
        LOG_FATAL("server does not handle A_OPEN\n");
        exit(1);
    }
    D("Unknown packet command %08x\n", p->msg.command);
    put_apacket(p);
    free(t_packet);
}

#define CNXN_DATA_MAX_TOKENS 3
static void parse_banner(char *data, TRANSPORT *t)
{
    char *banner = s_strdup(data);
    char *end = NULL;

    end = strchr(banner, ':');
    if(end) {
        *end = '\0';
    }
    const char* target_banner = STATE_HOST;
    if(!strcmp(banner, STATE_DEVICE)) {
        t->connection_state = CS_DEVICE;
        target_banner = STATE_DEVICE;
    }
    else if(!strcmp(banner, STATE_BOOTLOADER)){
        t->connection_state = CS_BOOTLOADER;
        target_banner = STATE_BOOTLOADER;
    }
    else if(!strcmp(banner, STATE_RECOVERY)) {
        t->connection_state = CS_RECOVERY;
        target_banner = STATE_RECOVERY;
    }
    else if(!strcmp(banner, STATE_SIDELOAD)) {
        t->connection_state = CS_SIDELOAD;
        target_banner = STATE_SIDELOAD;
    }
    else {
        t->connection_state = CS_HOST;
    }
    s_free(banner);
    // since version 2
    char *tokens[CNXN_DATA_MAX_TOKENS];
    size_t cnt = tokenize(data, "::", tokens, CNXN_DATA_MAX_TOKENS);

    if (cnt == 3) {
        // update device_name except usb device but it should be changed soon.
        if (strcmp(STATE_UNKNOWN, tokens[1])) {
            t->device_name = strdup(tokens[1]);
        }

        if (!strcmp(tokens[2], "1")) {
            t->connection_state = CS_PWLOCK;
            target_banner = STATE_LOCKED;
        }
    }

    if (cnt) {
        free_strings(tokens, cnt);
    }

    D("setting connection_state to '%s'\n", target_banner);
    update_transports();
    return;
}

int list_transports_msg(char*  buffer, size_t  bufferlen)
{
    char  head[5];
    int   len;

    len = list_transports(buffer+4, bufferlen-4);
    snprintf(head, sizeof(head), "%04x", len);
    memcpy(buffer, head, 4);
    len += 4;
    return len;
}

static void  update_transports(void)
{
    D("update transports\n");
    char             buffer[1024];
    int              len;

    len = list_transports_msg(buffer, sizeof(buffer));


    LIST_NODE* curptr = local_socket_list;
    while(curptr != NULL) {
        SDB_SOCKET *s = curptr->data;
        curptr = curptr->next_ptr;
        if (HAS_SOCKET_STATUS(s, DEVICE_TRACKER)) {
            device_tracker_send(s, buffer, len);
        }
    }
}

void send_cmd(unsigned arg0, unsigned arg1, unsigned cmd, char* data, TRANSPORT* t) {
    PACKET *p = get_apacket();
    p->msg.arg0 = arg0;
    p->msg.arg1 = arg1;
    p->msg.command = cmd;

    if(data != NULL) {
        snprintf((char*)p->data, sizeof(p->data), "%s", data);
        p->msg.data_length = strlen((char*)p->data) + 1;
    }

    send_packet(p, t);
    put_apacket(p);
}

static void *transport_thread(void *_t)
{
    TRANSPORT *t = _t;
    PACKET *p;

    D("T(%s), FD(%d)\n", t->serial, t->sfd);
    t->connection_state = CS_WAITCNXN;
    send_cmd(A_VERSION, MAX_PAYLOAD, A_CNXN, "host::", t);
    t->connection_state = CS_OFFLINE;
    // allow the device some time to respond to the connect message
    sdb_sleep_ms(1000);

    D("%s: data dump started\n", t->serial);
    while(1) {
        p = get_apacket();
        LOG_INFO("T(%s) remote read start\n", t->serial);

        if(t->read_from_remote(t, &p->msg, sizeof(MESSAGE))) {
        	break;
        }
		if(check_header(p)) {
			break;
		}
		if(p->msg.data_length) {
			if(t->read_from_remote(t, p->data, p->msg.data_length)){
				break;
			}
		}
		if(check_data(p)) {
			break;
		}
		dump_packet(t->serial, "remote_read", p);
		D("%s: received remote packet, sending to transport\n",
		  t->serial);
		handle_packet(p, t);
    }
	LOG_INFO("T(%s) remote read fail. terminate transport\n", t->serial);
	put_apacket(p);

    t->connection_state = CS_OFFLINE;
    do {
        if(t->req == t->res) {
            p = get_apacket();
            p->msg.command = A_TCLS;
            T_PACKET* t_packet = malloc(sizeof(T_PACKET));
            t_packet->t = t;
            t_packet->p = p;
            wakeup_select(t_packet);
            break;
        }
        else {
            //TODO this should be changed to wait later.
            sdb_sleep_ms(1000);
        }
    }
    while(1);
    return 0;
}

void register_transport(TRANSPORT *t)
{
    D("T(%s), device name: '%s'\n", t->serial, t->device_name);
    sdb_thread_t transport_thread_ptr;

    //transport is updated by transport_thread, we do not have to update here.
    if(sdb_thread_create(&transport_thread_ptr, transport_thread, t)){
        LOG_FATAL("cannot create output thread\n");
    }

        /* put us on the master device list */
    sdb_mutex_lock(&transport_lock, "transport register_transport");
    t->node = prepend(&transport_list, t);
    sdb_mutex_unlock(&transport_lock, "transport register_transport");
}

//lock is done by transport_unref
static void remove_transport(TRANSPORT *t)
{
    D("transport removed. T(%s), device name: %s\n", t->serial, t->device_name);

    remove_node(&transport_list, t->node, no_free);

    //In Windows, handle is not removed from sdb_handle_map, yet.
#ifdef OS_WINDOWS
    sdb_close(t->sfd);
#endif

    run_transport_close(t);

    if (t->serial)
        free(t->serial);
    if (t->device_name)
        free(t->device_name);

    free(t);
}


static void transport_unref(TRANSPORT *t)
{
    if (t == NULL) {
        return;
    }

    sdb_mutex_lock(&transport_lock, "transport_unref transport");
    int nr;

    D("transport: %s unref (kicking and closing)\n", t->serial);
    if (!t->kicked) {
        t->kicked = 1;
        t->kick(t);
    }
    t->close(t);
    remove_transport(t);

    LIST_NODE* curptr = transport_list;

    while(curptr != NULL) {
        TRANSPORT* tmp = curptr->data;
        curptr = curptr->next_ptr;
        if (tmp->type == kTransportUsb) {
            if (tmp->device_name && sscanf(tmp->device_name, "device-%d", &nr) == 1) {
                free(tmp->device_name);
                asprintf(&tmp->device_name, "device-%d", nr - 1);
            }
        }
    }

    sdb_mutex_unlock(&transport_lock, "transport_unref transport");
    update_transports();
}

TRANSPORT *acquire_one_transport(transport_type ttype, const char* serial, char** error_out)
{
    TRANSPORT *result = NULL;
    char* null_str = NULL;

    if(error_out == NULL) {
        error_out = &null_str;
    }

    sdb_mutex_lock(&transport_lock, "transport acquire_one_transport");

    LIST_NODE* curptr = transport_list;
    while(curptr != NULL) {
        TRANSPORT* transport_ = curptr->data;
        curptr = curptr->next_ptr;

        /* check for matching serial number */
        if (serial) {
            if (transport_->serial && !strcmp(serial, transport_->serial)) {
                result = transport_;
                break;
            }
        } else {
            if(ttype == kTransportAny) {
                if (result) {
                    *error_out = (char*)TRANSPORT_ERR_MORE_THAN_ONE_TARGET;
                    result = NULL;
                    break;
                }
                result = transport_;
            }
            if (ttype == transport_->type) {
                if (result) {
                    if(ttype == kTransportUsb) {
                        *error_out = (char*)TRANSPORT_ERR_MORE_THAN_ONE_DEV;
                    }
                    else if(ttype == kTransportLocal) {
                        *error_out = (char*)TRANSPORT_ERR_MORE_THAN_ONE_EMUL;
                    }
                    result = NULL;
                    break;
                }
                result = transport_;
            }
        }
    }

    sdb_mutex_unlock(&transport_lock, "transport acquire_one_transport");

    if (result == NULL ) {
        *error_out = (char*)TRANSPORT_ERR_TARGET_NOT_FOUND;
    }

    return result;
}

int list_transports(char *buf, size_t  bufsize)
{
    char*       p   = buf;
    char*       end = buf + bufsize;
    int         len;

        /* XXX OVERRUN PROBLEMS XXX */
    sdb_mutex_lock(&transport_lock, "transport list_transports");

    LIST_NODE* curptr = transport_list;
    while(curptr != NULL) {
        TRANSPORT* t = curptr->data;
        curptr = curptr->next_ptr;
        const char* serial = t->serial;
        const char* devicename = (t->device_name == NULL) ? DEFAULT_DEVICENAME : t->device_name; /* tizen specific */
        if (!serial || !serial[0])
            serial = "????????????";
        // FIXME: what if each string length is longger than static length?
        len = snprintf(p, end - p, "%-20s\t%-10s\t%s\n", serial, connection_state_name(t), devicename);

        if (p + len >= end) {
            /* discard last line if buffer is too short */
            break;
        }
        p += len;
    }

    p[0] = 0;
    sdb_mutex_unlock(&transport_lock, "transport list_transports");
    return p - buf;
}

int register_device_con_transport(int s, const char *serial) {

    //TODO REMOTE_DEVICE_CONNECT complete device connect after resolving security issue
#if 0
    if(current_local_transports >= SDB_LOCAL_TRANSPORT_MAX) {
        LOG_ERROR("Too many tcp connection\n");
        return -1;
    }

    TRANSPORT *t = calloc(1, sizeof(TRANSPORT));
    char buff[32];

    if (!serial) {
        snprintf(buff, sizeof buff, "T-%p", t);
    }
    else {
        snprintf(buff, sizeof buff, "T-%s", serial);
    }
    serial = buff;

    init_socket_transport(t, s, 0);
    t->remote_cnxn_socket = NULL;
    t->serial = strdup(buff);
    t->device_name = strdup("unknown");
    t->type = kTransportRemoteDevCon;
    TRANSPORT* old_t = acquire_one_transport(kTransportAny, serial, NULL);
    if(old_t != NULL) {
        D("old transport '%s' is found. Unregister it\n", old_t->serial);
        kick_transport(old_t);
    }

    ++current_local_transports;
    register_transport(t);
    return 0;
#endif
    return -1;
}

#undef TRACE_TAG
#define TRACE_TAG  TRACE_RWX

int readx(int fd, void *ptr, size_t len)
{
    char *p = ptr;
    D("FD(%d) wanted=%d\n", fd, (int)len);
    while(len > 0) {
        int r = sdb_read(fd, p, len);
        if(r < 0) {
            if(errno == EINTR) {
                continue;
            }
            LOG_ERROR("FD(%d) error %d: %s\n", fd, errno, strerror(errno));
            return -1;
        }
        if( r == 0) {
            D("FD(%d) disconnected\n", fd);
            return -1;
        }
        len -= r;
        p += r;
    }
    return 0;
}

int writex(int fd, const void *ptr, size_t len)
{
    char *p = (char *)ptr;

    while( len > 0) {
        int r = sdb_write(fd, p, len);
        if(r < 0) {
            if (errno == EINTR) {
                continue;
            }
            D("fd=%d error %d: %s\n", fd, errno, strerror(errno));
            return -1;
        }
        if( r == 0) {
            D("fd=%d disconnected\n", fd);
            return -1;
        }

        len -= r;
        p += r;
    }
    return 0;
}

static int check_header(PACKET *p)
{
    if(p->msg.magic != (p->msg.command ^ 0xffffffff)) {
        LOG_ERROR("check_header(): invalid magic\n");
        return -1;
    }

    if(p->msg.data_length > MAX_PAYLOAD) {
    	LOG_ERROR("check_header(): %d > MAX_PAYLOAD\n", p->msg.data_length);
        return -1;
    }

    LOG_INFO("success to check header\n");
    return 0;
}

static int check_data(PACKET *p)
{
    unsigned count, sum;
    unsigned char *x;

    count = p->msg.data_length;
    x = p->data;
    sum = 0;
    while(count-- > 0) {
        sum += *x++;
    }

    if(sum != p->msg.data_check) {
        return -1;
    } else {
        return 0;
    }
}

static unsigned int decoding_to_remote_ls_id(unsigned int encoded_ls_id) {
    unsigned int remote_ls_id = encoded_ls_id & ~15;
    return remote_ls_id;
}

static unsigned int decoding_to_local_ls_id(unsigned encoded_ls_id) {
    unsigned int local_ls_id = encoded_ls_id & 15;
    local_ls_id |= remote_con_flag;
    return local_ls_id;
}

void wakeup_select_func(int _fd, unsigned ev, void *data) {
    T_PACKET* t_packet = NULL;

    readx(_fd, &t_packet, sizeof(t_packet));

    TRANSPORT* t= t_packet->t;
    D("T(%s)\n", t->serial);
    PACKET* p = t_packet->p;
    free(t_packet);

    if(p == NULL) {
        D("T(%S) packet NULL\n", t->serial);
        return;
    }

    int c_state = t->connection_state;
    unsigned int cmd = p->msg.command;
    unsigned int local_id = p->msg.arg1;
    unsigned int remote_id = p->msg.arg0;
    SDB_SOCKET* sock = NULL;
    //CNXN cannot be distinguished using remote_con_flag
    if(t->remote_cnxn_socket != NULL && cmd == A_CNXN) {
        dump_packet("remote_con", "wakeup_select_func", p);
        sock = t->remote_cnxn_socket->data;
        if(sock != NULL) {
            remove_first(&(t->remote_cnxn_socket), no_free);
            LOG_INFO("LS_L(%X)\n", sock->local_id);
            p->ptr = (void*)(&p->msg);
            p->len = sizeof(MESSAGE) + p->msg.data_length;
            local_socket_enqueue(sock, p);
        }
        goto endup;
    }
    //If transport is remote device, packet should not have to be decoded.
    if((local_id & remote_con_flag) && t->type != kTransportRemoteDevCon) {
        LOG_INFO("LS_L(%X), LS_R(%X), LS_E(%X)\n", decoding_to_local_ls_id(local_id),
                decoding_to_remote_ls_id(local_id), local_id);
        sock = find_local_socket(decoding_to_local_ls_id(local_id));
        p->msg.arg1 = decoding_to_remote_ls_id(local_id);
        p->ptr = (void*)(&p->msg);
        p->len = sizeof(MESSAGE) + p->msg.data_length;
        local_socket_enqueue(sock, p);
        goto endup;
    }
    sock = find_local_socket(local_id);

    if(c_state != CS_OFFLINE && sock != NULL) {
        //packet is used by local_socket_enqueue do not put a packet.
        if(cmd == A_WRTE) {
            D("T(%s) write packet from RS(%d) to LS(%X)\n", t->serial, remote_id, local_id);
            p->len = p->msg.data_length;
            p->ptr = p->data;
            if(local_socket_enqueue(sock, p) == 0) {
                send_cmd(local_id, remote_id, A_OKAY, NULL, t);
            }
            goto endup;
        }
        else if(cmd == A_OKAY) {
            if(!HAS_SOCKET_STATUS(sock, REMOTE_SOCKET)) {
                SET_SOCKET_STATUS(sock, REMOTE_SOCKET);
                D("remote socket attached LS(%X), RS(%d)\n", sock->local_id, sock->remote_id);
                sock->remote_id = remote_id;
                sock->transport = t;
            }
            //TODO HOT PATCH FOR 2048.
            if(sock->check_2048 == 1) {
                sock->check_2048 = 0;
                PACKET *__p = get_apacket();
                __p->msg.command = A_WRTE;
                __p->msg.arg0 = sock->local_id;
                __p->msg.arg1 = sock->remote_id;
                __p->data[0] = sock->char_2048;
                __p->msg.data_length = 1;
                send_packet(__p, sock->transport);
            }
            else {
                local_socket_ready(sock);
            }
        }
    }
    if(cmd == A_CLSE) {
        if(sock != NULL) {
            D("T(%s) close LS(%X)\n", t->serial, local_id);
            local_socket_close(sock);
        }
    }
    else if(cmd == A_CNXN) {
        D("T(%s) gets CNXN\n", t->serial);
        if(t->connection_state != CS_OFFLINE) {
            t->connection_state = CS_OFFLINE;
            run_transport_close(t);
        }
        parse_banner((char*) p->data, t);
    }
    else if(cmd == A_STAT) {
        D("T(%s) gets A_STAT:%d\n", t->serial, p->msg.arg0);
        if (t->connection_state != CS_OFFLINE) {
            t->connection_state = CS_OFFLINE;
        }
        if (p->msg.arg0 == 1) {
            t->connection_state = CS_PWLOCK;
        } else {
            t->connection_state = CS_DEVICE;
        }
        update_transports();
    }
    else if(cmd == A_TCLS) {
        //transport thread is finished
        transport_unref(t);
        return;
    }
    put_apacket(p);

endup:
    //request is done. res increases 1.
    ++(t->res);
}

const char *connection_state_name(TRANSPORT *t)
{
    if(t != NULL) {
        int state = t->connection_state;

        if(state == CS_OFFLINE) {
            return STATE_OFFLINE;
        }
        if(state == CS_BOOTLOADER) {
            return STATE_BOOTLOADER;
        }
        if(state == CS_DEVICE) {
            return STATE_DEVICE;
        }
        if(state == CS_HOST) {
            return STATE_HOST;
        }
        if(state == CS_RECOVERY) {
            return STATE_RECOVERY;
        }
        if(state == CS_SIDELOAD) {
            return STATE_SIDELOAD;
        }
        if (state == CS_PWLOCK) {
            return STATE_LOCKED;
        }
    }
    return STATE_UNKNOWN;
}

PACKET *get_apacket(void)
{
    PACKET *p = malloc(sizeof(PACKET));
    if(p == 0) {
        LOG_FATAL("failed to allocate an apacket\n");
    }
    memset(p, 0, sizeof(PACKET) - MAX_PAYLOAD);
    return p;
}

void put_apacket(void *p)
{
    PACKET* packet = p;
    free(packet);
}
