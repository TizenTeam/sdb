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

#include <windows.h>
#include <winsock2.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include "utils.h"
#include "fdevent.h"
#include "fdevent_backend.h"
#include "sdb_map.h"

#define  TRACE_TAG  TRACE_SYSDEPS
#include "utils_backend.h"
#include "log.h"

// error mapping table between windoes & unix
// error codes are described: http://msdn.microsoft.com/en-us/library/windows/desktop/ms681382%28v=vs.85%29.aspx
void setBaseErrno(DWORD dwLastErrorCode);
int getBaseErrno(DWORD dwLastErrorCode);

static int total_handle_number = 0;
//this increases 1 when one file or socket is created.
static int windows_handle_fd_count = 0;

struct errno_lists {
        unsigned long wincode;
        int posixcode;
};

static struct errno_lists errno_list[] = {
        { 0, 0 },                               /* 0 return 0 as normal operation */
        { ERROR_INVALID_FUNCTION, EINVAL },     /* 1 Incorrect function. */
        { ERROR_FILE_NOT_FOUND, ENOENT },       /* 2 The system cannot find the file specified. */
        { ERROR_PATH_NOT_FOUND, ENOENT },       /* 3 The system cannot find the path specified. */
        { ERROR_TOO_MANY_OPEN_FILES, EMFILE },  /* 4 The system cannot open the file. */
        { ERROR_ACCESS_DENIED, EACCES },        /* 5 Access is denied. */
        { ERROR_INVALID_HANDLE, EBADF },        /* 6 The handle is invalid. */
        { ERROR_ARENA_TRASHED, ENOMEM },        /* 7 The storage control blocks were destroyed.*/
        { ERROR_NOT_ENOUGH_MEMORY, ENOMEM },    /* 8 Not enough storage is available to process this command. */
        { ERROR_INVALID_BLOCK, ENOMEM },        /* 9 he storage control block address is invalid. */
        { ERROR_BAD_ENVIRONMENT, E2BIG },       /* 10 The environment is incorrect. */
        { ERROR_BAD_FORMAT, ENOEXEC },          /* 11 An attempt was made to load a program with an incorrect format. */
        { ERROR_INVALID_ACCESS, EINVAL },       /* 12 The access code is invalid. */
        { ERROR_INVALID_DATA, EINVAL },         /* 13 The data is invalid. */
        { ERROR_OUTOFMEMORY, ENOMEM },          /* 14 Not enough storage is available to complete this operation. */
        { ERROR_INVALID_DRIVE, ENOENT },        /* 15 The system cannot find the drive specified.*/
        { ERROR_CURRENT_DIRECTORY, EACCES },    /* 16 */
        { ERROR_NOT_SAME_DEVICE, EXDEV },       /* 17 */
        { ERROR_NO_MORE_FILES, ENOENT },        /* 18 */
        { ERROR_LOCK_VIOLATION, EACCES },       /* 33 */
        { ERROR_BAD_NETPATH, ENOENT },          /* 53 */
        { ERROR_NETWORK_ACCESS_DENIED, EACCES },/* 65 */
        { ERROR_BAD_NET_NAME, ENOENT },         /* 67 */
        { ERROR_FILE_EXISTS, EEXIST },          /* 80 */
        { ERROR_CANNOT_MAKE, EACCES },          /* 82 */
        { ERROR_FAIL_I24, EACCES },             /* 83 */
        { ERROR_INVALID_PARAMETER, EINVAL },    /* 87 */
        { ERROR_NO_PROC_SLOTS, EAGAIN },        /* 89 */
        { ERROR_DRIVE_LOCKED, EACCES },         /* 108 */
        { ERROR_BROKEN_PIPE, EPIPE },           /* 109 */
        { ERROR_DISK_FULL, ENOSPC },            /* 112 */
        { ERROR_INVALID_TARGET_HANDLE, EBADF }, /* 114 */
        { ERROR_INVALID_HANDLE, EINVAL },       /* 124 */
        { ERROR_WAIT_NO_CHILDREN, ECHILD },     /* 128 */
        { ERROR_CHILD_NOT_COMPLETE, ECHILD },   /* 129 */
        { ERROR_DIRECT_ACCESS_HANDLE, EBADF },  /* 130 */
        { ERROR_NEGATIVE_SEEK, EINVAL },        /* 131 */
        { ERROR_SEEK_ON_DEVICE, EACCES },       /* 132 */
        { ERROR_DIR_NOT_EMPTY, ENOTEMPTY },     /* 145 */
        { ERROR_NOT_LOCKED, EACCES },           /* 158 */
        { ERROR_BAD_PATHNAME, ENOENT },         /* 161 */
        { ERROR_MAX_THRDS_REACHED, EAGAIN },    /* 164 */
        { ERROR_LOCK_FAILED, EACCES },          /* 167 */
        { ERROR_ALREADY_EXISTS, EEXIST },       /* 183 */
        { ERROR_FILENAME_EXCED_RANGE, ENOENT }, /* 206 */
        { ERROR_NESTING_NOT_ALLOWED, EAGAIN },  /* 215 */
        { ERROR_NO_DATA, EPIPE },               /* 232 */
        { ERROR_NOT_ENOUGH_QUOTA, ENOMEM },     /* 1816 */
        { WSAEINTR, EINTR }, /* 10004 Interrupted function call. */
        { WSAEWOULDBLOCK, EAGAIN } /* 10035 This error is returned from operations on nonblocking sockets that cannot be completed immediately */
};

