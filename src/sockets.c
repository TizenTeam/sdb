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
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "fdevent.h"
#include "sdb_constants.h"
#include "sdb_map.h"
#include "sdb_client.h"
#include "sockets.h"
#include "transport.h"
#include "log.h"
#include "listener.h"
#include "sdb.h"

#define  TRACE_TAG  TRACE_SOCKETS

static int qemu_socket_enqueue(SDB_SOCKET *s, PACKET *p);
static int smart_socket_check(SDB_SOCKET *s, PACKET **p);
static int smart_socket_enqueue(SDB_SOCKET *s, PACKET *p);
static int local_enqueue(int fd, PACKET* p, SDB_SOCKET* s, int event_func);
static int peer_enqueue(SDB_SOCKET* socket, PACKET* p);
static void local_socket_destroy(SDB_SOCKET  *s);
static void remove_socket(void* s);
static void destroy_socket(void* data);
static void local_socket_event_func(int fd, unsigned ev, void *_s);
static unsigned unhex(unsigned char*s, int len);
static int find_transports(char **serial_out, const char *prefix);
static void unregister_all_tcp_transports();
static void connect_emulator(char* host, int port, char* buf, int buf_len);

//TODO REMOTE_DEVICE_CONNECT
//const unsigned int unsigned_int_bit = sizeof(unsigned int) * 8;
//const unsigned int remote_con_right_padding = ~(~0 << sizeof(unsigned int) * 4);
//const unsigned int remote_con_flag = 1 << (sizeof(unsigned int) * 8 - 1);
//unsigned int remote_con_cur_r_id = 1;
//unsigned int remote_con_cur_l_number = 0;
//const unsigned int remote_con_l_max = 16; // Ox1111
//const unsigned int remote_con_r_max = ~(~0 << (sizeof(unsigned int) * 8 - 5));
//unsigned int remote_con_l_table[16] = {0,};

static unsigned local_socket_next_id = 1;

LIST_NODE* local_socket_list = NULL;

SDB_SOCKET *find_local_socket(unsigned id)
{
    SDB_SOCKET *result = NULL;


    LIST_NODE* curptr = local_socket_list;
    while(curptr != NULL) {
        SDB_SOCKET* s = curptr->data;
        curptr = curptr->next_ptr;
        if(s->local_id == id) {
            result = s;
            break;
        }
    }

    return result;
}

static void remove_socket(void* s)
{
    SDB_SOCKET* socket = s;
    socket->node = NULL;
}

int local_socket_enqueue(SDB_SOCKET *s, PACKET *p)
{
    D("LS(%X) local enqueue\n", s->local_id);

    if(s->pkt_list == NULL) {
        int r = local_enqueue(s->fd, p, s, 0);
        if( r == 0) {
            //enqueue done normally
            return 0;
        }
        if( r < 0) {
            //error occurred
            return 1;
        }
    }
    //wait for next round
    append(&(s->pkt_list), p);
    FDEVENT_ADD(&s->fde, FDE_WRITE);

    return 1;
}

void local_socket_ready(SDB_SOCKET *s)
{
    D("local socket ready. LS(%X)\n", s->local_id);
    if(HAS_SOCKET_STATUS(s, NOTIFY)) {
        D("local socket notify to the client FD(%d)\n", s->fd);
        sdb_write(s->fd, "OKAY", 4);
        REMOVE_SOCKET_STATUS(s, NOTIFY);
    }
    FDEVENT_ADD(&s->fde, FDE_READ);
}

void local_socket_close(SDB_SOCKET *s)
{
    D("LS(%X) FD(%d)\n", s->local_id, s->fd);
    if(HAS_SOCKET_STATUS(s, NOTIFY)) {
        D("LS(%X) fail to send notify\n", s->local_id);
        sendfailmsg(s->fd, "closed");
        REMOVE_SOCKET_STATUS(s, NOTIFY);
    }


    if(HAS_SOCKET_STATUS(s, REMOTE_SOCKET)) {
        send_cmd(0, s->remote_id, A_CLSE, NULL, s->transport);
    }
    int id = s->local_id;

    if(!s->closing && s->pkt_list != NULL) {
        s->closing = 1;
        FDEVENT_DEL(&s->fde, FDE_READ);
        remove_node(&local_socket_list, s->node, remove_socket);
        D("LS(%X) pending close\n", id);
    }
    else {
        local_socket_destroy(s);
        D("LS(%X) closed\n", id);
    }
}

static void destroy_socket(void* data) {
    SDB_SOCKET* socket = data;
    socket->node = NULL;

    //TODO REMOTE_DEVICE_CONNECT
//    if(HAS_SOCKET_STATUS(socket, REMOTE_CON)) {
//        free(socket->read_packet);
//        unsigned int id = socket->local_id & ~remote_con_flag;
//
//        TRANSPORT* t = socket->transport;
//        if(t != NULL) {
//            LIST_NODE* node = t->remote_cnxn_socket;
//            while(node != NULL) {
//                SDB_SOCKET* s = node->data;
//                node = node->next_ptr;
//                if(s == socket) {
//                    remove_node(&(t->remote_cnxn_socket), s->node, no_free);
//                    break;
//                }
//            }
//        }
//        remote_con_l_table[id] = 0;
//    }
    socket->local_id = 0;
    free(socket);
}

