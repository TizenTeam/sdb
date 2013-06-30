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

#ifndef __SDB_H
#define __SDB_H

#include <limits.h>

#include "transport.h"  /* readx(), writex() */
#include "sdb_usb.h"

#define MAX_PAYLOAD 4096

#define A_SYNC 0x434e5953
#define A_CNXN 0x4e584e43
#define A_OPEN 0x4e45504f
#define A_OKAY 0x59414b4f
#define A_CLSE 0x45534c43
#define A_WRTE 0x45545257

#define A_VERSION 0x01000000        // SDB protocol version

#define SDB_VERSION_MAJOR 2       // Used for help/version information
#define SDB_VERSION_MINOR 2         // Used for help/version information

#define SDB_SERVER_VERSION    5    // Increment this when we want to force users to start a new sdb server

typedef struct amessage amessage;
typedef struct apacket apacket;
typedef struct asocket asocket;
typedef struct alistener alistener;
typedef struct aservice aservice;
typedef struct atransport atransport;
typedef struct adisconnect  adisconnect;

struct amessage {
    unsigned command;       /* command identifier constant      */
    unsigned arg0;          /* first argument                   */
    unsigned arg1;          /* second argument                  */
    unsigned data_length;   /* length of payload (0 is allowed) */
    unsigned data_check;    /* checksum of data payload         */
    unsigned magic;         /* command ^ 0xffffffff             */
};

struct apacket
{
    apacket *next;

    unsigned len;
    unsigned char *ptr;

    amessage msg;
    unsigned char data[MAX_PAYLOAD];
};

/* An asocket represents one half of a connection between a local and
** remote entity.  A local asocket is bound to a file descriptor.  A
** remote asocket is bound to the protocol engine.
*/
struct asocket {
        /* chain pointers for the local/remote list of
        ** asockets that this asocket lives in
        */
    asocket *next;
    asocket *prev;

        /* the unique identifier for this asocket
        */
    unsigned id;

        /* flag: set when the socket's peer has closed
        ** but packets are still queued for delivery
        */
    int    closing;

        /* flag: quit sdbd when both ends close the
        ** local service socket
        */
    int    exit_on_close;

        /* the asocket we are connected to
        */

    asocket *peer;

        /* For local asockets, the fde is used to bind
        ** us to our fd event system.  For remote asockets
        ** these fields are not used.
        */
    fdevent fde;
    int fd;

        /* queue of apackets waiting to be written
        */
    apacket *pkt_first;
    apacket *pkt_last;

        /* enqueue is called by our peer when it has data
        ** for us.  It should return 0 if we can accept more
        ** data or 1 if not.  If we return 1, we must call
        ** peer->ready() when we once again are ready to
        ** receive data.
        */
    int (*enqueue)(asocket *s, apacket *pkt);

        /* ready is called by the peer when it is ready for
        ** us to send data via enqueue again
        */
    void (*ready)(asocket *s);

        /* close is called by the peer when it has gone away.
        ** we are not allowed to make any further calls on the
        ** peer once our close method is called.
        */
    void (*close)(asocket *s);

        /* socket-type-specific extradata */
    void *extra;

    	/* A socket is bound to atransport */
    atransport *transport;
};


/* the adisconnect structure is used to record a callback that
** will be called whenever a transport is disconnected (e.g. by the user)
** this should be used to cleanup objects that depend on the
** transport (e.g. remote sockets, listeners, etc...)
*/
struct  adisconnect
{
    void        (*func)(void*  opaque, atransport*  t);
    void*         opaque;
    adisconnect*  next;
    adisconnect*  prev;
};


/* a transport object models the connection to a remote device or emulator
** there is one transport per connected device/emulator. a "local transport"
** connects through TCP (for the emulator), while a "usb transport" through
** USB (for real devices)
**
** note that kTransportHost doesn't really correspond to a real transport
** object, it's a special value used to indicate that a client wants to
** connect to a service implemented within the SDB server itself.
*/
typedef enum transport_type {
        kTransportUsb,
        kTransportLocal,
        kTransportAny,
        kTransportHost,
} transport_type;

struct atransport
{
    atransport *next;
    atransport *prev;

    int (*read_from_remote)(apacket *p, atransport *t);
    int (*write_to_remote)(apacket *p, atransport *t);
    void (*close)(atransport *t);
    void (*kick)(atransport *t);

    int fd;
    int transport_socket;
    fdevent transport_fde;
    int ref_count;
    unsigned sync_token;
    int connection_state;
    transport_type type;

        /* usb handle or socket fd as needed */
    usb_handle *usb;
    int sfd;

        /* used to identify transports for clients */
    char *serial;
    char *product;
    int sdb_port; // Use for emulators (local transport)
    char *device_name; // for connection explorer

        /* a list of adisconnect callbacks called when the transport is kicked */
    int          kicked;
    adisconnect  disconnects;
};


/* A listener is an entity which binds to a local port
** and, upon receiving a connection on that port, creates
** an asocket to connect the new local connection to a
** specific remote service.
**
** TODO: some listeners read from the new connection to
** determine what exact service to connect to on the far
** side.
*/
struct alistener
{
    alistener *next;
    alistener *prev;

    fdevent fde;
    int fd;

    const char *local_name;
    const char *connect_to;
    atransport *transport;
    adisconnect  disconnect;
};


void print_packet(const char *label, apacket *p);

asocket *find_local_socket(unsigned id);
void install_local_socket(asocket *s);
void remove_socket(asocket *s);
void close_all_sockets(atransport *t);

#define  LOCAL_CLIENT_PREFIX  "emulator-"

asocket *create_local_socket(int fd);
asocket *create_local_service_socket(const char *destination);

asocket *create_remote_socket(unsigned id, atransport *t);
void connect_to_remote(asocket *s, const char *destination);
void connect_to_smartsocket(asocket *s);

