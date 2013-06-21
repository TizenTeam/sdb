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
#include "utils.h"
#include "fdevent.h"
#include "fdevent_backend.h"

#define  TRACE_TAG  TRACE_SYSDEPS
#include "sdb.h"
#include "utils_backend.h"

/*
 * from: http://msdn.microsoft.com/en-us/library/windows/desktop/ms682499(v=vs.85).aspx
 */
static int _launch_server(int server_port) {
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

            //Read "OK\n" stdout message from sdb server
            DWORD dwRead;
            char chBuf[3];

            bSuccess = ReadFile(g_hChildStd_OUT_Rd, chBuf, sizeof(chBuf), &dwRead, NULL);
            CloseHandle(g_hChildStd_OUT_Rd);

            if( ! bSuccess || dwRead == 0 ) {
                fprintf(stderr, "fail to read ok message from sdb server: %ld\n", GetLastError());
                return -1;
            }
            if (dwRead != 3 || chBuf[0] != 'O' || chBuf[1] != 'K' || chBuf[2] != '\n') {
                fprintf(stderr, "fail to get OK\\n from sdb server: [%s]\n", chBuf);
                return -1;
            }
        }
    }
    return 0;
}

static void _start_logging(void)
{
    const char*  p = getenv("SDB_TRACE");
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


extern void fatal(const char *fmt, ...);

#define assert(cond)  do { if (!(cond)) fatal( "assertion failed '%s' on %s:%ld\n", #cond, __FILE__, __LINE__ ); } while (0)

/**************************************************************************/
/**************************************************************************/
/*****                                                                *****/
/*****    common file descriptor handling                             *****/
/*****                                                                *****/
/**************************************************************************/
/**************************************************************************/

static sdb_mutex_t _win32_lock;
static FHRec _win32_fhs[WIN32_MAX_FHS];
static int _win32_fh_count;

FH _fh_from_int(int fd) {
    FH f;

    fd -= WIN32_FH_BASE;

    if (fd < 0 || fd >= _win32_fh_count) {
        D( "_fh_from_int: invalid fd %d\n", fd + WIN32_FH_BASE);
        errno = EBADF;
        return NULL;
    }

    f = &_win32_fhs[fd];

    if (f->used == 0) {
        D( "_fh_from_int: invalid fd %d\n", fd + WIN32_FH_BASE);
        errno = EBADF;
        return NULL;
    }

    return f;
}

int _fh_to_int(FH f) {
    if (f && f->used && f >= _win32_fhs && f < _win32_fhs + WIN32_MAX_FHS)
        return (int) (f - _win32_fhs) + WIN32_FH_BASE;

    return -1;
}

static FH _fh_alloc(FHClass clazz) {
    int nn;
    FH f = NULL;

    sdb_mutex_lock(&_win32_lock);

    if (_win32_fh_count < WIN32_MAX_FHS) {
        f = &_win32_fhs[_win32_fh_count++];
        goto Exit;
    }

    for (nn = 0; nn < WIN32_MAX_FHS; nn++) {
        if (_win32_fhs[nn].clazz == NULL) {
            f = &_win32_fhs[nn];
            goto Exit;
        }
    }
    D( "_fh_alloc: no more free file descriptors\n");
    Exit: if (f) {
        f->clazz = clazz;
        f->used = 1;
        f->eof = 0;
        clazz->_fh_init(f);
    }
    sdb_mutex_unlock(&_win32_lock);
    return f;
}

static int _fh_close(FH f) {
    if (f->used) {
        f->clazz->_fh_close(f);
        f->used = 0;
        f->eof = 0;
        f->clazz = NULL;
    }
    return 0;
}

/* forward definitions */
static const FHClassRec _fh_file_class;
static const FHClassRec _fh_socket_class;

/**************************************************************************/
/**************************************************************************/
/*****                                                                *****/
/*****    file-based descriptor handling                              *****/
/*****                                                                *****/
/**************************************************************************/
/**************************************************************************/

static void _fh_file_init(FH f) {
    f->fh_handle = INVALID_HANDLE_VALUE;
}

static int _fh_file_close(FH f) {
    CloseHandle(f->fh_handle);
    f->fh_handle = INVALID_HANDLE_VALUE;
    return 0;
}

static int _fh_file_read(FH f, void* buf, int len) {
    DWORD read_bytes;

    if (!ReadFile(f->fh_handle, buf, (DWORD) len, &read_bytes, NULL)) {
        D( "sdb_read: could not read %d bytes from %s\n", len, f->name);
        errno = EIO;
        return -1;
    } else if (read_bytes < (DWORD) len) {
        f->eof = 1;
    }
    return (int) read_bytes;
}

static int _fh_file_write(FH f, const void* buf, int len) {
    DWORD wrote_bytes;

    if (!WriteFile(f->fh_handle, buf, (DWORD) len, &wrote_bytes, NULL)) {
        D( "sdb_file_write: could not write %d bytes from %s\n", len, f->name);
        errno = EIO;
        return -1;
    } else if (wrote_bytes < (DWORD) len) {
        f->eof = 1;
    }
    return (int) wrote_bytes;
}

static int _fh_file_lseek(FH f, int pos, int origin) {
    DWORD method;
    DWORD result;

    switch (origin) {
    case SEEK_SET:
        method = FILE_BEGIN;
        break;
    case SEEK_CUR:
        method = FILE_CURRENT;
        break;
    case SEEK_END:
        method = FILE_END;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    result = SetFilePointer(f->fh_handle, pos, NULL, method);
    if (result == INVALID_SET_FILE_POINTER) {
        errno = EIO;
        return -1;
    } else {
        f->eof = 0;
    }
    return (int) result;
}

static void _fh_file_hook(FH f, int event, EventHook eventhook); /* forward */

static const FHClassRec _fh_file_class = { _fh_file_init, _fh_file_close, _fh_file_lseek, _fh_file_read, _fh_file_write,
        _fh_file_hook };

/**************************************************************************/
/**************************************************************************/
/*****                                                                *****/
/*****    file-based descriptor handling                              *****/
/*****                                                                *****/
/**************************************************************************/
/**************************************************************************/

static int _sdb_open(const char* path, int options) {
    FH f;

    DWORD desiredAccess = 0;
    DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    switch (options) {
    case O_RDONLY:
        desiredAccess = GENERIC_READ;
        break;
    case O_WRONLY:
        desiredAccess = GENERIC_WRITE;
        break;
    case O_RDWR:
        desiredAccess = GENERIC_READ | GENERIC_WRITE;
        break;
    default:
        D("sdb_open: invalid options (0x%0x)\n", options);
        errno = EINVAL;
        return -1;
    }

    f = _fh_alloc(&_fh_file_class);
    if (!f) {
        errno = ENOMEM;
        return -1;
    }

    f->fh_handle = CreateFile(path, desiredAccess, shareMode, NULL, OPEN_EXISTING, 0, NULL);

    if (f->fh_handle == INVALID_HANDLE_VALUE) {
        _fh_close(f);
        D( "sdb_open: could not open '%s':", path);
        switch (GetLastError()) {
        case ERROR_FILE_NOT_FOUND:
            D( "file not found\n");
            errno = ENOENT;
            return -1;

        case ERROR_PATH_NOT_FOUND:
            D( "path not found\n");
            errno = ENOTDIR;
            return -1;

        default:
            D( "unknown error\n");
            errno = ENOENT;
            return -1;
        }
    }

    snprintf(f->name, sizeof(f->name), "%d(%s)", _fh_to_int(f), path);
    D( "sdb_open: '%s' => fd %d\n", path, _fh_to_int(f));
    return _fh_to_int(f);
}

static int _sdb_open_mode(const char* path, int options, int mode) {
    return sdb_open(path, options);
}

static int _unix_open(const char* path, int options, ...) {
    if ((options & O_CREAT) == 0) {
        return open(path, options);
    } else {
        int mode;
        va_list args;
        va_start( args, options);
        mode = va_arg( args, int );
        va_end( args);
        return open(path, options, mode);
    }
}

/* ignore mode on Win32 */
static int _sdb_creat(const char* path, int mode) {
    FH f;

    f = _fh_alloc(&_fh_file_class);
    if (!f) {
        errno = ENOMEM;
        return -1;
    }

    f->fh_handle = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);

    if (f->fh_handle == INVALID_HANDLE_VALUE) {
        _fh_close(f);
        D( "sdb_creat: could not open '%s':", path);
        switch (GetLastError()) {
        case ERROR_FILE_NOT_FOUND:
            D( "file not found\n");
            errno = ENOENT;
            return -1;

        case ERROR_PATH_NOT_FOUND:
            D( "path not found\n");
            errno = ENOTDIR;
            return -1;

        default:
            D( "unknown error\n");
            errno = ENOENT;
            return -1;
        }
    }
    snprintf(f->name, sizeof(f->name), "%d(%s)", _fh_to_int(f), path);
    D( "sdb_creat: '%s' => fd %d\n", path, _fh_to_int(f));
    return _fh_to_int(f);
}

static int _sdb_read(int fd, void* buf, size_t len) {
    FH f = _fh_from_int(fd);

    if (f == NULL) {
        return -1;
    }

    return f->clazz->_fh_read(f, buf, len);
}

static int _sdb_write(int fd, const void* buf, size_t len) {
    FH f = _fh_from_int(fd);

    if (f == NULL) {
        return -1;
    }

    return f->clazz->_fh_write(f, buf, len);
}

static int _sdb_lseek(int fd, int pos, int where) {
    FH f = _fh_from_int(fd);

    if (!f) {
        return -1;
    }

    return f->clazz->_fh_lseek(f, pos, where);
}

static int _sdb_shutdown(int fd) {
    FH f = _fh_from_int(fd);

    if (!f) {
        return -1;
    }

    D( "sdb_shutdown: %s\n", f->name);
    shutdown(f->fh_socket, SD_BOTH);
    return 0;
}

static int _sdb_close(int fd) {
    FH f = _fh_from_int(fd);

    if (!f) {
        return -1;
    }

    D( "sdb_close: %s\n", f->name);
    _fh_close(f);
    return 0;
}

static int _sdb_unlink(const char* path) {
    int rc = unlink(path);

    if (rc == -1 && errno == EACCES) {
        /* unlink returns EACCES when the file is read-only, so we first */
        /* try to make it writable, then unlink again...                  */
        rc = chmod(path, _S_IREAD | _S_IWRITE);
        if (rc == 0)
            rc = unlink(path);
    }
    return rc;
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

/**************************************************************************/
/**************************************************************************/
/*****                                                                *****/
/*****    socket-based file descriptors                               *****/
/*****                                                                *****/
/**************************************************************************/
/**************************************************************************/

static void _socket_set_errno(void) {
    switch (WSAGetLastError()) {
    case 0:
        errno = 0;
        break;
    case WSAEWOULDBLOCK:
        errno = EAGAIN;
        break;
    case WSAEINTR:
        errno = EINTR;
        break;
    default:
        D( "_socket_set_errno: unhandled value %d\n", WSAGetLastError());
        errno = EINVAL;
    }
}

static void _fh_socket_init(FH f) {
    f->fh_socket = INVALID_SOCKET;
    f->event = WSACreateEvent();
    f->mask = 0;
}

static int _fh_socket_close(FH f) {
    /* gently tell any peer that we're closing the socket */
    shutdown(f->fh_socket, SD_BOTH);
    closesocket(f->fh_socket);
    f->fh_socket = INVALID_SOCKET;
    CloseHandle(f->event);
    f->mask = 0;
    return 0;
}

static int _fh_socket_lseek(FH f, int pos, int origin) {
    errno = EPIPE;
    return -1;
}

static int _fh_socket_read(FH f, void* buf, int len) {
    int result = recv(f->fh_socket, buf, len, 0);
    if (result == SOCKET_ERROR) {
        _socket_set_errno();
        result = -1;
    }
    return result;
}

static int _fh_socket_write(FH f, const void* buf, int len) {
    int result = send(f->fh_socket, buf, len, 0);
    if (result == SOCKET_ERROR) {
        _socket_set_errno();
        result = -1;
    }
    return result;
}

static void _fh_socket_hook(FH f, int event, EventHook hook); /* forward */

static const FHClassRec _fh_socket_class = { _fh_socket_init, _fh_socket_close, _fh_socket_lseek, _fh_socket_read,
        _fh_socket_write, _fh_socket_hook };

/**************************************************************************/
/**************************************************************************/
/*****                                                                *****/
/*****    replacement for libs/cutils/socket_xxxx.c                   *****/
/*****                                                                *****/
/**************************************************************************/
/**************************************************************************/

#include <winsock2.h>

static int _winsock_init;

static void _cleanup_winsock(void) {
    WSACleanup();
}

static void _init_winsock(void) {
    if (!_winsock_init) {
        WSADATA wsaData;
        int rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (rc != 0) {
            fatal("sdb: could not initialize Winsock\n");
        }
        atexit(_cleanup_winsock);
        _winsock_init = 1;
    }
}

static int _socket_loopback_client(int port, int type) {
    FH f = _fh_alloc(&_fh_socket_class);
    struct sockaddr_in addr;
    SOCKET s;

    if (!f)
        return -1;

    if (!_winsock_init)
        _init_winsock();

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    s = socket(AF_INET, type, 0);
    if (s == INVALID_SOCKET) {
        D("socket_loopback_client: could not create socket\n");
        _fh_close(f);
        return -1;
    }

    f->fh_socket = s;
    if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        D("socket_loopback_client: could not connect to %s:%d\n", type != SOCK_STREAM ? "udp" : "tcp", port);
        _fh_close(f);
        return -1;
    }
    snprintf(f->name, sizeof(f->name), "%d(lo-client:%s%d)", _fh_to_int(f), type != SOCK_STREAM ? "udp:" : "", port);
    D( "socket_loopback_client: port %d type %s => fd %d\n", port, type != SOCK_STREAM ? "udp" : "tcp", _fh_to_int(f));
    return _fh_to_int(f);
}

#define LISTEN_BACKLOG 4

static int _socket_loopback_server(int port, int type) {
    FH f = _fh_alloc(&_fh_socket_class);
    struct sockaddr_in addr;
    SOCKET s;
    int n;

    if (!f) {
        return -1;
    }

    if (!_winsock_init)
        _init_winsock();

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    s = socket(AF_INET, type, 0);
    if (s == INVALID_SOCKET)
        return -1;

    f->fh_socket = s;

    n = 1;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*) &n, sizeof(n));

    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        _fh_close(f);
        return -1;
    }
    if (type == SOCK_STREAM) {
        int ret;

        ret = listen(s, LISTEN_BACKLOG);
        if (ret < 0) {
            _fh_close(f);
            return -1;
        }
    }
    snprintf(f->name, sizeof(f->name), "%d(lo-server:%s%d)", _fh_to_int(f), type != SOCK_STREAM ? "udp:" : "", port);
    D( "socket_loopback_server: port %d type %s => fd %d\n", port, type != SOCK_STREAM ? "udp" : "tcp", _fh_to_int(f));
    return _fh_to_int(f);
}