// be sure to hold the socket list lock when calling this
static void local_socket_destroy(SDB_SOCKET  *s)
{
    D("LS(%X) FD(%d)\n", s->local_id, s->fd);

        /* IMPORTANT: fdevent_remove closes the fd
        ** that belongs to this socket
        */
    fdevent_remove(&s->fde);

        /* dispose of any unwritten data */
    free_list(s->pkt_list, put_apacket);
    s->pkt_list = NULL;

    if(s->node != NULL) {
        remove_node(&local_socket_list, s->node, destroy_socket);
    }
    else {
        destroy_socket(s);
    }
}

    //TODO REMOTE_DEVICE_CONNECT block this code until security issue is cleared
#if 0
//remote_ls_id is a socket in the remote server.
//local_ls_id is a socket of fowarding socket
// left half is local_ls_id and right half is remote_ls_id.
static unsigned int encoding_ls_id(unsigned int remote_ls_id, unsigned int local_ls_id) {
    D("LS_R(%X), LS_L(%X)\n", remote_ls_id, local_ls_id);
    return remote_ls_id | local_ls_id;
}

static void send_remote_con_packet(SDB_SOCKET* sock, PACKET* p) {

    //message and data are filled send it to the target.
    if(p->msg.command != A_CNXN) {
        //A_CNXN does not have to encode local socket id.
        p->msg.arg0 = encoding_ls_id(p->msg.arg0, sock->local_id);
        LOG_INFO("LS_L(%X) LS_E(%X) encoding done\n", sock->local_id, p->msg.arg0);
    }
    else {
        //for notifying transport that it has remaning socket which wait for CNXN
        LOG_INFO("LS_L(%X) message was CNXN\n", sock->local_id);
        append(&sock->transport->remote_cnxn_socket, sock);
    }

    encoding_packet(p);
    //send packet to the target transport
    if(sock->transport->write_to_remote(p, sock->transport)) {
        LOG_ERROR("fail to write packet\n");
        dump_packet("write error", "remote_con_enqueue", p);
    }
}

static int remote_con_enqueue(SDB_SOCKET* socket, PACKET* p) {
    LOG_INFO("LS_L(%X)\n", socket->local_id);

    PACKET* read_p = socket->read_packet;

    if(read_p->len == 0 && p->len >= sizeof(MESSAGE)) {
        if(p->len == sizeof(MESSAGE) + p->msg.data_length) {
            //send packet directly without memcpy.
            if(check_header(p) || check_data(read_p)) {
                LOG_ERROR("bad packet: terminated (data)\n");
                dump_packet("bad packet", "remote_con_enqueue", read_p);
                put_apacket(p);
                return 0;
            }

            LOG_INFO("packet is complete send directly\n");
            send_remote_con_packet(socket, p);
            put_apacket(p);
            return 1;
        }
    }

    while(p != NULL) {
        //read MESSAGE
        if(read_p->len < sizeof(MESSAGE)) {
            int read_length = 0;
            int msg_done = 1;
            if(read_p->len + p->len >= sizeof(MESSAGE)) {
                read_length = sizeof(MESSAGE) - read_p->len;
            }
            else {
                read_length = p->len;
                msg_done = 0;
            }
            memcpy((void*)&(read_p->msg) + read_p->len, p->ptr, read_length);
            p->ptr += read_length;
            p->len -= read_length;
            read_p->len += read_length;
            if(msg_done) {
                LOG_INFO("message reading done\n");

                #if 0 && defined HAVE_BIG_ENDIAN
                    D("read remote packet: %04x arg0=%0x arg1=%0x data_length=%0x data_check=%0x magic=%0x\n",
                            socket_p->msg.command, socket_p->msg.arg0, socket_p->msg.arg1, socket_p->msg.data_length, socket_p->msg.data_check, socket_p->msg.magic);
                #endif
                if(check_header(read_p)) {
                    LOG_ERROR("bad header: terminated (data)\n");
                    dump_packet("bad header", "remote_con_enqueue", read_p);
                    read_p->len = 0;
                    put_apacket(p);
                    return 0;
                }
            }
            else {
                //wait for next round
                LOG_INFO("wait for next round while reading message\n");
                put_apacket(p);
                return 0;
            }
        }
        //read data
        unsigned data_length = read_p->msg.data_length + sizeof(MESSAGE);
        int read_length = 0;
        int data_done = 1;
        if(read_p->len + p->len >= data_length) {
            read_length = data_length - read_p->len;
        }
        else {
            read_length = p->len;
            data_done = 0;
        }

        memcpy((void*)&(read_p->msg) + read_p->len, p->ptr, read_length);
        p->ptr += read_length;
        p->len -= read_length;
        read_p->len += read_length;

        if(data_done) {
            LOG_INFO("data reading done\n");
            if(check_data(read_p)) {
                LOG_ERROR("bad data: terminated (data)\n");
                dump_packet("bad data", "remote_con_enqueue", read_p);
                read_p->len = 0;
                put_apacket(p);
                return 0;
            }

            send_remote_con_packet(socket, read_p);
            read_p->len = 0;
        }

        if(p->len == 0) {
            LOG_INFO("reading all the packet is done\n");
            put_apacket(p);
            p = NULL;
        }
    }

    LOG_INFO("read_p->len: %d\n", read_p->len);
    return 1;
}
#endif

