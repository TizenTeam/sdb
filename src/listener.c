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

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "log.h"
#include "listener.h"
#include "fdevent.h"
#include "utils.h"
#include "sockets.h"

LIST_NODE* listener_list = NULL;

static void listener_event_func(int _fd, unsigned ev, void *_l);
static LISTENER* find_listener(int local_port);

void  free_listener(void* data)
{
    LISTENER* listener = data;
    fdevent_remove(&(listener->fde));
    free(listener);
}

int install_listener(int local_port, int connect_port, TRANSPORT* transport, LISTENER_TYPE ltype)
{
    D("LN(%d)\n", local_port);

    LISTENER* listener = find_listener(local_port);

    if(listener != NULL) {
        if(listener->type != forwardListener) {
            LOG_ERROR("can not repurpose if it is not forward listener");
            return -1;
        }

        listener->type = ltype;
        listener->connect_port = connect_port;
        listener->transport = transport;
        return 0;
    }

    //TODO REMOTE_DEVICE_CONNECT block remote connect until security issue is cleard
//    int fd = sdb_port_listen(INADDR_ANY, port, SOCK_STREAM);

    int fd = -1;
    if(ltype == qemuListener || ltype == forwardListener || ltype == serverListener) {
        fd = sdb_port_listen(INADDR_ANY, local_port, SOCK_STREAM);
    }
    else {
        fd = sdb_port_listen(INADDR_LOOPBACK, local_port, SOCK_STREAM);
    }

    if(fd < 0) {
        if(ltype == serverListener) {
            LOG_FATAL("server LN(%d) cannot bind \n", local_port);
        }
        else {
            LOG_ERROR("LN(%d) cannot bind \n", local_port);
        }
        return -2;
    }

    listener = calloc(1, sizeof(LISTENER));
    listener->type = ltype;
    listener->local_port = local_port;
    listener->connect_port = connect_port;
    listener->fd = fd;
    listener->node = prepend(&listener_list, listener);
    listener->transport = transport;
    close_on_exec(fd);
    fdevent_install(&listener->fde, fd, listener_event_func, listener);
    FDEVENT_SET(&listener->fde, FDE_READ);
    return 0;
}

//int remove_listener(int local_port, int connect_port, TRANSPORT* transport)
//{
//    LOG_INFO("LN(%d)\n", local_port);
//    LISTENER* listener = find_listener(local_port);
//
//    if(listener != NULL &&
//            connect_port == listener->connect_port &&
//            listener->transport != NULL &&
//            listener->transport == transport) {
//        remove_node(&listener_list, listener->node, free_listener);
//        LOG_INFO("LN(%d) removed\n", local_port);
//        return 0;
//    }
//
//    LOG_ERROR("LN(%d) could not find\n", local_port);
//    return -1;
//}

int remove_listener(int local_port)
{
    LOG_INFO("LN(%d)\n", local_port);
    LISTENER* listener = find_listener(local_port);

    if(listener != NULL && listener->transport != NULL) {
        remove_node(&listener_list, listener->node, free_listener);
        LOG_INFO("LN(%d) removed\n", local_port);
        return 0;
    }

    LOG_ERROR("LN(%d) could not find\n", local_port);
    return -1;
}

static void listener_event_func(int _fd, unsigned ev, void *_l)
{
    LISTENER *l = _l;
    LOG_INFO("LN(%d)\n", l->local_port);

    if(ev & FDE_READ) {
        int fd = sdb_socket_accept(_fd);

        if(fd < 0) {
            LOG_ERROR("LN(%d) fail to create socket\n", l->local_port);
            return;
        }

        SDB_SOCKET *s = create_local_socket(fd);

        if(l->type == serverListener) {
            sdb_socket_setbufsize(fd, CHUNK_SIZE);
            local_socket_ready(s);
        }
        else  if(l->type == qemuListener) {
            sdb_socket_setbufsize(fd, CHUNK_SIZE);
            SET_SOCKET_STATUS(s, QEMU_SOCKET);
            local_socket_ready(s);
        }
        else {

//TODO REMOTE_DEVICE_CONNECT
#if 0
            if(l->transport->type == kTransportRemoteDevCon) {
                if(assign_remote_connect_socket_rid(s)) {
                    local_socket_close(s);
                    return;
                }
            }
#endif

            s->transport = l->transport;
            char connect_to[50];
            snprintf(connect_to, sizeof connect_to, "tcp:%d", l->connect_port);

            connect_to_remote(s, connect_to);
        }
    }
}

static LISTENER* find_listener(int local_port) {
    LIST_NODE* currentptr = listener_list;
    while(currentptr != NULL) {
        LISTENER* l = currentptr->data;
        currentptr = currentptr->next_ptr;
        if(local_port == l->local_port) {
            return l;
        }
    }
    return NULL;
}