static int _socket_network_client(const char *host, int port, int type) {
    FH f = _fh_alloc(&_fh_socket_class);
    struct hostent *hp;
    struct sockaddr_in addr;
    SOCKET s;

    if (!f)
        return -1;

    if (!_winsock_init)
        _init_winsock();

    hp = gethostbyname(host);
    if (hp == 0) {
        _fh_close(f);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = hp->h_addrtype;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);

    s = socket(hp->h_addrtype, type, 0);
    if (s == INVALID_SOCKET) {
        _fh_close(f);
        return -1;
    }
    f->fh_socket = s;

    if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        _fh_close(f);
        return -1;
    }

    snprintf(f->name, sizeof(f->name), "%d(net-client:%s%d)", _fh_to_int(f), type != SOCK_STREAM ? "udp:" : "", port);
    D(
            "socket_network_client: host '%s' port %d type %s => fd %d\n", host, port, type != SOCK_STREAM ? "udp" : "tcp", _fh_to_int(f));
    return _fh_to_int(f);
}

static int _socket_inaddr_any_server(int port, int type) {
    FH f = _fh_alloc(&_fh_socket_class);
    struct sockaddr_in addr;
    SOCKET s;
    int n;

    if (!f)
        return -1;

    if (!_winsock_init)
        _init_winsock();

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s = socket(AF_INET, type, 0);
    if (s == INVALID_SOCKET) {
        _fh_close(f);
        return -1;
    }

    f->fh_socket = s;
    n = 1;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*) &n, sizeof(n));

    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        _fh_close(f);
        return -1;
    }

    if (type == SOCK_STREAM) {
        int ret;

        ret = listen(s, LISTEN_BACKLOG);
        if (ret < 0) {
            _fh_close(f);
            return -1;
        }
    }
    snprintf(f->name, sizeof(f->name), "%d(any-server:%s%d)", _fh_to_int(f), type != SOCK_STREAM ? "udp:" : "", port);
    D( "socket_inaddr_server: port %d type %s => fd %d\n", port, type != SOCK_STREAM ? "udp" : "tcp", _fh_to_int(f));
    return _fh_to_int(f);
}