static int peer_enqueue(SDB_SOCKET* socket, PACKET* p) {
    if(HAS_SOCKET_STATUS(socket, REMOTE_SOCKET)) {
        D("entered remote_socket_enqueue RS(%d) WRTE fd=%d\n",
                socket->remote_id, socket->fd);
        p->msg.command = A_WRTE;
        p->msg.arg0 = socket->local_id;
        p->msg.arg1 = socket->remote_id;
        p->msg.data_length = p->len;
        send_packet(p, socket->transport);
        put_apacket(p);
        FDEVENT_DEL(&socket->fde, FDE_READ);
        return 1;
    }

    //TODO REMOTE_DEVICE_CONNECT block this code until security issue is resolved
#if 0
    if(HAS_SOCKET_STATUS(socket, REMOTE_CON)) {
        return remote_con_enqueue(socket, p);
    }
#endif

    if(HAS_SOCKET_STATUS(socket, DEVICE_TRACKER)) {
        D("close device tracker.fd: '%d', LS(%X)", socket->fd, socket->local_id);
        put_apacket(p);
        local_socket_close(socket);
        return -1;
    }

    if(HAS_SOCKET_STATUS(socket, QEMU_SOCKET)) {
        return qemu_socket_enqueue(socket, p);
    }

    //packet can be queued, so do not free it here.
    return smart_socket_enqueue(socket, p);
}

/**
 * If returns 0, appending is done. packet is freed.
 * 1, EAGAIN happens packet is put in the queue. wait for next round.
 * -1, error happens. socket is closed. return immediately.
 */
static int local_enqueue(int fd, PACKET* p, SDB_SOCKET* s, int event_func) {
    D("LS(%X) FD(%d)\n", s->local_id, fd);
    while(p->len > 0) {
        dump_packet("0", "local_enqueue", p);
        int r = sdb_write(fd, p->ptr, p->len);
        if(r < 0) {
            if(errno == EAGAIN) {
                //packet is used next round do not free.
                D( "LS(%X) EAGAIN pending to next round\n", s->local_id);
                return 1;
            }
            //TODO if we handle EINTR when local_socket_enqueue calls this, Windows debug fails.
            if(errno == EINTR && event_func) {
                D( "LS(%X) EINTR continue\n", s->local_id);
                continue;
            }
        }
        if(r > 0) {
            p->len -= r;
            p->ptr += r;
            continue;
        }
        //error happens. close the socket.
        D( "LS(%X) error ER(%d) %s\n", s->local_id, errno, strerror(errno) );
        put_apacket(p);
        local_socket_close(s);
        return -1; /* not ready (error) */
    }
    //append done.
    D("LS(%X) enqueue done\n", s->local_id);
    put_apacket(p);
    return 0;
}

static void local_socket_event_func(int fd, unsigned ev, void *_s)
{
    SDB_SOCKET *s = _s;

    LOG_INFO("LS(%X) FD(%d)\n", s->local_id, s->fd);

    /* put the FDE_WRITE processing before the FDE_READ
    ** in order to simplify the code.
    */
    if(ev & FDE_WRITE){
        D("LS(%X) gets a write event\n", s->local_id);
        while(s->pkt_list != NULL) {
            PACKET* p = s->pkt_list->data;
            int r = local_enqueue(fd, p, s, 1);

            if ( r == 0) {
                //packet enqueue done
                remove_first(&s->pkt_list, no_free);
            }
            else {
                //in both cases r = 1 and r = -1, return immediately.
                return;
            }
        }
        if (s->closing) {
            D("LS(%X) closing because 'closing' is set after write\n", s->local_id);
            local_socket_close(s);
            return;
        }
        //all the packet in the pkt_list is enqueued. write event is done.
        FDEVENT_DEL(&s->fde, FDE_WRITE);
        if(HAS_SOCKET_STATUS(s, REMOTE_SOCKET)) {
            send_cmd(s->local_id, s->remote_id, A_OKAY, NULL, s->transport);
        }
    }


    if(ev & FDE_READ){
        D("LS(%X) gets a read event\n", s->local_id);
        //packet should be freed in peer_enqueue
        PACKET *p = get_apacket();

        void *x;
        //TODO REMOTE_DEVICE_CONNECT
//        if(HAS_SOCKET_STATUS(s, REMOTE_CON)) {
//            x = &p->msg;
//            p->ptr = &p->msg;
//        }
//        else {
            x = p->data;
//        }
        size_t avail = MAX_PAYLOAD;
        int r = 1;

        while(avail > 0) {
            r = sdb_read(fd, x, avail);
            if(r > 0) {
                avail -= r;
                x += r;
                continue;
            }
            if(r < 0) {
                if(errno == EAGAIN) {
                    D("LS(%X) EAGAIN while reading\n", s->local_id);
                    break;
                }
                if(errno == EINTR) {
                    continue;
                }
                LOG_ERROR("LS(%X) error while reading ER(%d), %s\n", s->local_id, errno, strerror(errno));
                // r = 0 means EOF or unhandled error.
                r = 0;
            }
            D("LS(%X) reading done\n", s->local_id);
            break;
        }

        if(avail == MAX_PAYLOAD) {
            put_apacket(p);
        } else {
            p->len = MAX_PAYLOAD - avail;

            if(peer_enqueue(s, p) < 0) {
                //local socket is already closed by peer or should not close the socket.
                return;
            }
        }
        if(r == 0) {
            D("LS(%X) EOF. closing\n", s->local_id);
            local_socket_close(s);
        }
    }
}

SDB_SOCKET *create_local_socket(int fd)
{
    D("FD(%d)\n", fd);
    SDB_SOCKET *s = calloc(1, sizeof(SDB_SOCKET));
    if (s == NULL) {
        LOG_FATAL("cannot allocate socket\n");
    }
    s->status = 0;
    s->node = NULL;
    s->pkt_list = NULL;
    s->fd = fd;

    s->local_id = local_socket_next_id++;
    s->node = prepend(&local_socket_list, s);

    fdevent_install(&s->fde, fd, local_socket_event_func, s);
    D("LS(%X) FD(%d) created\n", s->local_id, s->fd);
    return s;
}