void setBaseErrno(DWORD dwLastErrorCode)
{
    errno = getBaseErrno(dwLastErrorCode);
}

int getBaseErrno(DWORD dwLastErrorCode)
{
    int i;

    for (i = 0; i < sizeof(errno_list)/sizeof(errno_list[0]); ++i) {
        if (dwLastErrorCode == errno_list[i].posixcode) {
            return errno_list[i].posixcode;
        }
    }
    LOG_FIXME("unsupport error code: %ld\n", dwLastErrorCode);
    return EINVAL;
}

/*
 * from: http://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx
 */
static int _launch_server(void) {
    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;
    SECURITY_ATTRIBUTES saAttr;

    // Set the bInheritHandle flag so pipe handles are inherited.
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT.
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0)) {
        fprintf(stderr, "failed to StdoutRd CreatePipe: %ld\n", GetLastError());
        return -1;
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        fprintf(stderr, "failed to set Stdout SetHandleInformation: %ld\n", GetLastError());
        return -1;
    }

    // Create a child process that uses the previously created pipes for STDIN and STDOUT.
    {
        PROCESS_INFORMATION piProcInfo;
        STARTUPINFO siStartInfo;
        BOOL bSuccess = FALSE;
        char module_path[MAX_PATH];
        TCHAR szCmdline[] = TEXT("sdb fork-server server");

        // Set up members of the PROCESS_INFORMATION structure.
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

        // Set up members of the STARTUPINFO structure.
        // This structure specifies the STDIN and STDOUT handles for redirection.
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
        siStartInfo.cb = sizeof(STARTUPINFO);
        siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
        siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        // Get current module path
        GetModuleFileName(NULL, module_path, sizeof(module_path));

        // Create the child process.
        bSuccess = CreateProcess(module_path, szCmdline, // command line
                NULL, // process security attributes
                NULL, // primary thread security attributes
                TRUE, // handles are inherited
                DETACHED_PROCESS, // creation flags
                NULL, // use parent's environment
                NULL, // use parent's current directory
                &siStartInfo, // STARTUPINFO pointer
                &piProcInfo); // receives PROCESS_INFORMATION

        // If an error occurs, exit the application.
        if (!bSuccess) {
            fprintf(stderr, "fail to create sdb server process: %ld\n", GetLastError());
            CloseHandle(g_hChildStd_OUT_Wr);
            CloseHandle(g_hChildStd_OUT_Rd);
            return -1;
        }
        else {
            CloseHandle(g_hChildStd_OUT_Wr);
            // Close handles to the child process and its primary thread.
            // Some applications might keep these handles to monitor the status
            // of the child process, for example.

            CloseHandle(piProcInfo.hProcess);
            CloseHandle(piProcInfo.hThread);
            // wait for sec due to for booting up sdb server
            sdb_sleep_ms(3000);
        }
    }
    return 0;
}