static int _sdb_socket_accept(int serverfd, struct sockaddr* addr, socklen_t *addrlen) {
    FH serverfh = _fh_from_int(serverfd);
    FH fh;

    if (!serverfh || serverfh->clazz != &_fh_socket_class) {
        D( "sdb_socket_accept: invalid fd %d\n", serverfd);
        return -1;
    }

    fh = _fh_alloc(&_fh_socket_class);
    if (!fh) {
        D( "sdb_socket_accept: not enough memory to allocate accepted socket descriptor\n");
        return -1;
    }

    fh->fh_socket = accept(serverfh->fh_socket, addr, addrlen);
    if (fh->fh_socket == INVALID_SOCKET) {
        _fh_close(fh);
        D( "sdb_socket_accept: accept on fd %d return error %ld\n", serverfd, GetLastError());
        return -1;
    }

    snprintf(fh->name, sizeof(fh->name), "%d(accept:%s)", _fh_to_int(fh), serverfh->name);
    D( "sdb_socket_accept on fd %d returns fd %d\n", serverfd, _fh_to_int(fh));
    return _fh_to_int(fh);
}

static void _disable_tcp_nagle(int fd) {
    FH fh = _fh_from_int(fd);
    int on;

    if (!fh || fh->clazz != &_fh_socket_class)
        return;

    setsockopt(fh->fh_socket, IPPROTO_TCP, TCP_NODELAY, (const char*) &on, sizeof(on));
}