//TODO REMOTE_DEVICE_CONNECT
//void create_remote_connection_socket(SDB_SOCKET* socket) {
//    LOG_INFO("FD(%d)\n", socket->fd);
//    SET_SOCKET_STATUS(socket, REMOTE_CON);
//    socket->read_packet = malloc(sizeof(PACKET));
//    socket->read_packet->len = 0;
//}

void connect_to_remote(SDB_SOCKET *s, const char* destination)
{
    D("LS(%X)\n", s->local_id);
    PACKET *p = get_apacket();
    int len = strlen(destination) + 1;

    if(len > (MAX_PAYLOAD-1)) {
        LOG_FATAL("destination oversized\n");
    }

    D("LS(%X): connect('%s')\n", s->local_id, destination);
    p->msg.command = A_OPEN;
    p->msg.arg0 = s->local_id;
    p->msg.data_length = len;
    strcpy((char*) p->data, destination);
    send_packet(p, s->transport);
    put_apacket(p);
}

static unsigned unhex(unsigned char*s, int len) {

    unsigned n = 0;
    while(len > 0) {
        MAP_KEY key;
        key.key_int = (int)(0 | *s++);
        void* _c = map_get(&hex_map, key);
        unsigned c = (unsigned)_c;
        n = (n << 4) | c;
        len--;
    }
    return n;
}

/**
 * return 0 for host
 * return 1 for transport
 */
static int parse_host_service(char* host_str, char** service_ptr, TRANSPORT** t, char** err_str) {

    int prefix_len = strlen(PREFIX_HOST_USB);
    if (!strncmp(host_str, PREFIX_HOST_USB, prefix_len)) {
        *service_ptr = host_str + prefix_len;
        *t = acquire_one_transport(kTransportUsb, NULL, err_str);
        return 0;
    }
    prefix_len = strlen(PREFIX_HOST_LOCAL);
    if (!strncmp(host_str, PREFIX_HOST_LOCAL, prefix_len)) {
        *service_ptr = host_str + prefix_len;
        *t = acquire_one_transport(kTransportLocal, NULL, err_str);
        return 0;
    }
    prefix_len = strlen(PREFIX_HOST);
    if (!strncmp(host_str, PREFIX_HOST, prefix_len)) {
        if(!strncmp(host_str, PREFIX_TRANSPORT_ANY, strlen(PREFIX_TRANSPORT_ANY))) {
            *t = acquire_one_transport(kTransportAny, NULL, err_str);
            return 1;
        }
        if(!strncmp(host_str, PREFIX_TRANSPORT_LOCAL, strlen(PREFIX_TRANSPORT_LOCAL))) {
            *t = acquire_one_transport(kTransportLocal, NULL, err_str);
            return 1;
        }
        if(!strncmp(host_str, PREFIX_TRANSPORT_USB, strlen(PREFIX_TRANSPORT_USB))) {
            *t = acquire_one_transport(kTransportUsb, NULL, err_str);
            return 1;
        }
        if(!strncmp(host_str, PREFIX_TRANSPORT_SERIAL, strlen(PREFIX_TRANSPORT_SERIAL))) {
            *t = acquire_one_transport(kTransportAny, host_str + strlen(PREFIX_TRANSPORT_SERIAL), err_str);
            return 1;
        }
        *service_ptr = host_str + prefix_len;
        //does not have to find transport.
        *t = NULL;
        *err_str = NULL;
        return 0;
    }
    prefix_len = strlen(PREFIX_HOST_ANY);
    if(!strncmp(host_str, PREFIX_HOST_ANY, prefix_len)) {
        *service_ptr = host_str + prefix_len;
        *t = acquire_one_transport(kTransportAny, NULL, err_str);
        return 0;
    }
    prefix_len = strlen(PREFIX_HOST_SERIAL);
    if(!strncmp(host_str, PREFIX_HOST_SERIAL, prefix_len)) {
        char* serial = host_str + prefix_len;
        char* end = strchr(serial, ':');
        if(end == NULL) {
            *err_str = (char*)ERR_TRANSPORT_TARGET_NOT_FOUND;
            return 0;
        }

        char* new_end = end + 1;
        if(isdigit(*new_end)) {
            while(1) {
                new_end++;
                if(!isdigit(*new_end)) {
                    if(*new_end == ':') {
                        end = new_end;
                    }
                    break;
                }
            }
        }

        *end = '\0';
        *service_ptr = end + 1;
        *t = acquire_one_transport(kTransportAny, serial, err_str);
        return 0;
    }
    *service_ptr = NULL;
    return 0;
}