static void _start_logging(void)
{
    const char*  p = getenv(DEBUG_ENV);
    if (p == NULL) {
        return;
    }

    char    temp[ MAX_PATH ];
    FILE*   fnul;
    FILE*   flog;

    GetTempPath( sizeof(temp) - 8, temp );
    strcat( temp, "sdb.log" );

    /* Win32 specific redirections */
    fnul = fopen( "NUL", "rt" );
    if (fnul != NULL)
        stdin[0] = fnul[0];

    flog = fopen( temp, "at" );
    if (flog == NULL)
        flog = fnul;

    setvbuf( flog, NULL, _IONBF, 0 );

    stdout[0] = flog[0];
    stderr[0] = flog[0];
    fprintf(stderr,"--- sdb starting (pid %d) ---\n", getpid());
}

/**************************************************************************/
/**************************************************************************/
/*****                                                                *****/
/*****    common file descriptor handling                             *****/
/*****                                                                *****/
/**************************************************************************/
/**************************************************************************/

static sdb_mutex_t _win32_lock;
static sdb_mutex_t sdb_handle_map_lock;

static SDB_HANDLE* alloc_handle(int socket) {

	SDB_HANDLE* _h = NULL;
    sdb_mutex_lock(&_win32_lock, "_fh_alloc _win32_lock");

    if(total_handle_number < WIN32_MAX_FHS) {
    	total_handle_number++;
    	windows_handle_fd_count++;
    	if(socket) {
			SDB_SOCK_HANDLE* __h = malloc(sizeof(SDB_SOCK_HANDLE));
			__h->event_location = -1;
			_h = (SDB_HANDLE*)__h;
			_h->is_socket = 1;
    		_h->u.socket = INVALID_SOCKET;
			LOG_INFO("assign socket fd FD(%d)\n", _h->fd);
    	}
    	else {
    		_h = malloc(sizeof(SDB_HANDLE));
    		_h->u.file_handle = INVALID_HANDLE_VALUE;
    		_h->is_socket = 0;
    		LOG_INFO("assign file fd FD(%d)\n", _h->fd);
    	}

		_h->fd = windows_handle_fd_count;
    	sdb_handle_map_put(_h->fd, _h);
    }
    else {
    	errno = ENOMEM;
    	LOG_ERROR("no more space for file descriptor. max file descriptor is %d\n", WIN32_MAX_FHS);
    }

    sdb_mutex_unlock(&_win32_lock, "_fh_alloc _win32_lock");
    return _h;
}

SDB_HANDLE* sdb_handle_map_get(int _key) {
	MAP_KEY key;
	key.key_int = _key;
	sdb_mutex_lock(&sdb_handle_map_lock, "sdb_handle_map_get");
	SDB_HANDLE* result = map_get(&sdb_handle_map, key);
	sdb_mutex_unlock(&sdb_handle_map_lock, "sdb_handle_map_get");
	return result;
}

void sdb_handle_map_put(int _key, SDB_HANDLE* value) {
	MAP_KEY key;
	key.key_int = _key;
	sdb_mutex_lock(&sdb_handle_map_lock, "sdb_handle_map_put");
	map_put(&sdb_handle_map, key, value);
	sdb_mutex_unlock(&sdb_handle_map_lock, "sdb_handle_map_put");
}

void sdb_handle_map_remove(int _key) {
	MAP_KEY key;
	key.key_int = _key;
	sdb_mutex_lock(&sdb_handle_map_lock, "sdb_handle_map_remove");
	map_remove(&sdb_handle_map, key);
	sdb_mutex_unlock(&sdb_handle_map_lock, "sdb_handle_map_remove");
}

static int _fh_close(SDB_HANDLE* _h) {
	if(IS_SOCKET_HANDLE(_h)) {
	    shutdown(_h->u.socket, SD_BOTH);
	    closesocket(_h->u.socket);
	    _h->u.socket = INVALID_SOCKET;
	}
	else {
	    CloseHandle(_h->u.file_handle);
	    _h->u.file_handle = INVALID_HANDLE_VALUE;
	}
	sdb_handle_map_remove(_h->fd);
	free(_h);
	sdb_mutex_lock(&_win32_lock, "_fh_close");
	total_handle_number--;
	sdb_mutex_unlock(&_win32_lock, "_fh_close");
    return 0;
}