/**************************************************************************/
/**************************************************************************/
/*****                                                                *****/
/*****    emulated socketpairs                                       *****/
/*****                                                                *****/
/**************************************************************************/
/**************************************************************************/

/* we implement socketpairs directly in use space for the following reasons:
 *   - it avoids copying data from/to the Nt kernel
 *   - it allows us to implement fdevent hooks easily and cheaply, something
 *     that is not possible with standard Win32 pipes !!
 *
 * basically, we use two circular buffers, each one corresponding to a given
 * direction.
 *
 * each buffer is implemented as two regions:
 *
 *   region A which is (a_start,a_end)
 *   region B which is (0, b_end)  with b_end <= a_start
 *
 * an empty buffer has:  a_start = a_end = b_end = 0
 *
 * a_start is the pointer where we start reading data
 * a_end is the pointer where we start writing data, unless it is BUFFER_SIZE,
 * then you start writing at b_end
 *
 * the buffer is full when  b_end == a_start && a_end == BUFFER_SIZE
 *
 * there is room when b_end < a_start || a_end < BUFER_SIZE
 *
 * when reading, a_start is incremented, it a_start meets a_end, then
 * we do:  a_start = 0, a_end = b_end, b_end = 0, and keep going on..
 */

#define  BIP_BUFFER_SIZE   4096