static int handle_request_with_t(SDB_SOCKET* socket, char* service, TRANSPORT* t, char* err_str) {
    int forward = 0;

    if(!strncmp(service,"forward:",8)) {
        forward = 8;
    }
    else if (!strncmp(service,"killforward:",12)) {
        forward = 12;
    }

    if(forward) {
        char* forward_err = NULL;

        char *local, *remote = NULL;
        local = service + forward;
        remote = strchr(local,';');

        if (t == NULL || t->connection_state == CS_OFFLINE) {
            if(t != NULL) {
                forward_err = (char*)ERR_TRANSPORT_TARGET_OFFLINE;
            }
            else {
                forward_err = err_str;
            }
            goto sendfail;
        }
        if(remote == 0 || remote[1] == '\0') {
            forward_err = "malformed forward spec";
            goto sendfail;
        }
        *remote++ = 0;

        if(strncmp("tcp:", local, 4)){
            forward_err = (char*)ERR_FORWARD_UNKNOWN_LOCAL_PORT;
            goto sendfail;
        }

        if(strncmp("tcp:", remote, 4)){
            forward_err = (char*)ERR_FORWARD_UNKNOWN_REMOTE_PORT;
            goto sendfail;
        }

        if (forward == 8) {
            if(!install_listener(atoi(local + 4), atoi(remote + 4), t, forwardListener)) {
                writex(socket->fd, "OKAYOKAY", 8);
                return 0;
            }
            else {
                forward_err = (char*)ERR_FORWARD_INSTALL_FAIL;
                goto sendfail;
            }
        } else {
            if(!remove_listener(atoi(local + 4), atoi(remote + 4), t)) {
                writex(socket->fd, "OKAYOKAY", 8);
                return 0;
            } else {
                forward_err = (char*)ERR_FORWARD_REMOVE_FAIL;
                goto sendfail;
            }
        }
sendfail:
        sendfailmsg(socket->fd, forward_err);
        return 0;
    }
    else if(!strncmp(service,"get-serialno",strlen("get-serialno"))) {
       if (t) {
           sendokmsg(socket->fd, t->serial);
        }
       else {
           sendokmsg(socket->fd, "target not exist");
       }
        return 0;
    }
    else if(!strncmp(service,"get-state",strlen("get-state"))) {
        const char *state = connection_state_name(t);
        sendokmsg(socket->fd, state);
        return 0;
    }
#ifdef MAKE_DEBUG
    else if(!strncmp(service, "send-packet", strlen("send-packet"))) {
        char data[MAX_PAYLOAD] = {'1', };
        send_cmd(0, 0, 0x00000000, data, t);
        sendokmsg(socket->fd, "send_packet OK!");
        return 0;
    }
    else if(!strncmp(service, "transport-close", strlen("transport-close"))) {
        if(!sdb_close(t->sfd)) {
            sendokmsg(socket->fd, "transport sfd closed!");
        }
        else {
            sendfailmsg(socket->fd, "fail to close sfd!");
        }
        return 0;
    }
#endif

    //TODO REMOTE_DEVICE_CONNECT block this code until security issue is cleared
#if 0
    else if(!strncmp(service,"_dev_con",strlen("_dev_con"))) {
        if(t == NULL) {
            sendfailmsg(socket->fd, err_str);
            return 0;
        }
        if(assign_remote_connect_socket_lid(socket)) {
            sendfailmsg(socket->fd, "remote connect socket exceeds limit. cannot create remote socket\n");
            local_socket_close(socket);
            return 0;
        }
        socket->transport = t;
        create_remote_connection_socket(socket);

        char* buf = "OKAY";
        writex(socket->fd, buf, strlen(buf));
        return 0;
    }
#endif

    return -1;
}

static int find_transports(char **serial_out, const char *prefix)
{
    int nr = 0; // not found
    char *match = NULL;

    if (!serial_out || !prefix)
        return -1;

    sdb_mutex_lock(&transport_lock, "transport find_transports");

    LIST_NODE* curptr = transport_list;
    while(curptr != NULL) {
        TRANSPORT* t = curptr->data;
        curptr = curptr->next_ptr;
        char* serial = t->serial;
        if (!serial || !serial[0])
            continue;
        if (!strncmp(prefix, serial, strlen(prefix))) {
            match = serial;
            nr++;
        }

        if (nr > 1) {
            match = NULL;
            break;
        }
    }
    sdb_mutex_unlock(&transport_lock, "transport find_transports");

    if (nr == 1 && match) {
        *serial_out = strdup(match);
    } else if (nr == 0) {
        asprintf(serial_out, "device not found");
    } else if (nr > 1) {
        asprintf(serial_out, "more than one device and emulator");
    }

    return nr;
}

static void unregister_all_tcp_transports()
{
    sdb_mutex_lock(&transport_lock, "transport unregister_all_tcp_transports");

    LIST_NODE* curptr = transport_list;
    while(curptr != NULL) {
        TRANSPORT* t = curptr->data;
        curptr = curptr->next_ptr;
        if (t->type == kTransportConnect) {
            //just kick the transport. transport is destroied by transport thread.
            if(!t->kicked) {
                t->kicked = 1;
                t->kick(t);
            }
        }
    }

    sdb_mutex_unlock(&transport_lock, "transport unregister_all_tcp_transports");
}

