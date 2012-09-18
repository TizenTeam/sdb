#ifndef _SDB_CLIENT_H_
#define _SDB_CLIENT_H_

#include "sdb.h"

/* connect to sdb, connect to the named service, and return
** a valid fd for interacting with that service upon success
** or a negative number on failure
*/
int sdb_connect(const char *service);
int _sdb_connect(const char *service);

/* connect to sdb, connect to the named service, return 0 if
** the connection succeeded AND the service returned OKAY
*/
int sdb_command(const char *service);

/* connect to sdb, connect to the named service, return
** a malloc'd string of its response upon success or NULL
** on failure.
*/
char *sdb_query(const char *service);

/* Set the preferred transport to connect to.
*/
void sdb_set_transport(transport_type type, const char* serial);

/* Set TCP specifics of the transport to use
*/
void sdb_set_tcp_specifics(int server_port);

/* Return the console port of the currently connected emulator (if any)
 * of -1 if there is no emulator, and -2 if there is more than one.
 * assumes sdb_set_transport() was alled previously...
 */
int  sdb_get_emulator_console_port(void);

/* send commands to the current emulator instance. will fail if there
 * is zero, or more than one emulator connected (or if you use -s <serial>
 * with a <serial> that does not designate an emulator)
 */
int  sdb_send_emulator_command(int  argc, char**  argv);

/* return verbose error string from last operation */
const char *sdb_error(void);

/* read a standard sdb status response (OKAY|FAIL) and
** return 0 in the event of OKAY, -1 in the event of FAIL
** or protocol error
*/
int sdb_status(int fd);

#endif
