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

#include "log.h"
#include "listener.h"
#include "fdevent.h"
#include "utils.h"
#include "sockets.h"

LIST_NODE* listener_list = NULL;

static void listener_event_func(int _fd, unsigned ev, void *_l);
static LISTENER* find_listener(const char *local_name);

void  free_listener(void* data)
{
    LISTENER* listener = data;
    fdevent_remove(&(listener->fde));
    free((void*)listener->local_name);
    free((void*)listener->connect_to);
    free(listener);
}

int install_listener(const char *local_name, const char *connect_to, TRANSPORT* transport)
{
    D("LN(%s)\n", local_name);

    LISTENER* listener = find_listener(local_name);

    if(listener != NULL) {
        char *cto;

            /* can't repurpose a smartsocket */
        if(listener->connect_to[0] == '*') {
            return -1;
        }

        cto = strdup(connect_to);
        if(cto == 0) {
            return -1;
        }

        //printf("rebinding '%s' to '%s'\n", local_name, connect_to);
        free((void*) listener->connect_to);
        listener->connect_to = cto;
        if (listener->transport != transport) {
            listener->transport = transport;
        }
        return 0;
    }

    if(strncmp("tcp:", local_name, 4)){
        LOG_FATAL("LN(%s) unknown local portname\n", local_name);
        return -2;
    }

    int port = atoi(local_name + 4);

    //TODO REMOTE_DEVICE_CONNECT block remote connect until security issue is cleard
//    int fd = sdb_port_listen(INADDR_ANY, port, SOCK_STREAM);
    int fd = sdb_port_listen(INADDR_LOOPBACK, port, SOCK_STREAM);

    if(fd < 0) {
        LOG_FATAL("LN(%s) cannot bind\n", local_name);
        return -2;
    }

    listener = calloc(1, sizeof(LISTENER));
    listener->local_name = strdup(local_name);
    listener->connect_to = strdup(connect_to);
    listener->fd = fd;
    listener->node = prepend(&listener_list, listener);
    listener->transport = transport;
    close_on_exec(fd);
    fdevent_install(&listener->fde, fd, listener_event_func, listener);
    FDEVENT_SET(&listener->fde, FDE_READ);
    return 0;
}

int remove_listener(const char *local_name, const char *connect_to, TRANSPORT* transport)
{
    D("LN(%s)\n", local_name);
    LISTENER* listener = find_listener(local_name);

    if(listener != NULL &&
            !strcmp(connect_to, listener->connect_to) &&
            listener->transport != NULL &&
            listener->transport == transport) {
        remove_node(&listener_list, listener->node, free_listener);
        D("LN(%s) removed\n", local_name);
        return 0;
    }

    D("LN(%s) could not find\n", local_name);
    return -1;
}

static void listener_event_func(int _fd, unsigned ev, void *_l)
{
    LISTENER *l = _l;
    D("LN(%s)\n", l->local_name);

    if(ev & FDE_READ) {
        int fd = sdb_socket_accept(_fd);

        if(fd < 0) {
            D("LN(%s) fail to create\n", l->local_name);
            return;
        }

        SDB_SOCKET *s = create_local_socket(fd);

        int ss = 0;
        if(!strcmp(l->connect_to, "*smartsocket*")) {
            ss = 1;
        }

        if(ss) {
            sdb_socket_setbufsize(fd, CHUNK_SIZE);
        }
        if(s) {

            if(ss) {
                local_socket_ready(s);
            }
            else {

                if(l->transport->type == kTransportRemoteDevCon) {
                    if(assign_remote_connect_socket_rid(s)) {
                        local_socket_close(s);
                        return;
                    }
                }

                s->transport = l->transport;
                connect_to_remote(s, l->connect_to);
            }
            return;
        }

        sdb_close(fd);
    }
}

static LISTENER* find_listener(const char *local_name) {
    LIST_NODE* currentptr = listener_list;
    while(currentptr != NULL) {
        LISTENER* l = currentptr->data;
        currentptr = currentptr->next_ptr;
        if(!strcmp(local_name, l->local_name)) {
            return l;
        }
    }
    return NULL;
}