static int handle_host_request(char *service, SDB_SOCKET* socket)
{
    LOG_INFO("LS(%X)\n", socket->fd);
    char cmd_buf[1024];
    int cbuf_size = sizeof cmd_buf;

    if (!strncmp(service, "serial-match:", 13)) {
        char *tmp = service + 13;
        char *serial = NULL;
        int ret = -1;
        D("Try to find device for: %s\n", tmp);
        ret = find_transports(&serial, tmp);
        if (ret <= 0) {
            LOG_ERROR("No device found\n");
            sendfailmsg(socket->fd, serial);
        } else if (ret == 1) {
            sendokmsg(socket->fd, serial);
        } else {
            LOG_ERROR("found more than one devices matched: %d\n", ret);
            sendfailmsg(socket->fd, serial);
        }
        free(serial);
        return 0;
    }
    // return a list of all devices
    if (!strcmp(service, "devices")) {
        list_targets(cmd_buf, cbuf_size, kTransportAny);
        sendokmsg(socket->fd, cmd_buf);
        return 0;
    }

    // return a list of all remote emulator
    if (!strcmp(service, "remote_emul")) {
        list_targets(cmd_buf, cbuf_size, kTransportConnect);
        sendokmsg(socket->fd, cmd_buf);
        return 0;
    }

    // add a new TCP transport, device or emulator
    if (!strncmp(service, "connect:", 8)) {
        char* host = service + 8;
        char* portstr = strchr(host, ':');
        int port = -1;

        if(portstr) {
        	*portstr++ = 0;
        	if(!sscanf(portstr, "%d", &port)) {
        		snprintf(cmd_buf, cbuf_size, "bad port format '%s'", portstr);
        		goto connect_done;
        	}
        }
        connect_emulator(host, port, cmd_buf, cbuf_size);

connect_done:
        sendokmsg(socket->fd, cmd_buf);
        return 0;
    }

    // remove TCP transport
    if (!strncmp(service, "disconnect:", 11)) {
        char* serial = service + 11;
        if (serial[0]) {
            if (!strchr(serial, ':')) {
                snprintf(cmd_buf, cbuf_size, "%s:26101", serial);
                serial = cmd_buf;
            }
            TRANSPORT *t = acquire_one_transport(kTransportAny, serial, NULL);

            if (t) {
                cmd_buf[0] = '\0';
                kick_transport(t);
            } else {
                snprintf(cmd_buf, cbuf_size, "No such device %s", serial);
            }
        } else {
            unregister_all_tcp_transports();
            cmd_buf[0] = '\0';
        }

        sendokmsg(socket->fd, cmd_buf);
        return 0;
    }

    if (!strncmp(service, "device_con:", 11)) {
        char* _host = service + 11;
        char host_buf[4096];
        char target_buf[4096];
        char full_cmd[4096];
        char full_serial[256];
        strncpy(host_buf, _host, sizeof(host_buf) - 1);
        _host = host_buf;
        char* serial = strchr(host_buf, ':');

        if(serial == NULL) {
            sendfailmsg(socket->fd, "serial number is NULL. cannot find the target device\n");
            return 0;
        }
        *(serial) = '\0';
        serial++;

        int fd = sdb_host_connect(_host, DEFAULT_SDB_PORT, SOCK_STREAM);
        if (fd < 0) {
            snprintf(target_buf, sizeof(target_buf), "fail to connect with '%s'", _host);
            LOG_ERROR(target_buf);
            sendfailmsg(socket->fd, target_buf);
            return 0;
        }
        D("FD(%d) remote connected with host: %s\n", fd, _host);

        D("FULL_CMD %s\n", full_cmd);
        snprintf(full_cmd, sizeof(full_cmd), "host:serial-match:%s", serial);
        if(!send_service_with_length(fd, full_cmd, socket->fd)) {
            if(!sdb_status(fd, socket->fd)) {
                int n = read_msg_size(fd);
                if(n > 0 && n < 256) {
                    if(!readx(fd, full_serial, n)) {
                        full_serial[n] = 0;
                        serial = full_serial;
                        goto success;
                    }
                }
                snprintf(target_buf, sizeof(target_buf), "fail to read full serial of %s", serial);
                sendfailmsg(socket->fd, target_buf);
            }
        }
        sdb_close(fd);
        return 0;

success:
        sdb_close(fd);
        fd = sdb_host_connect(_host, DEFAULT_SDB_PORT, SOCK_STREAM);
        if (fd < 0) {
            snprintf(target_buf, sizeof(target_buf), "fail to connect with '%s'", _host);
            LOG_ERROR(target_buf);
            sendfailmsg(socket->fd, target_buf);
            return 0;
        }
        D("FD(%d) remote connected\n", fd);
        get_host_prefix(target_buf, sizeof target_buf, kTransportAny, serial, host);
        snprintf(full_cmd, sizeof full_cmd, "%s_dev_con",target_buf);

        D("FULL_CMD: %s\n", full_cmd);
        if(!send_service_with_length(fd, full_cmd, socket->fd)) {
            if(!sdb_status(fd, socket->fd)) {
                if(!register_device_con_transport(fd, serial)) {
                    snprintf(target_buf, sizeof target_buf, "success to connect with remote target '%s'\n", serial);
                    snprintf(full_cmd, sizeof(full_cmd), "OKAY%04x%s",(unsigned)strlen(target_buf), target_buf);
                    if(!writex(socket->fd, full_cmd, strlen(full_cmd))) {
                        return 0;
                    }
                    else {
                        sendfailmsg(socket->fd, "fail to write OKAY message\n");
                    }
                }
                else {
                    sendfailmsg(socket->fd, "fail to connect with remote device\n");
                }
            }
        }
        sdb_close(fd);
        return 0;
    }

    // returns our value for SDB_VERSION_PATCH
    if (!strcmp(service, "version")) {
        char ver[SDB_VERSION_MAX_LENGTH-8] = {0,};
        char buf[SDB_VERSION_MAX_LENGTH] = {0,};

        snprintf(ver, sizeof(ver), "%d.%d.%d", SDB_VERSION_MAJOR, SDB_VERSION_MINOR, SDB_VERSION_PATCH);
        snprintf(buf, sizeof(buf), "OKAY%04x%s", strlen(ver), ver);
        writex(socket->fd, buf, strlen(buf));
        return 0;
    }

    // indicates a new emulator instance has started
       if (!strncmp(service,"emulator:",9)) { /* tizen specific */
           D("new emulator is in\n");
           char *tmp = strtok(service+9, DEVICEMAP_SEPARATOR);
           int  port = 0;

           if (tmp == NULL) {
               port = atoi(service+9);
           } else {
               port = atoi(tmp);
               tmp = strtok(NULL, DEVICEMAP_SEPARATOR);
           }
           local_connect(port, tmp);
        return 0;
    }

   if(!strcmp(service, "kill")) {
       LOG_INFO("sdb is being killed\n");
       sdb_cleanup();
       sdb_write(socket->fd, "OKAY", 4);
       exit(0);
   }

    return -1;
}

