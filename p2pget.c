#ifdef _WIN32
#include <stdio.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#ifndef _DEBUG
#pragma comment (linker, "/ENTRY:mainCRTStartup /subsystem:windows")
#endif

#define getpid()    GetCurrentThreadId()
#define PIPE_BUF    (4096)
typedef int socklen_t;
void dump(char *buf, int len);

#else//_GNUC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define closesocket close
#ifndef _DEBUG
#define _DEBUG
#endif
#endif

int __atoi(const char *s, const char *t)
{
    int i = 0;
    while(s < t) {
        i *= 10;
        if (*s >= '0' && *s <= '9') i += (*s - '0');
        else break;
        s++;
    }
    return i;
}

int main(int argc, const char *argv[])
{
    char *server = 0, *uri = 0;
    int ret, sock, port = 11081;
    struct sockaddr_in svr_addr, cli_addr;
    char *p = strdup(argv[1]), *q, reqbuf[PIPE_BUF+4];
#ifdef _WIN32
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2,2), &wsadata) == SOCKET_ERROR) {
        perror("WSAStartup");
        return -1;
    }
#endif
    //parse url
    //printf("%s\n", p);
    if (0 == strncmp(p, "http://", sizeof("http://")-1)) {
        p += sizeof("http://")-1;
        q = p;while(*p && (*p != ':' && *p != '/')) p++;
        if (*p == ':') {
            server = q;*p = 0;
            q = ++p;while(*p && *p != '/') p++;
            if (*p) port = __atoi(q, p);
            if (*p == '/') {
                uri = q = p++;
            }
        }
    }
    printf("%s**%d**%s\n", server, port, uri);
    if (!server || !port || !uri) return -1;

    //make http request
    p = reqbuf;
    p += sprintf(p, "GET %s HTTP/1.1\r\n", uri);
    p += sprintf(p, "Range: bytes=1-10\r\n");
    p += sprintf(p, "\r\n");

    //sendto
    sock=socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket"); return 0;
    }

#if 0
    /* bind local server */
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    svr_addr.sin_port = htons(1984+2);
    ret = bind(sock, (struct sockaddr *)&svr_addr, sizeof(svr_addr));
    if (ret < 0) {
        perror("bind");return ret;
    }
#endif

    svr_addr.sin_family            = AF_INET;
    svr_addr.sin_addr.s_addr = inet_addr(server);
    svr_addr.sin_port                = htons(port);
    ret = sendto(sock, reqbuf, strlen(reqbuf), 0, (struct sockaddr *)&svr_addr, sizeof(svr_addr));
    printf("ret = %d\n", ret);

    while(1) {
        char mainbuf[PIPE_BUF+4];
        socklen_t length=sizeof(cli_addr);
        ret = recvfrom(sock, mainbuf, PIPE_BUF, 0, (struct sockaddr*)&cli_addr, (socklen_t*)&length);
        if (ret < 0) {
            perror("recvfrom");return 0;
        }
        printf("<%d> (n%d) recvfrom %s:%d\n", getpid(), ret, (char *)inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));
        dump(mainbuf, ret);

        p = mainbuf;
        if (*(int *)p == *(int *)"P2P ") {
            p += 4;
            q = p;while(*p && *p != ':') p++;
            if (*p == ':') {
                *p++ = 0;
                printf("@@@haha. punch from remote session {%s:%d}\n", q, __atoi(p, &mainbuf[ret]));
                svr_addr.sin_family = AF_INET;
                svr_addr.sin_port = htons(__atoi(p, &mainbuf[ret]));
                svr_addr.sin_addr.s_addr = inet_addr(q);
                ret = sendto(sock, reqbuf, strlen(reqbuf), 0, (struct sockaddr *)&svr_addr, sizeof(svr_addr));
                printf("ret = %d\n", ret);
            }
        }
    }
    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
