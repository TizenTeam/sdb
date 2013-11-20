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
#include <string.h>

#include "log.h"
#include "fdevent.h"
#include "utils.h"
#define  TRACE_TAG  TRACE_TRANSPORT
#include "sdb_usb.h"
#include "transport.h"


static int remote_read(TRANSPORT* t, void* data, int len)
{
	return sdb_usb_read(t->usb, data, len);
}

static int remote_write(PACKET *p, TRANSPORT *t)
{
    dump_packet(t->serial, "remote_write_usb", p);
    if(sdb_usb_write(t->usb, &p->msg, sizeof(MESSAGE))) {
        LOG_ERROR("mesage write error\n");
        return -1;
    }

    if(p->msg.data_length != 0) {
		if(sdb_usb_write(t->usb, &p->data, p->msg.data_length)) {
			D("remote usb: 2 - write terminated\n");
			return -1;
		}
    }

    return 0;
}

static void remote_close(TRANSPORT *t)
{
    sdb_usb_close(t->usb);
    t->usb = 0;
}

static void remote_kick(TRANSPORT *t)
{
    sdb_usb_kick(t->usb);
}

static int get_connected_device_count(transport_type type)
{
    int cnt = 0;
    sdb_mutex_lock(&transport_lock, "transport get_connected_device_count");

    LIST_NODE* curptr = transport_list;
    while(curptr != NULL) {
        TRANSPORT *t = curptr->data;
        curptr = curptr->next_ptr;
        if (type == t->type) {
            cnt++;
        }
    }

    sdb_mutex_unlock(&transport_lock, "transport get_connected_device_count");
    D("connected device count:%d\n",cnt);
    return cnt;
}

void register_usb_transport(usb_handle *usb, const char *serial)
{
    TRANSPORT *t = calloc(1, sizeof(TRANSPORT));
    char device_name[256];

    D("transport: %p init'ing for usb_handle %p (sn='%s')\n", t, usb,
      serial ? serial : "");

    t->close = remote_close;
    t->kick = remote_kick;
    t->read_from_remote = remote_read;
    t->write_to_remote = remote_write;
    t->connection_state = CS_OFFLINE;
    t->type = kTransportUsb;
    t->usb = usb;
    t->sdb_port = -1;
    t->req = 0;
    t->res = 0;
    t->suspended = 0;

    if(serial) {
        t->serial = strdup(serial);
    }
    t->remote_cnxn_socket = NULL;
    register_transport(t);

    /* tizen specific */
    sprintf(device_name, "device-%d",get_connected_device_count(kTransportUsb));
    t->device_name = strdup(device_name);
}

int is_sdb_interface(int vendor_id, int usb_class, int usb_subclass, int usb_protocol)
{
    /**
     * TODO: find easy way to add usb devices vendors
     */
    if (vendor_id == VENDOR_ID_SAMSUNG && usb_class == SDB_INTERFACE_CLASS && usb_subclass == SDB_INTERFACE_SUBCLASS
            && usb_protocol == SDB_INTERFACE_PROTOCOL) {
        return 1;
    }

    return 0;
}

void close_usb_devices()
{
    sdb_mutex_lock(&transport_lock, "transport close_usb_devices");

    LIST_NODE* curptr = transport_list;
    while(curptr != NULL) {
        TRANSPORT* t = curptr->data;
        curptr = curptr->next_ptr;
        if ( !t->kicked ) {
            t->kicked = 1;
            t->kick(t);
        }
    }

    sdb_mutex_unlock(&transport_lock, "transport close_usb_devices");
}
