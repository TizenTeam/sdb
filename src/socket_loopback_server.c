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
// libs/cutils/socket_loopback_server.c

#include "sockets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>

#define LISTEN_BACKLOG 4
#define LOOPBACK_UP 1
#define LOOPBACK_DOWN 0

#ifndef HAVE_WINSOCK
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#endif
#include "sysdeps.h"

int get_loopback_status(void) {

	int           s;
	struct ifconf ifc;
	struct ifreq *ifr;
	int           ifcnt;
	char          buf[1024];
	int i;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if(s < 0)
	{
		perror("socket");
		return LOOPBACK_DOWN;
	}

	// query available interfaces
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if(ioctl(s, SIOCGIFCONF, &ifc) < 0)
	{
		perror("ioctl(SIOCGIFCONF)");
		return LOOPBACK_DOWN;
	}

	// iterate the list of interfaces
	ifr = ifc.ifc_req;
	ifcnt = ifc.ifc_len / sizeof(struct ifreq);
	for(i = 0; i < ifcnt; i++)
	{
		struct sockaddr_in *addr;
		addr = (struct sockaddr_in *)&ifr->ifr_addr;

		if (ntohl(addr->sin_addr.s_addr) == INADDR_LOOPBACK)
		{
			return LOOPBACK_UP;
		}
	}
	return LOOPBACK_DOWN;
}

/* open listen() port on loopback interface */
int socket_loopback_server(int port, int type)
{
    struct sockaddr_in addr;
    int s, n;
    int cnt_max = 30;

    /* tizen specific */
#if !SDB_HOST
    // check the loopback interface has been up in 30 sec
    while(cnt_max > 0) {
        if(get_loopback_status() == LOOPBACK_DOWN) {
            cnt_max--;
            sdb_sleep_ms(1000);
        }
        else {
            break;
        }
    }
#endif
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if(cnt_max ==0)
    	addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
    	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    s = socket(AF_INET, type, 0);
    if(s < 0) return -1;

    n = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));



    if(bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        sdb_close(s);
        return -1;
    }

    if (type == SOCK_STREAM) {
        int ret;

        ret = listen(s, LISTEN_BACKLOG);

        if (ret < 0) {
            sdb_close(s);
            return -1; 
        }
    }

    return s;
}