#if 0
#include <stdio.h>
#  define  BIPD(x)      D x
#  define  BIPDUMP   bip_dump_hex

static void bip_dump_hex( const unsigned char* ptr, size_t len )
{
    int nn, len2 = len;

    if (len2 > 8) len2 = 8;

    for (nn = 0; nn < len2; nn++)
    printf("%02x", ptr[nn]);
    printf("  ");

    for (nn = 0; nn < len2; nn++) {
        int c = ptr[nn];
        if (c < 32 || c > 127)
        c = '.';
        printf("%c", c);
    }
    printf("\n");
    fflush(stdout);
}

#else
#  define  BIPD(x)        do {} while (0)
#  define  BIPDUMP(p,l)   BIPD(p)
#endif


static void bip_buffer_init(BipBuffer buffer) {
    D( "bit_buffer_init %p\n", buffer);
    buffer->a_start = 0;
    buffer->a_end = 0;
    buffer->b_end = 0;
    buffer->can_write = 1;
    buffer->can_read = 0;
    buffer->fdin = 0;
    buffer->fdout = 0;
    buffer->closed = 0;
    buffer->evt_write = CreateEvent(NULL, TRUE, TRUE, NULL);
    buffer->evt_read = CreateEvent(NULL, TRUE, FALSE, NULL);
    InitializeCriticalSection(&buffer->lock);
}

static void bip_buffer_close(BipBuffer bip) {
    bip->closed = 1;

    if (!bip->can_read) {
        SetEvent(bip->evt_read);
    }
    if (!bip->can_write) {
        SetEvent(bip->evt_write);
    }
}

static void bip_buffer_done(BipBuffer bip) {
    BIPD(( "bip_buffer_done: %d->%d\n", bip->fdin, bip->fdout ));
    CloseHandle(bip->evt_read);
    CloseHandle(bip->evt_write);
    DeleteCriticalSection(&bip->lock);
}

static int bip_buffer_write(BipBuffer bip, const void* src, int len) {
    int avail, count = 0;

    if (len <= 0)
        return 0;

    BIPD(( "bip_buffer_write: enter %d->%d len %d\n", bip->fdin, bip->fdout, len ));
    BIPDUMP( src, len);

    EnterCriticalSection(&bip->lock);

    while (!bip->can_write) {
        int ret;
        LeaveCriticalSection(&bip->lock);

        if (bip->closed) {
            errno = EPIPE;
            return -1;
        }
        /* spinlocking here is probably unfair, but let's live with it */
        ret = WaitForSingleObject(bip->evt_write, INFINITE);
        if (ret != WAIT_OBJECT_0) { /* buffer probably closed */
            D(
                    "bip_buffer_write: error %d->%d WaitForSingleObject returned %d, error %ld\n", bip->fdin, bip->fdout, ret, GetLastError());
            return 0;
        }
        if (bip->closed) {
            errno = EPIPE;
            return -1;
        }
        EnterCriticalSection(&bip->lock);
    }

    BIPD(( "bip_buffer_write: exec %d->%d len %d\n", bip->fdin, bip->fdout, len ));

    avail = BIP_BUFFER_SIZE - bip->a_end;
    if (avail > 0) {
        /* we can append to region A */
        if (avail > len)
            avail = len;

        memcpy(bip->buff + bip->a_end, src, avail);
        src += avail;
        count += avail;
        len -= avail;

        bip->a_end += avail;
        if (bip->a_end == BIP_BUFFER_SIZE && bip->a_start == 0) {
            bip->can_write = 0;
            ResetEvent(bip->evt_write);
            goto Exit;
        }
    }

    if (len == 0)
        goto Exit;

    avail = bip->a_start - bip->b_end;
    assert( avail > 0);
    /* since can_write is TRUE */

    if (avail > len)
        avail = len;

    memcpy(bip->buff + bip->b_end, src, avail);
    count += avail;
    bip->b_end += avail;

    if (bip->b_end == bip->a_start) {
        bip->can_write = 0;
        ResetEvent(bip->evt_write);
    }

    Exit:
    assert( count > 0);

    if (!bip->can_read) {
        bip->can_read = 1;
        SetEvent(bip->evt_read);
    }

    BIPD(
            ( "bip_buffer_write: exit %d->%d count %d (as=%d ae=%d be=%d cw=%d cr=%d\n", bip->fdin, bip->fdout, count, bip->a_start, bip->a_end, bip->b_end, bip->can_write, bip->can_read ));
    LeaveCriticalSection(&bip->lock);

    return count;
}