void fatal(const char *fmt, ...);
void fatal_errno(const char *fmt, ...);

void handle_packet(apacket *p, atransport *t);
void send_packet(apacket *p, atransport *t);

int sdb_main(int is_daemon, int server_port);


/* transports are ref-counted
** get_device_transport does an acquire on your behalf before returning
*/
void init_transport_registration(void);
int find_transports(char **serial, const char *prefix);
int  list_transports(char *buf, size_t  bufsize);
void update_transports(void);

asocket*  create_device_tracker(void);

/* Obtain a transport from the available transports.
** If state is != CS_ANY, only transports in that state are considered.
** If serial is non-NULL then only the device with that serial will be chosen.
** If no suitable transport is found, error is set.
*/
atransport *acquire_one_transport(int state, transport_type ttype, const char* serial, char **error_out);
void   add_transport_disconnect( atransport*  t, adisconnect*  dis );
void   remove_transport_disconnect( atransport*  t, adisconnect*  dis );
void   run_transport_disconnects( atransport*  t );
void   kick_transport( atransport*  t );

/* initialize a transport object's func pointers and state */
int get_available_local_transport_index();
int  init_socket_transport(atransport *t, int s, int port, int local);
void init_usb_transport(atransport *t, usb_handle *usb, int state);
void close_usb_devices();


/* cause new transports to be init'd and added to the list */
void register_socket_transport(int s, const char *serial, int port, int local, const char *device_name);

/* these should only be used for the "sdb disconnect" command */
void unregister_transport(atransport *t);
void unregister_all_tcp_transports();

void register_usb_transport(usb_handle *h, const char *serial, unsigned writeable);

/* this should only be used for transports with connection_state == CS_NOPERM */
void unregister_usb_transport(usb_handle *usb);

atransport *find_transport(const char *serial);
atransport* find_emulator_transport_by_sdb_port(int sdb_port);

int service_to_fd(const char *name);
asocket *host_service_to_socket(const char*  name, const char *serial);

/* packet allocator */
apacket *get_apacket(void);
void put_apacket(apacket *p);

int check_header(apacket *p);
int check_data(apacket *p);

/* define SDB_TRACE to 1 to enable tracing support, or 0 to disable it */

#define  SDB_TRACE    1

/* IMPORTANT: if you change the following list, don't
 * forget to update the corresponding 'tags' table in
 * the sdb_trace_init() function implemented in sdb.c
 */
typedef enum {
    TRACE_SDB = 0,
    TRACE_SOCKETS,
    TRACE_PACKETS,
    TRACE_TRANSPORT,
    TRACE_RWX,
    TRACE_USB,
    TRACE_SYNC,
    TRACE_SYSDEPS,
    TRACE_JDWP,
    TRACE_SERVICES,
    TRACE_PROPERTIES
} SdbTrace;

#if SDB_TRACE

#define DQ(...) ((void)0)


  extern int     sdb_trace_mask;
  extern unsigned char    sdb_trace_output_count;
  void    sdb_trace_init(void);

#  define SDB_TRACING  ((sdb_trace_mask & (1 << TRACE_TAG)) != 0)

  /* you must define TRACE_TAG before using this macro */
#  define  D(...)                                      \
        do {                                           \
            if (SDB_TRACING) {                         \
                int save_errno = errno;                \
                sdb_mutex_lock(&D_lock);               \
                fprintf(stderr, "%s::%s():",           \
                        __FILE__, __FUNCTION__);       \
                errno = save_errno;                    \
                fprintf(stderr, __VA_ARGS__ );         \
                fflush(stderr);                        \
                sdb_mutex_unlock(&D_lock);             \
                errno = save_errno;                    \
           }                                           \
        } while (0)
#  define  DR(...)                                     \
        do {                                           \
            if (SDB_TRACING) {                         \
                int save_errno = errno;                \
                sdb_mutex_lock(&D_lock);               \
                errno = save_errno;                    \
                fprintf(stderr, __VA_ARGS__ );         \
                fflush(stderr);                        \
                sdb_mutex_unlock(&D_lock);             \
                errno = save_errno;                    \
           }                                           \
        } while (0)
#else
#  define  D(...)          ((void)0)
#  define  DR(...)         ((void)0)
#  define  SDB_TRACING     0
#endif


#if !TRACE_PACKETS
#define print_packet(tag,p) do {} while (0)
#endif


/* sdb and sdbd are coexisting on the target, so use 26099 for sdb
 * to avoid conflicting with sdbd's usage of 26098
 */
#define DEFAULT_SDB_PORT 26099 /* tizen specific */
#define DEFAULT_SDB_LOCAL_TRANSPORT_PORT 26101 /* tizen specific */

void local_init(int port);
int  local_connect(int  port, const char *device_name);
int  local_connect_arbitrary_ports(int console_port, int sdb_port, const char *device_name);


unsigned host_to_le32(unsigned n);
int sdb_commandline(int argc, char **argv);

int connection_state(atransport *t);

#define CS_ANY       -1
#define CS_OFFLINE    0
#define CS_BOOTLOADER 1
#define CS_DEVICE     2
#define CS_HOST       3
#define CS_RECOVERY   4
#define CS_NOPERM     5 /* Insufficient permissions to communicate with the device */
#define CS_SIDELOAD   6

extern int HOST;
extern int SHELL_EXIT_NOTIFY_FD;

#define CHUNK_SIZE (64*1024)

int sendfailmsg(int fd, const char *reason);
int handle_host_request(char *service, transport_type ttype, char* serial, int reply_fd, asocket *s);

void register_device_name(const char *device_type, const char *device_name, int port);
int get_devicename_from_shdmem(int port, char *device_name);
#endif