static int check_socket_err(int result) {

	if(result != SOCKET_ERROR) {
		return result;
	}

	DWORD err = WSAGetLastError();

	if(err == WSAEWOULDBLOCK) {
		errno = EAGAIN;
		LOG_ERROR("socket error EAGAIN\n");
	}
	else if(err == WSAEINTR) {
		errno = EINTR;
		LOG_ERROR("socket error EINTR\n");
	}
	else if(err == 0) {
		errno = 0;
		LOG_ERROR("socket error 0\n");
	}
	else {
		errno = EINVAL;
		LOG_ERROR("unknown error %d\n", err);
	}
	return -1;
}

static int check_file_err(HANDLE h) {

    if (h != INVALID_HANDLE_VALUE) {
    	return 1;
    }

	_fh_close(h);
	DWORD err = GetLastError();

	if(err == ERROR_PATH_NOT_FOUND) {
		LOG_ERROR("path not found\n");
		errno = ENOTDIR;
	}
	else if (err == ERROR_FILE_NOT_FOUND) {
		LOG_ERROR("file not found\n");
		errno = ENOENT;
	}
	else {
		LOG_ERROR("unknown erro %d\n", err);
		errno = ENOENT;
	}
	return -1;
}

static int _sdb_open(const char* path, int file_options) {

    DWORD access_mode = 0;
    DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    if(file_options == O_RDONLY) {
        access_mode = GENERIC_READ;
    }
    else if(file_options == O_WRONLY) {
    	access_mode = GENERIC_WRITE;
    }
    else if(file_options == O_RDWR) {
    	access_mode = GENERIC_READ | GENERIC_WRITE;
    }
    else {
        LOG_ERROR("invalid options (0x%0x)\n", file_options);
        errno = EINVAL;
        return -1;
    }

    SDB_HANDLE* _h;
    _h = alloc_handle(0);
    if (!_h) {
        errno = ENOMEM;
        return -1;
    }

    _h->u.file_handle = CreateFile(path, access_mode, share_mode, NULL, OPEN_EXISTING, 0, NULL);

    if(check_file_err(_h->u.file_handle) == -1) {
    	return -1;
    }
    return _h->fd;
}

static int _sdb_creat(const char* path, int mode) {
    SDB_HANDLE* _h = alloc_handle(0);

    if (!_h) {
    	errno = ENOMEM;
        return -1;
    }
    _h->u.file_handle = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);

    if(check_file_err(_h->u.file_handle) == -1) {
    	return -1;
    }
    return _h->fd;
}

static int _sdb_read(int fd, void* buffer, size_t r_length) {
    SDB_HANDLE* _h = sdb_handle_map_get(fd);

    if (_h == NULL) {
    	LOG_ERROR("FD(%d) disconnected\n", fd);
        return 0;
    }

    if(IS_SOCKET_HANDLE(_h)) {
    	return check_socket_err(recv(_h->u.socket, buffer, r_length, 0));
    }
    else {
        DWORD r_bytes;

        if (!ReadFile(_h->u.file_handle, buffer, (DWORD) r_length, &r_bytes, NULL)) {
            D( "sdb_read: could not read %d bytes from FD(%d)\n", r_length, _h->fd);
            errno = EIO;
            return -1;
        }
        return (int) r_bytes;
    }
}

static int _sdb_write(int fd, const void* buffer, size_t w_length) {
    SDB_HANDLE* _h = sdb_handle_map_get(fd);

    if (_h == NULL) {
    	LOG_ERROR("FD(%d) not exists. disconnected\n", fd);
        return 0;
    }

    if(IS_SOCKET_HANDLE(_h)) {
    	return check_socket_err(send(_h->u.socket, buffer, w_length, 0));
    }
    else {
        DWORD w_bytes;

        if (!WriteFile(_h->u.file_handle, buffer, (DWORD) w_length, &w_bytes, NULL)) {
            D( "sdb_file_write: could not write %d bytes from FD(%d)\n", w_length, _h->fd);
            errno = EIO;
            return -1;
        }
        return (int) w_bytes;
    }
}

