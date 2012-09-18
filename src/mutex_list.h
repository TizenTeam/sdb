/* the list of mutexes used by addb */
#ifndef SDB_MUTEX
#error SDB_MUTEX not defined when including this file
#endif

SDB_MUTEX(dns_lock)
SDB_MUTEX(socket_list_lock)
SDB_MUTEX(transport_lock)
#if SDB_HOST
SDB_MUTEX(local_transports_lock)
#endif
SDB_MUTEX(usb_lock)

#undef SDB_MUTEX