static int bip_buffer_read(BipBuffer bip, void* dst, int len) {
    int avail, count = 0;

    if (len <= 0)
        return 0;

    BIPD(( "bip_buffer_read: enter %d->%d len %d\n", bip->fdin, bip->fdout, len ));

    EnterCriticalSection(&bip->lock);
    while (!bip->can_read) {
#if 0
        LeaveCriticalSection( &bip->lock );
        errno = EAGAIN;
        return -1;
#else
        int ret;
        LeaveCriticalSection(&bip->lock);

        if (bip->closed) {
            errno = EPIPE;
            return -1;
        }

        ret = WaitForSingleObject(bip->evt_read, INFINITE);
        if (ret != WAIT_OBJECT_0) { /* probably closed buffer */
            D(
                    "bip_buffer_read: error %d->%d WaitForSingleObject returned %d, error %ld\n", bip->fdin, bip->fdout, ret, GetLastError());
            return 0;
        }
        if (bip->closed) {
            errno = EPIPE;
            return -1;
        }
        EnterCriticalSection(&bip->lock);
#endif
    }

    BIPD(( "bip_buffer_read: exec %d->%d len %d\n", bip->fdin, bip->fdout, len ));

    avail = bip->a_end - bip->a_start;
    assert( avail > 0);
    /* since can_read is TRUE */

    if (avail > len)
        avail = len;

    memcpy(dst, bip->buff + bip->a_start, avail);
    dst += avail;
    count += avail;
    len -= avail;

    bip->a_start += avail;
    if (bip->a_start < bip->a_end)
        goto Exit;

    bip->a_start = 0;
    bip->a_end = bip->b_end;
    bip->b_end = 0;

    avail = bip->a_end;
    if (avail > 0) {
        if (avail > len)
            avail = len;
        memcpy(dst, bip->buff, avail);
        count += avail;
        bip->a_start += avail;

        if (bip->a_start < bip->a_end)
            goto Exit;

        bip->a_start = bip->a_end = 0;
    }

    bip->can_read = 0;
    ResetEvent(bip->evt_read);

    Exit:
    assert( count > 0);

    if (!bip->can_write) {
        bip->can_write = 1;
        SetEvent(bip->evt_write);
    }

    BIPDUMP( (const unsigned char*)dst - count, count);
    BIPD(
            ( "bip_buffer_read: exit %d->%d count %d (as=%d ae=%d be=%d cw=%d cr=%d\n", bip->fdin, bip->fdout, count, bip->a_start, bip->a_end, bip->b_end, bip->can_write, bip->can_read ));
    LeaveCriticalSection(&bip->lock);

    return count;
}

typedef struct SocketPairRec_ {
    BipBufferRec a2b_bip;
    BipBufferRec b2a_bip;
    FH a_fd;
    int used;

} SocketPairRec;

void _fh_socketpair_init(FH f) {
    f->fh_pair = NULL;
}

static int _fh_socketpair_close(FH f) {
    if (f->fh_pair) {
        SocketPair pair = f->fh_pair;

        if (f == pair->a_fd) {
            pair->a_fd = NULL;
        }

        bip_buffer_close(&pair->b2a_bip);
        bip_buffer_close(&pair->a2b_bip);

        if (--pair->used == 0) {
            bip_buffer_done(&pair->b2a_bip);
            bip_buffer_done(&pair->a2b_bip);
            free(pair);
        }
        f->fh_pair = NULL;
    }
    return 0;
}

static int _fh_socketpair_lseek(FH f, int pos, int origin) {
    errno = ESPIPE;
    return -1;
}

static int _fh_socketpair_read(FH f, void* buf, int len) {
    SocketPair pair = f->fh_pair;
    BipBuffer bip;

    if (!pair)
        return -1;

    if (f == pair->a_fd)
        bip = &pair->b2a_bip;
    else
        bip = &pair->a2b_bip;

    return bip_buffer_read(bip, buf, len);
}