static int _sdb_shutdown(int fd) {
	SDB_HANDLE* _h = sdb_handle_map_get(fd);

    if (_h == NULL) {
    	LOG_ERROR("FD(%d) not exists\n", fd);
        return -1;
    }

	if(!IS_SOCKET_HANDLE(_h)) {
		LOG_ERROR("FD(%d) is file fd\n", _h->fd);
		return -1;
	}

    D( "sdb_shutdown: FD(%d)\n", fd);
    shutdown(_h->u.socket, SD_BOTH);
    return 0;
}

static int _sdb_transport_close(int fd) {

	SDB_HANDLE* _h = sdb_handle_map_get(fd);

    if (_h == NULL) {
    	LOG_ERROR("FD(%d) not exists\n", fd);
        return -1;
    }

    D( "sdb_close: FD(%d)\n", fd);
    shutdown(_h->u.socket, SD_BOTH);
    closesocket(_h->u.socket);
    _h->u.socket = INVALID_SOCKET;
    return 0;
}

static int _sdb_close(int fd) {

	SDB_HANDLE* _h = sdb_handle_map_get(fd);

    if (_h == NULL) {
    	LOG_ERROR("FD(%d) not exists\n", fd);
        return -1;
    }

    D( "sdb_close: FD(%d)\n", fd);
    _fh_close(_h);
    return 0;
}

static int _sdb_mkdir(const char* path, int mode) {
    return _mkdir(path);
}

static __inline__ int unix_close(int fd) {
    return close(fd);
}

int unix_read(int fd, void* buf, size_t len) {
    return read(fd, buf, len);
}

int unix_write(int fd, const void* buf, size_t len) {
    return write(fd, buf, len);
}

static void _close_on_exec(int fd) {
    /* nothing really */
}

static void _sdb_sleep_ms(int mseconds) {
    Sleep(mseconds);
}

static int _sdb_socket_setbufsize(int fd, int bufsize) {
    int opt = bufsize;
    return setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (const char*) &opt, sizeof(opt));
}

static char* _sdb_dirstart(const char* path) {
    char* p = strchr(path, '/');
    char* p2 = strchr(path, '\\');

    if (!p)
        p = p2;
    else if (p2 && p2 > p)
        p = p2;

    return p;
}

static char* _sdb_dirstop(const char* path) {
    char* p = strrchr(path, '/');
    char* p2 = strrchr(path, '\\');

    if (!p)
        p = p2;
    else if (p2 && p2 > p)
        p = p2;

    return p;
}

static int _winsock_init;

static void _cleanup_winsock() {
    WSACleanup();
}

static void _init_winsock(void) {
    if (!_winsock_init) {
        WSADATA wsaData;
        if(WSAStartup(MAKEWORD(2, 2), &wsaData)) {
            LOG_FATAL("sdb: could not initialize Winsock\n");
        }
        _winsock_init = 1;
        atexit(_cleanup_winsock);
    }
}

static int _sdb_port_listen(uint32_t inet, int port, int type) {
    SDB_HANDLE* _h = alloc_handle(1);
    struct sockaddr_in addr;
    SOCKET s;
    int n;

    if (_h == NULL) {
        return -1;
    }

    if (!_winsock_init)
        _init_winsock();

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(inet);
    addr.sin_port = htons(port);

    if ((s = socket(AF_INET, type, 0)) == INVALID_SOCKET) {
        LOG_ERROR("failed to create socket to %u(%d)\n", inet, port);
        return -1;
    }

    _h->u.socket = s;

    n = 1;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*) &n, sizeof(n));

    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        _fh_close(_h);
        return -1;
    }
    // only listen if tcp mode
    if (type == SOCK_STREAM) {
        if (listen(s, LISTEN_BACKLOG) < 0) {
            _fh_close(_h);
            return -1;
        }
    }

    D( "sdb_port_listen: port %d type %s => FD(%d)\n", port, type != SOCK_STREAM ? "udp" : "tcp", _h->fd);
    return _h->fd;
}