static int smart_socket_check(SDB_SOCKET *s, PACKET **p) {
    unsigned len;

    if(s->pkt_list == NULL) {
        prepend(&s->pkt_list, *p);
    }
    else {
        PACKET* socket_packet = s->pkt_list->data;
        if((socket_packet->len + (*p)->len) > MAX_PAYLOAD) {
            LOG_ERROR("LS(%x): overflow\n", s->local_id);
            put_apacket(*p);
            return -1;
        }

        memcpy(socket_packet->data + socket_packet->len,
                (*p)->data, (*p)->len);
        socket_packet->len += (*p)->len;
        put_apacket(*p);

        *p = socket_packet;
    }

        /* don't bother if we can't decode the length */
    if((*p)->len < 4) {
        LOG_INFO("LS(%X): waiting for more bytes for getting the packet length\n", s->local_id);
        return 1;
    }

    len = unhex((*p)->data, 4);

    if((len < 1) ||  (len > 1024)) {
        LOG_ERROR("LS(%X): bad size (%d)\n", s->local_id, len);
        return -1;
    }

        /* can't do anything until we have the full header */
    if((len + 4) > (*p)->len) {
        LOG_INFO("LS(%X): waiting for %d more bytes in smart socket\n", s->local_id, len+4 - (*p)->len);
        return 1;
    }

    (*p)->data[len + 4] = 0;
    LOG_INFO("LS(%X) %s\n", s->local_id, (*p)->data + 4);
    return 0;
}

static int qemu_socket_enqueue(SDB_SOCKET *s, PACKET *p)
{
    LOG_INFO("LS(%X) data %s\n", s->local_id, p->data);

    //TODO sync command is not fully implemented.
    int result = smart_socket_check(s, &p);

    if(result == -1) {
        goto fail;
    }

    if(result == 1) {
        return result;
    }

    //TODO sync command is not fully implemented.
    char* host_str = (char *)p->data + 4;

    if(strncmp(host_str, "host:", strlen("host:"))) {
        LOG_ERROR("unknown qemu protocol '%s'\n", host_str);
        goto fail;
    }

    host_str = host_str + 5;

    LOG_INFO("qemu request: '%s'\n", host_str);
    if(!strncmp(host_str, "sync:", strlen("sync:"))) {
        host_str += strlen("sync:");
        char* suspend = strchr(host_str, ':');
        if(suspend == NULL) {
            LOG_ERROR("sync: does not contain suspended status!\n");
            goto fail;
        }
        *(suspend++) = '\0';

        TRANSPORT* t = acquire_one_transport(kTransportAny, host_str, NULL);

        if(t == NULL) {
            LOG_ERROR("sync: error. No Such serial name '%s'\n", host_str);
            goto fail;
        }

        if(*suspend == '0') {
            LOG_INFO("T(%s) exits suspended mode\n", t->serial);
            if(t->suspended != 0) {
                t->suspended = 0;
                update_transports();
            }
        }
        else if(*suspend == '1') {
            LOG_INFO("T(%s) enters suspended mode\n", t->serial);
            if(t->suspended != 1) {
                t->suspended = 1;
                run_transport_close(t);
                update_transports();
            }
        }
        else {
            LOG_ERROR("sync: contains wrong suspended status '%c'!\n", suspend);
            goto fail;
        }
    }

    fail:
        free_list(s->pkt_list, put_apacket);
        s->pkt_list = NULL;
        local_socket_close(s);
        return -1;
}

static int smart_socket_enqueue(SDB_SOCKET *s, PACKET *p)
{
    LOG_INFO("LS(%X)\n", s->local_id);
    int result = smart_socket_check(s, &p);

    if(result == -1) {
        goto fail;
    }

    if(result == 1) {
        return result;
    }

    char* host_str = (char *)p->data + 4;
    char *service = NULL;
    char* err_str = NULL;
    TRANSPORT* t = NULL;
    if(parse_host_service(host_str, &service, &t, &err_str) == 1) {
        if (t && t->connection_state != CS_OFFLINE && t->suspended == 0) {
            s->transport = t;
            sdb_write(s->fd, "OKAY", 4);
            D("LS(%X) get transport T(%s)", s->local_id, t->serial);
        } else {
            if(t != NULL) {
                if(t->suspended) {
                    err_str =(char*)ERR_TRANSPORT_TARGET_SUSPENDED;
                }
                else {
                    err_str = (char*)ERR_TRANSPORT_TARGET_OFFLINE;
                }
            }
            LOG_ERROR("LS(%X) get no transport", s->local_id);
            sendfailmsg(s->fd, err_str);
        }
        p->len = 0;
        return 0;
    }
    if (service) {
        if(handle_request_with_t(s, service, t, err_str) == 0) {
            D( "LS(%X): handled host service with '%s'\n", s->local_id, service );
            goto fail;
        }

        if(handle_host_request(service, s) == 0) {
                /* XXX fail message? */
            D( "LS(%X): handled host service '%s'\n", s->local_id, service );
            goto fail;
        }

        if (!strcmp(service,"track-devices")) {
            free_list(s->pkt_list, put_apacket);
            s->pkt_list = NULL;
            D("LS(%X): OKAY\n", s->local_id);
            SET_SOCKET_STATUS(s, DEVICE_TRACKER);
            sdb_write(s->fd, "OKAY", 4);

            D( "LS(%X) fd: '%d' for device tracking\n", s->local_id, s->fd );
            char buffer[1024];
            int len;
            len = list_transports_msg(buffer, sizeof(buffer));
            device_tracker_send(s, buffer, len);
            return 0;
        }
        else {
            D( "LS(%X): couldn't create host service '%s'\n", s->local_id, service );
            sendfailmsg(s->fd, "unknown host service");
            goto fail;
        }
    }
    if(!(s->transport) || (s->transport->connection_state == CS_OFFLINE)) {
        sendfailmsg(s->fd, "device offline (x)");
        goto fail;
    }

    if (!(s->transport) || (s->transport->connection_state == CS_PWLOCK)) {
        sendfailmsg(s->fd, "device locked");
        goto fail;
    }

    //TODO REMOTE_DEVICE_CONNECT
#if 0
    if(s->transport->type == kTransportRemoteDevCon) {
        if(assign_remote_connect_socket_rid(s)) {
            sendfailmsg(s->fd, "remote connect socket exceeds limit. cannot create remote socket\n");
            local_socket_close(s);
            return -1;
        }
    }
#endif

    SET_SOCKET_STATUS(s, NOTIFY);

    connect_to_remote(s, (char*) (p->data + 4));
    free_list(s->pkt_list, put_apacket);
    s->pkt_list = NULL;
    FDEVENT_DEL(&s->fde, FDE_READ);
    return 0;

fail:

    free_list(s->pkt_list, put_apacket);
    s->pkt_list = NULL;
#if 0 //REMOTE_DEVICE_CONNECT
    if(!HAS_SOCKET_STATUS(s, REMOTE_CON))
#endif
        local_socket_close(s);
    return -1;
}