static int _fh_socketpair_write(FH f, const void* buf, int len) {
    SocketPair pair = f->fh_pair;
    BipBuffer bip;

    if (!pair)
        return -1;

    if (f == pair->a_fd)
        bip = &pair->a2b_bip;
    else
        bip = &pair->b2a_bip;

    return bip_buffer_write(bip, buf, len);
}

static void _fh_socketpair_hook(FH f, int event, EventHook hook); /* forward */

static const FHClassRec _fh_socketpair_class = { _fh_socketpair_init, _fh_socketpair_close, _fh_socketpair_lseek,
        _fh_socketpair_read, _fh_socketpair_write, _fh_socketpair_hook };

static int _sdb_socketpair(int sv[2]) {
    FH fa, fb;
    SocketPair pair;

    fa = _fh_alloc(&_fh_socketpair_class);
    fb = _fh_alloc(&_fh_socketpair_class);

    if (!fa || !fb)
        goto Fail;

    pair = malloc(sizeof(*pair));
    if (pair == NULL) {
        D("sdb_socketpair: not enough memory to allocate pipes\n");
        goto Fail;
    }

    bip_buffer_init(&pair->a2b_bip);
    bip_buffer_init(&pair->b2a_bip);

    fa->fh_pair = pair;
    fb->fh_pair = pair;
    pair->used = 2;
    pair->a_fd = fa;

    sv[0] = _fh_to_int(fa);
    sv[1] = _fh_to_int(fb);

    pair->a2b_bip.fdin = sv[0];
    pair->a2b_bip.fdout = sv[1];
    pair->b2a_bip.fdin = sv[1];
    pair->b2a_bip.fdout = sv[0];

    snprintf(fa->name, sizeof(fa->name), "%d(pair:%d)", sv[0], sv[1]);
    snprintf(fb->name, sizeof(fb->name), "%d(pair:%d)", sv[1], sv[0]);
    D( "sdb_socketpair: returns (%d, %d)\n", sv[0], sv[1]);
    return 0;

    Fail: _fh_close(fb);
    _fh_close(fa);
    return -1;
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


/**  FILE EVENT HOOKS
 **/

static void _event_file_prepare(EventHook hook) {
    if (hook->wanted & (FDE_READ | FDE_WRITE)) {
        /* we can always read/write */
        hook->ready |= hook->wanted & (FDE_READ | FDE_WRITE);
    }
}

static int _event_file_peek(EventHook hook) {
    return (hook->wanted & (FDE_READ | FDE_WRITE));
}

static void _fh_file_hook(FH f, int events, EventHook hook) {
    hook->h = f->fh_handle;
    hook->prepare = _event_file_prepare;
    hook->peek = _event_file_peek;
}

/** SOCKET EVENT HOOKS
 **/

static void _event_socket_verify(EventHook hook, WSANETWORKEVENTS* evts) {
    if (evts->lNetworkEvents & (FD_READ | FD_ACCEPT | FD_CLOSE)) {
        if (hook->wanted & FDE_READ)
            hook->ready |= FDE_READ;
        if ((evts->iErrorCode[FD_READ] != 0) && hook->wanted & FDE_ERROR)
            hook->ready |= FDE_ERROR;
    }
    if (evts->lNetworkEvents & (FD_WRITE | FD_CONNECT | FD_CLOSE)) {
        if (hook->wanted & FDE_WRITE)
            hook->ready |= FDE_WRITE;
        if ((evts->iErrorCode[FD_WRITE] != 0) && hook->wanted & FDE_ERROR)
            hook->ready |= FDE_ERROR;
    }
    if (evts->lNetworkEvents & FD_OOB) {
        if (hook->wanted & FDE_ERROR)
            hook->ready |= FDE_ERROR;
    }
}

static void _event_socket_prepare(EventHook hook) {
    WSANETWORKEVENTS evts;

    /* look if some of the events we want already happened ? */
    if (!WSAEnumNetworkEvents(hook->fh->fh_socket, NULL, &evts))
        _event_socket_verify(hook, &evts);
}

static int _socket_wanted_to_flags(int wanted) {
    int flags = 0;
    if (wanted & FDE_READ)
        flags |= FD_READ | FD_ACCEPT | FD_CLOSE;

    if (wanted & FDE_WRITE)
        flags |= FD_WRITE | FD_CONNECT | FD_CLOSE;

    if (wanted & FDE_ERROR)
        flags |= FD_OOB;

    return flags;
}

static int _event_socket_start(EventHook hook) {
    /* create an event which we're going to wait for */
    FH fh = hook->fh;
    long flags = _socket_wanted_to_flags(hook->wanted);

    hook->h = fh->event;
    if (hook->h == INVALID_HANDLE_VALUE) {
        D( "_event_socket_start: no event for %s\n", fh->name);
        return 0;
    }

    if (flags != fh->mask) {
        D( "_event_socket_start: hooking %s for %x (flags %ld)\n", hook->fh->name, hook->wanted, flags);
        if (WSAEventSelect(fh->fh_socket, hook->h, flags)) {
            D( "_event_socket_start: WSAEventSelect() for %s failed, error %d\n", hook->fh->name, WSAGetLastError());
            CloseHandle(hook->h);
            hook->h = INVALID_HANDLE_VALUE;
            exit(1);
            return 0;
        }
        fh->mask = flags;
    }
    return 1;
}

static void _event_socket_stop(EventHook hook) {
    hook->h = INVALID_HANDLE_VALUE;
}

static int _event_socket_check(EventHook hook) {
    int result = 0;
    FH fh = hook->fh;
    WSANETWORKEVENTS evts;

    if (!WSAEnumNetworkEvents(fh->fh_socket, hook->h, &evts)) {
        _event_socket_verify(hook, &evts);
        result = (hook->ready != 0);
        if (result) {
            ResetEvent(hook->h);
        }
    }
    D( "_event_socket_check %s returns %d\n", fh->name, result);
    return result;
}

static int _event_socket_peek(EventHook hook) {
    WSANETWORKEVENTS evts;
    FH fh = hook->fh;

    /* look if some of the events we want already happened ? */
    if (!WSAEnumNetworkEvents(fh->fh_socket, NULL, &evts)) {
        _event_socket_verify(hook, &evts);
        if (hook->ready)
            ResetEvent(hook->h);
    }

    return hook->ready != 0;
}

static void _fh_socket_hook(FH f, int events, EventHook hook) {
    hook->prepare = _event_socket_prepare;
    hook->start = _event_socket_start;
    hook->stop = _event_socket_stop;
    hook->check = _event_socket_check;
    hook->peek = _event_socket_peek;

    _event_socket_start(hook);
}

/** SOCKETPAIR EVENT HOOKS
 **/

static void _event_socketpair_prepare(EventHook hook) {
    FH fh = hook->fh;
    SocketPair pair = fh->fh_pair;
    BipBuffer rbip = (pair->a_fd == fh) ? &pair->b2a_bip : &pair->a2b_bip;
    BipBuffer wbip = (pair->a_fd == fh) ? &pair->a2b_bip : &pair->b2a_bip;

    if (hook->wanted & FDE_READ && rbip->can_read)
        hook->ready |= FDE_READ;

    if (hook->wanted & FDE_WRITE && wbip->can_write)
        hook->ready |= FDE_WRITE;
}

static int _event_socketpair_start(EventHook hook) {
    FH fh = hook->fh;
    SocketPair pair = fh->fh_pair;
    BipBuffer rbip = (pair->a_fd == fh) ? &pair->b2a_bip : &pair->a2b_bip;
    BipBuffer wbip = (pair->a_fd == fh) ? &pair->a2b_bip : &pair->b2a_bip;

    if (hook->wanted == FDE_READ)
        hook->h = rbip->evt_read;

    else if (hook->wanted == FDE_WRITE)
        hook->h = wbip->evt_write;

    else {
        D("_event_socketpair_start: can't handle FDE_READ+FDE_WRITE\n");
        return 0;
    }
    D( "_event_socketpair_start: hook %s for %x wanted=%x\n", hook->fh->name, _fh_to_int(fh), hook->wanted);
    return 1;
}

static int _event_socketpair_peek(EventHook hook) {
    _event_socketpair_prepare(hook);
    return hook->ready != 0;
}

static void _fh_socketpair_hook(FH fh, int events, EventHook hook) {
    hook->prepare = _event_socketpair_prepare;
    hook->start = _event_socketpair_start;
    hook->peek = _event_socketpair_peek;
}

static void _sdb_sysdeps_init(void) {
    //re define mutex variable & initialized
#undef SDB_MUTEX
#define  SDB_MUTEX(x)  InitializeCriticalSection( & x );
    SDB_MUTEX(dns_lock)
    SDB_MUTEX(socket_list_lock)
    SDB_MUTEX(transport_lock)
    SDB_MUTEX(local_transports_lock)
    SDB_MUTEX(usb_lock)
    SDB_MUTEX(D_lock)
    SDB_MUTEX(_win32_lock);
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
    .sdb_open_mode = _sdb_open_mode,
    .unix_open = _unix_open,
    .sdb_creat = _sdb_creat,
    .sdb_read = _sdb_read,
    .sdb_write = _sdb_write,
    .sdb_lseek = _sdb_lseek,
    .sdb_shutdown = _sdb_shutdown,
    .sdb_close = _sdb_close,
    .close_on_exec = _close_on_exec,
    .sdb_unlink = _sdb_unlink,
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
    .socket_loopback_client = _socket_loopback_client,
    .socket_network_client = _socket_network_client,
    .socket_loopback_server = _socket_loopback_server,
    .socket_inaddr_any_server = _socket_inaddr_any_server
};