static int _sdb_host_connect(const char *host, int port, int type) {
    SDB_HANDLE* _h = alloc_handle(1);
    struct hostent *hp;
    struct sockaddr_in addr;
    SOCKET s;

    if (!_h)
        return -1;

    if (!_winsock_init)
        _init_winsock();

    // FIXME: might take a long time to get information
    if ((hp = gethostbyname(host)) == NULL) {
        LOG_ERROR("failed to get hostname:%s(%d)\n", host, port);
        _fh_close(_h);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = hp->h_addrtype;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);

    if ((s = socket(hp->h_addrtype, type, 0)) == INVALID_SOCKET) {
        LOG_ERROR("failed to create socket to %s(%d)\n", host, port);
        _fh_close(_h);
        return -1;
    }

    _h->u.socket = s;

    if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        _fh_close(_h);
        return -1;
    }

    D("sdb_host_connect: host '%s' port %d type %s => FD(%d)\n", host, port, type != SOCK_STREAM ? "udp" : "tcp", _h->fd);
    return _h->fd;
}

static int _sdb_socket_accept(int serverfd) {

    SDB_HANDLE* server_h = sdb_handle_map_get(serverfd);
    struct sockaddr addr;
    socklen_t alen = sizeof(addr);

    if (!server_h) {
        LOG_ERROR( "FD(%d) Invalid server fd\n", serverfd);
        return -1;
    }

	if(!IS_SOCKET_HANDLE(server_h)) {
		LOG_ERROR("FD(%d) is file fd\n", serverfd);
		return -1;
	}

    SDB_HANDLE* _h = alloc_handle(1);
    if (!_h) {
        return -1;
    }

    _h->u.socket = accept(server_h->u.socket, &addr, &alen);
    if (_h->u.socket == INVALID_SOCKET) {
        _fh_close(_h);
        LOG_ERROR( "sdb_socket_accept: accept on FD(%d) return error %ld\n", serverfd, GetLastError());
        return -1;
    }

    LOG_INFO( "sdb_socket_accept on FD(%d) returns FD(%d)\n", serverfd, _h->fd);
    return _h->fd;
}

static void _disable_tcp_nagle(int fd) {

    SDB_HANDLE* _h = sdb_handle_map_get(fd);
    if (!_h) {
        return;
    }

	if(!IS_SOCKET_HANDLE(_h)) {
		LOG_ERROR("FD(%d) is file fd\n", fd);
		return;
	}

    int on;
    setsockopt(_h->u.socket, IPPROTO_TCP, TCP_NODELAY, (const char*) &on, sizeof(on));
}