//TODO REMOTE_DEVICE_CONNECT
//int assign_remote_connect_socket_rid (SDB_SOCKET* s) {
//    if(remote_con_cur_r_id > remote_con_r_max) {
//        LOG_ERROR("remote connect socket exceeds limit. cannot create remote socket for LS(%X) FD(%d)\n", s->local_id, s->fd);
//        return -1;
//    }
//    int remote_id = (remote_con_cur_r_id << 4) | remote_con_flag;
//    LOG_INFO("LS(%X) -> LS_R(%X)\n", s->local_id, remote_id);
//    s->local_id = remote_id;
//    remote_con_cur_r_id++;
//    return 0;
//}

//int assign_remote_connect_socket_lid (SDB_SOCKET* s) {
//    if(remote_con_cur_l_number >= remote_con_l_max) {
//        LOG_ERROR("remote connect socket exceeds limit. cannot create remote socket for LS(%X) FD(%d)\n", s->local_id, s->fd);
//        return -1;
//    }
//    int i = 0;
//    for(i = 0; i< remote_con_l_max; i++) {
//        if(remote_con_l_table[i] == 0) {
//            unsigned int remote_id = i | remote_con_flag;
//            remote_con_cur_l_number++;
//            LOG_INFO("LS(%X) -> LS_L(%X)\n", s->local_id, remote_id);
//            s->local_id = remote_id;
//            remote_con_l_table[i] = 1;
//            return 0;
//        }
//    }
//    LOG_ERROR("not enough space in remote_con_l_table\n");
//    return -1;
//}

int
device_tracker_send( SDB_SOCKET* local_socket,
                     const char*      buffer,
                     int              len )
{
    D("device tracker send to the socket. LS(%X), fd: '%d'\n", local_socket->local_id, local_socket->fd);
    PACKET*  p = get_apacket();

    memcpy(p->data, buffer, len);
    p->len = len;
    p->ptr = p->data;
    //packet should not be freed after local_socket_enqueue because it can be still used in local socket packet queue.
    return local_socket_enqueue( local_socket, p );
}

int notify_qemu(char* host, int port, char* serial) {
    int  fd = -1;
    int  qemu_port = port + 2;

    fd = sdb_host_connect(host, qemu_port, SOCK_DGRAM);

    if (fd < 0) {
        LOG_ERROR("failed to create socket to localhost(%d)\n", qemu_port);
        return -1;
    }

    char request[255];
    snprintf(request, sizeof request, "5\n%s\n", serial);

    // send to sensord with udp

    LOG_INFO("notify qemu: %s\n", request);

    if (sdb_write(fd, request, strlen(request)) < 0) {
        LOG_ERROR("could not send sensord request\n");
        sdb_close(fd);
        return -1;
    }

    sdb_close(fd);
    return 0;
}

static void connect_emulator(char* host, int port, char* buf, int buf_len) {
    if(port < 0) {
        port = DEFAULT_SDB_LOCAL_TRANSPORT_PORT;
    }
    LOG_INFO("connecting ip: '%s', port: '%d'\n", host, port);

    int fd = sdb_host_connect(host, port, SOCK_STREAM);
    if (fd < 0) {
        snprintf(buf, buf_len, "fail to connect to %s", host);
        return;
    }

    LOG_INFO("FD(%d) connected\n", fd);
    close_on_exec(fd);
    disable_tcp_nagle(fd);
    char serial[100];
    snprintf(serial, sizeof(serial), "%s:%d", host, port);

    if (acquire_one_transport(kTransportAny, serial, NULL)) {
        snprintf(buf, buf_len, "%s is already connected", serial);
        return;
    }

    if(notify_qemu(host, port, serial)) {
        return;
    }
    register_socket_transport(fd, serial, host, port, kTransportConnect, NULL);

    snprintf(buf, buf_len, "connected to %s", serial);
}
