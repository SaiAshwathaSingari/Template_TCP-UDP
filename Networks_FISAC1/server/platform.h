/*
 * platform.h - Cross-platform compatibility for Windows (Winsock2) and Linux (POSIX)
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32

#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

typedef SOCKET socket_t;
#define SOCKET_INVALID INVALID_SOCKET
#define SHUT_RDWR SD_BOTH
#define MSG_NOSIGNAL 0

static inline int platform_init(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2,2), &wsa);
}
static inline void platform_cleanup(void) { WSACleanup(); }
static inline int platform_close(socket_t s) { return closesocket(s); }
static inline int platform_errno(void) { return WSAGetLastError(); }

/* strcasestr not available on Windows - provide it */
static char *strcasestr(const char *haystack, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
    }
    return NULL;
}

/* Windows pthreads come from mingw-w64 posix thread model */
#include <pthread.h>

/* Random token generation for Windows */
static void platform_random_bytes(unsigned char *buf, int len) {
    int i;
    /* Use CryptGenRandom or fallback to rand */
    HCRYPTPROV hProv;
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, len, buf);
        CryptReleaseContext(hProv, 0);
    } else {
        srand((unsigned)(GetTickCount() ^ GetCurrentProcessId()));
        for (i = 0; i < len; i++) buf[i] = (unsigned char)(rand() % 256);
    }
}

#else /* Linux/POSIX */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <strings.h>

typedef int socket_t;
#define SOCKET_INVALID (-1)

static inline int platform_init(void) { return 0; }
static inline void platform_cleanup(void) {}
static inline int platform_close(socket_t s) { return close(s); }
static inline int platform_errno(void) { return errno; }

static void platform_random_bytes(unsigned char *buf, int len) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        (void)!read(fd, buf, len);
        close(fd);
    } else {
        int i;
        srand((unsigned)(time(NULL) ^ getpid()));
        for (i = 0; i < len; i++) buf[i] = (unsigned char)(rand() % 256);
    }
}

#endif /* _WIN32 */

#endif /* PLATFORM_H */