static int socketpair_impl(int af, int type, int protocol, SOCKET socks[2])
{
    struct sockaddr_in addr;
    SOCKET s;

    LOG_INFO("+socketpair impl in\n");

    if (!_winsock_init) {
        _init_winsock();
    }
    // Create a socket, bind it to 127.0.0.1 and a random port, and listen.
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0x7f000001);
    addr.sin_port = 0;

    socks[0] = socks[1] = INVALID_SOCKET;

    if ((s = socket(af, type, 0)) == INVALID_SOCKET) {
        return -1;
    }

    if (bind(s, (const struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }

    int addr_len = sizeof(addr);
    if (getsockname(s, (struct sockaddr*) &addr, &addr_len) == SOCKET_ERROR) {
        closesocket(s);
        return -1;
    }

    do
    {
        if (listen(s, 1) == SOCKET_ERROR) {
            break;
        }
        // Creates the second socket and connects it to the same address and port.
        if ((socks[0] = socket(af, type, 0)) == INVALID_SOCKET) {
            break;
        }
        if (connect(socks[0], (const struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) {
            break;
        }
        if ((socks[1] = accept(s, NULL, NULL)) == INVALID_SOCKET) {
            break;
        }
        closesocket(s);
        LOG_INFO("-socketpair impl out\n");
        return 0;

    } while (0);

    closesocket(socks[0]);
    closesocket(socks[1]);
    closesocket(s);

    LOG_ERROR("socketpair error: %ld\n", WSAGetLastError());
    return -1;
}

static int _sdb_socketpair(int sv[2]) {
    SOCKET socks[2];
    int r = 0;

    r = socketpair_impl( AF_INET, SOCK_STREAM, IPPROTO_TCP, socks);
    if (r < 0) {
        LOG_ERROR("failed to create socket pair:(%ld)\n", GetLastError());
        return -1;
    }

    SDB_HANDLE* _ha = alloc_handle(1);
    SDB_HANDLE* _hb = alloc_handle(1);

    if (!_ha || !_hb) {
        return -1;
    }

    _ha->u.socket = socks[0];
    _hb->u.socket = socks[1];

    if (_ha->u.socket == INVALID_SOCKET || _hb->u.socket == INVALID_SOCKET) {
        _fh_close(_ha);
        _fh_close(_hb);
        LOG_ERROR( "failed to get socket:(%ld)\n", GetLastError());
        return -1;
    }
    sv[0] = _ha->fd;
    sv[1] = _hb->fd;

    D( "sdb_socketpair: returns (FD(%d), FD(%d))\n", sv[0], sv[1] );
    return 0;
}


/**************************************************************************/
/**************************************************************************/
/*****                                                                *****/
/*****    fdevents emulation                                          *****/
/*****                                                                *****/
/*****   this is a very simple implementation, we rely on the fact    *****/
/*****   that SDB doesn't use FDE_ERROR.                              *****/
/*****                                                                *****/
/**************************************************************************/
/**************************************************************************/

static void _sdb_sysdeps_init(void) {
    //re define mutex variable & initialized
#undef SDB_MUTEX
#define  SDB_MUTEX(x)  InitializeCriticalSection( & x );
    SDB_MUTEX(dns_lock)
    SDB_MUTEX(transport_lock)
    SDB_MUTEX(usb_lock)
    SDB_MUTEX(wakeup_select_lock)
    SDB_MUTEX(D_lock)
    SDB_MUTEX(_win32_lock);
    SDB_MUTEX(sdb_handle_map_lock);
}

typedef  void (*win_thread_func_t)(void*  arg);

static int _sdb_thread_create(sdb_thread_t *thread, sdb_thread_func_t func, void* arg) {
    thread->tid = _beginthread((win_thread_func_t) func, 0, arg);
    if (thread->tid == (unsigned) -1L) {
        return -1;
    }
    return 0;
}

static int _pthread_mutex_lock(pthread_mutex_t *mutex) {
    // Request ownership of the critical section.
    EnterCriticalSection(mutex);

    return 0;
}

static int _pthread_mutex_unlock(pthread_mutex_t* mutex) {
    // Release ownership of the critical section.
    LeaveCriticalSection(mutex);

    return 0;
}

static int _pthread_cond_init(pthread_cond_t *cond, const void *unused)
{
    cond->waiters = 0;
    cond->was_broadcast = 0;
    InitializeCriticalSection(&cond->waiters_lock);

    cond->sema = CreateSemaphore(NULL, 0, LONG_MAX, NULL);
    if (!cond->sema) {
        //die("CreateSemaphore() failed");
    }

    cond->continue_broadcast = CreateEvent(NULL,    /* security */
                FALSE,          /* auto-reset */
                FALSE,          /* not signaled */
                NULL);          /* name */
    if (!cond->continue_broadcast) {
        //die("CreateEvent() failed");
    }

    return 0;
}

static int _pthread_cond_wait(pthread_cond_t *cond, CRITICAL_SECTION *mutex) {
    int last_waiter;

    EnterCriticalSection(&cond->waiters_lock);
    cond->waiters++;
    LeaveCriticalSection(&cond->waiters_lock);

    /*
     * Unlock external mutex and wait for signal.
     * NOTE: we've held mutex locked long enough to increment
     * waiters count above, so there's no problem with
     * leaving mutex unlocked before we wait on semaphore.
     */
    LeaveCriticalSection(mutex);

    /* let's wait - ignore return value */
    WaitForSingleObject(cond->sema, INFINITE);

    /*
     * Decrease waiters count. If we are the last waiter, then we must
     * notify the broadcasting thread that it can continue.
     * But if we continued due to cond_signal, we do not have to do that
     * because the signaling thread knows that only one waiter continued.
     */
    EnterCriticalSection(&cond->waiters_lock);
    cond->waiters--;
    last_waiter = cond->was_broadcast && cond->waiters == 0;
    LeaveCriticalSection(&cond->waiters_lock);

    if (last_waiter) {
        /*
         * cond_broadcast was issued while mutex was held. This means
         * that all other waiters have continued, but are contending
         * for the mutex at the end of this function because the
         * broadcasting thread did not leave cond_broadcast, yet.
         * (This is so that it can be sure that each waiter has
         * consumed exactly one slice of the semaphor.)
         * The last waiter must tell the broadcasting thread that it
         * can go on.
         */
        SetEvent(cond->continue_broadcast);
        /*
         * Now we go on to contend with all other waiters for
         * the mutex. Auf in den Kampf!
         */
    }
    /* lock external mutex again */
    EnterCriticalSection(mutex);

    return 0;
}

static int _pthread_cond_broadcast(pthread_cond_t *cond)
{
    EnterCriticalSection(&cond->waiters_lock);

    if ((cond->was_broadcast = cond->waiters > 0)) {
        /* wake up all waiters */
        ReleaseSemaphore(cond->sema, cond->waiters, NULL);
        LeaveCriticalSection(&cond->waiters_lock);
        /*
         * At this point all waiters continue. Each one takes its
         * slice of the semaphor. Now it's our turn to wait: Since
         * the external mutex is held, no thread can leave cond_wait,
         * yet. For this reason, we can be sure that no thread gets
         * a chance to eat *more* than one slice. OTOH, it means
         * that the last waiter must send us a wake-up.
         */
        WaitForSingleObject(cond->continue_broadcast, INFINITE);
        /*
         * Since the external mutex is held, no thread can enter
         * cond_wait, and, hence, it is safe to reset this flag
         * without cond->waiters_lock held.
         */
        cond->was_broadcast = 0;
    } else {
        LeaveCriticalSection(&cond->waiters_lock);
    }
    return 0;
}

static char* _ansi_to_utf8(const char *str) {
    int len;
    char *utf8;
    wchar_t *unicode;

    //ANSI( MutiByte ) -> UCS-2( WideByte ) -> UTF-8( MultiByte )
    len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    unicode = (wchar_t *) calloc(len + 1, sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, str, -1, unicode, len);

    len = WideCharToMultiByte(CP_UTF8, 0, unicode, -1, NULL, 0, NULL, NULL);
    utf8 = (char *) calloc(len + 1, sizeof(char));

    WideCharToMultiByte(CP_UTF8, 0, unicode, -1, utf8, len, NULL, NULL);
    free(unicode);

    return utf8;
}
const struct utils_os_backend utils_windows_backend = {
    .name = "windows utils",
    .launch_server = _launch_server,
    .start_logging = _start_logging,
    .ansi_to_utf8 = _ansi_to_utf8,
    .sdb_open = _sdb_open,
    .sdb_creat = _sdb_creat,
    .sdb_read = _sdb_read,
    .sdb_write = _sdb_write,
    .sdb_shutdown = _sdb_shutdown,
    .sdb_transport_close = _sdb_transport_close,
    .sdb_close = _sdb_close,
    .close_on_exec = _close_on_exec,
    .sdb_mkdir = _sdb_mkdir,
    .sdb_socket_accept = _sdb_socket_accept,
    .sdb_socketpair = _sdb_socketpair,
    .sdb_sleep_ms = _sdb_sleep_ms,
    .sdb_dirstart = _sdb_dirstart,
    .sdb_dirstop = _sdb_dirstop,
    .sdb_socket_setbufsize = _sdb_socket_setbufsize,
    .disable_tcp_nagle = _disable_tcp_nagle,
// simple pthread implementations
    .sdb_thread_create = _sdb_thread_create,
    .sdb_mutex_lock = _pthread_mutex_lock,
    .sdb_mutex_unlock = _pthread_mutex_unlock,
    .sdb_cond_wait = _pthread_cond_wait,
    .sdb_cond_init = _pthread_cond_init,
    .sdb_cond_broadcast = _pthread_cond_broadcast,
    .sdb_sysdeps_init = _sdb_sysdeps_init,
    .sdb_host_connect = _sdb_host_connect,
    .sdb_port_listen = _sdb_port_listen
};
