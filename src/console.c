#include "sysdeps.h"
#include "sdb.h"
#include "sdb_client.h"
#include <stdio.h>

static int  connect_to_console(void)
{
    int  fd, port;

    port = sdb_get_emulator_console_port();
    if (port < 0) {
        if (port == -2)
            fprintf(stderr, "error: more than one emulator detected. use -s option\n");
        else
            fprintf(stderr, "error: no emulator detected\n");
        return -1;
    }
    fd = socket_loopback_client( port, SOCK_STREAM );
    if (fd < 0) {
        fprintf(stderr, "error: could not connect to TCP port %d\n", port);
        return -1;
    }
    return  fd;
}


int  sdb_send_emulator_command(int  argc, char**  argv)
{
    int   fd, nn;

    fd = connect_to_console();
    if (fd < 0)
        return 1;

#define  QUIT  "quit\n"

    for (nn = 1; nn < argc; nn++) {
        sdb_write( fd, argv[nn], strlen(argv[nn]) );
        sdb_write( fd, (nn == argc-1) ? "\n" : " ", 1 );
    }
    sdb_write( fd, QUIT, sizeof(QUIT)-1 );
    sdb_close(fd);

    return 0;
}
