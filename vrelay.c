#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

void dump(const char *buf, int len);

typedef struct {
    unsigned int hashID;
    int sockfd;//tcp
    struct sockaddr_in hole;//udp
    pthread_mutex_t mutex;
    pthread_cond_t cond;
}relay_t;

relay_t slots[128];

void _slots_init()
{
    int i;
    for (i = 0; i < sizeof(slots)/sizeof(relay_t); i++) {
        memset(&slots[i], 0, sizeof(slots[i]));
        pthread_mutex_init(&slots[i].mutex, 0);
        pthread_cond_init(&slots[i].cond, 0);
    }
}

unsigned int _hash(char *str, char *end)
{
    unsigned int hash   = 0;
    unsigned int x      = 0;

    while (str < end) {
        hash = (hash << 4) + (*str++);
        if  ((x = hash & 0xF0000000L) != 0) {
            hash ^= (x >> 24);
            hash &= ~x;
        }
    }

    return (hash & 0x7FFFFFFF);
}

void _fill(unsigned int hashID, int sockfd)
{
    int i, j = 0;
    for (i = 0; i < sizeof(slots)/sizeof(relay_t); i++) {//hash ID later
        if (slots[i].hashID == hashID) {//found ID
            j = i;break;
        }
        else if (!j && !slots[i].hashID) j = i;//first empty slot
    }
    if (j < sizeof(slots)/sizeof(relay_t)) {
        pthread_mutex_lock(&slots[j].mutex);
        if (slots[j].sockfd) {
            printf("force close %d\n", slots[j].sockfd);
            close(slots[j].sockfd);
        }
        slots[j].sockfd = sockfd;
        slots[j].hashID = hashID;
        printf("%s [%d] %u : %d\n", __FUNCTION__, j, slots[j].hashID, slots[j].sockfd);
        pthread_cond_broadcast(&slots[j].cond);
        pthread_mutex_unlock(&slots[j].mutex);
    }
}

int _take(unsigned int hashID)
{
    int i;
    for (i = 0; i < sizeof(slots)/sizeof(relay_t); i++) {//hash ID later
        if (slots[i].hashID == hashID) {//found ID
            pthread_mutex_lock(&slots[i].mutex);
            if (!slots[i].sockfd) {//faint, reverse connection not ready
                printf("waiting ...\n");
                struct timespec ts = {
                    .tv_sec = time(0) + 3,//abs
                    .tv_nsec = 0,
                };
                pthread_cond_timedwait(&slots[i].cond, &slots[i].mutex, &ts);
            }
            int sockfd = slots[i].sockfd;slots[i].sockfd = 0;//take over
            pthread_mutex_unlock(&slots[i].mutex);
            return sockfd;
        }
    }
    return -1;
}

typedef int (*cb_t)(int);

int _listen(int port, cb_t cb)
{
    int listenfd=socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        perror("socket"); return listenfd;
    }

    int optval = 1, optlen = sizeof(optval);
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, optlen);

    struct sockaddr_in srv_addr, cli_addr;
    srv_addr.sin_family            = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srv_addr.sin_port                = htons(port);

    if (bind(listenfd, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0 || listen(listenfd, 64) < 0) {
        perror("listen");return -1;
    }

    while(1) {
        socklen_t length=sizeof(cli_addr);
        printf("<%d> (%d) Listening %s:%d\n", getpid(), listenfd, (char *)inet_ntoa(srv_addr.sin_addr),ntohs(srv_addr.sin_port));
        int sockfd=accept(listenfd, (struct sockaddr*)&cli_addr, (socklen_t*)&length);
        if (sockfd < 0) {
            perror("accept");return sockfd;
        }
        printf("<%d> (%d) Accepted %s:%d\n", getpid(), sockfd, (char *)inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));
        if (cb) cb(sockfd);
        fflush(stdout);
        //close(sockfd);
    }

    close(listenfd);
    return 0;
}

int _pending(int s, int to)
{
    struct timeval tv = {
        .tv_sec = to,
        .tv_usec = 0,
    };
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(s, &rfds);
    select(s + 1, &rfds, NULL, NULL, &tv);
    return FD_ISSET(s, &rfds);
}

int _bind(const char *host, int port)
{
    int sockfd;
    /* create socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");return sockfd;
    }

    /* bind host */
    struct sockaddr_in host_addr;
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(port);
    host_addr.sin_addr.s_addr = inet_addr(host);
    int ret = connect(sockfd, (struct sockaddr *)&host_addr, sizeof(host_addr));
    if (ret < 0) {
        perror("connect");return ret;
    }
    printf("<%d> (%d) Connected %s:%d\n", getpid(), sockfd, (char *)inet_ntoa(host_addr.sin_addr),ntohs(host_addr.sin_port));
    return sockfd;
}

int _ping(int sockfd)
{
    char buf[PIPE_BUF+4];
    int ret = _pending(sockfd, 2);
    printf("ret = %d\n", ret);
    if (ret > 0) {
        ret = recv(sockfd, buf, PIPE_BUF, 0);
        if (ret < 0) {
            perror("read");return ret;
        }
    }
    else {
        printf("timeout\n");ret = 0;
    }

    //buf[ret] = 0;printf("%s\n", buf);
    dump(buf, ret);

    if (*(int *)buf == *(int *)"PING") {
        _fill(_hash(&buf[4], &buf[ret]), sockfd);
    }
    else close(sockfd);
    return 0;
}

void * _waitforping(void *arg)
{
    int port = 11080;
    int ret = _listen(port, _ping);
    printf("%s %d\n", __FUNCTION__, ret);
    return 0;
}

int _pang(int sockfd)
{
    char buf[PIPE_BUF+4];
    int len = recv(sockfd, buf, PIPE_BUF, 0);
    if (len < 0) {
        perror("read");return len;
    }
    buf[len] = 0;printf("%s\n", buf);
    if (*(int *)buf == *(int *)"GET ") {//maybe http request header
        char *p = &buf[4], *q;
        while(*p == '/' || *p == ' ') p++;q = p;
        while(*p && (*p != '/' && *p != ' ' && *p != '.')) p++;//parse sub homepage from url
        dump(q, p-q);
        int socks[2] = {sockfd, _take(_hash(q, p))};//shake sock hands
        if (socks[1] <= 0) {
            printf("Not Found ID\n");
        }
        else {
            char tmp[] = "PANG";
            int ret = send(socks[1], tmp, strlen(tmp), 0);//echo reverse connection
            if (ret < 0) {
                perror("send");
            }
            else {
                ret = send(socks[1], buf, len, 0);//relay first package (http request header)
                if (ret > 0 && 0 == fork()) {//child
                    _rproxy(socks);
                    exit(0);
                }
            }
            close(socks[1]);
        }
    }
    close(sockfd);
    return 0;
}

#define _MAX_BUF    (PIPE_BUF*256)
int _rproxy(int socks[])
{
    int ret = 0;
    unsigned char buf[_MAX_BUF+4];
#define MAX(a, b) ((a > b)?a:b)
    printf("<%d> %s %d <--> %d\n", getpid(), __FUNCTION__, socks[0], socks[1]);
    while(socks[0] && socks[1]) {
        fd_set rset;
        FD_ZERO(&rset);
        FD_SET(socks[0],&rset);
        FD_SET(socks[1],&rset);
        ret = select(MAX(socks[0], socks[1])+1, &rset, NULL, NULL, NULL);
        if (ret == 0) break;

        if(FD_ISSET(socks[0], &rset)) {
            ret=recv(socks[0], buf, _MAX_BUF, 0);
            //printf("(%d) recv return %d\n", socks[0], ret);
            if (ret <= 0) break;
            //dump(buf, ret);
            if (send(socks[1], buf, ret, 0) < 0) {
                break;
            }
            //printf("<%d>\t(%d) %d --> %d\n", getpid(), ret, socks[0], socks[1]);
        }
        else if(FD_ISSET(socks[1], &rset)) {
            ret=recv(socks[1], buf, _MAX_BUF, 0);
            //printf("(%d) recv return %d\n", socks[1], ret);
            if(ret <= 0 ) break;
            //dump(buf, ret);
            if(send(socks[0], buf, ret, 0) < 0) {
                break;
            }
            //printf("<%d>\t(%d) %d --> %d\n", getpid(), ret, socks[1], socks[0]);
        }
    }
    printf("<%d> %s end.\n", getpid(), __FUNCTION__);
    fflush(stdout);
    return 0;
}

void _sig(int sig)
{
    printf("sig=%d\n", sig);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (0 != fork()) return 0;

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGTERM, _sig);
    signal(SIGINT, _sig);

    //setsid();
    setpgrp();

    _slots_init();

    pthread_t thd;
    void * _waitforhole(void *arg);
    pthread_create(&thd, 0, _waitforhole, NULL);
    pthread_create(&thd, 0, _waitforping, NULL);

    _listen(8080, _pang);

    pthread_join(thd, NULL);
    return 0;
}

void _fillhole(unsigned int hashID, struct sockaddr_in *addr)
{
    int i, j = 0;
    for (i = 0; i < sizeof(slots)/sizeof(relay_t); i++) {//hash ID later
        if (slots[i].hashID == hashID) {//found ID
            j = i;break;
        }
        else if (!j && !slots[i].hashID) j = i;//first empty slot
    }
    if (j < sizeof(slots)/sizeof(relay_t)) {
        pthread_mutex_lock(&slots[j].mutex);
        memcpy(&slots[j].hole, addr, sizeof(slots[j].hole));
        slots[j].hashID = hashID;
        printf("%s [%d] %u : %s:%d\n", __FUNCTION__, j, slots[j].hashID, (char *)inet_ntoa(slots[j].hole.sin_addr),ntohs(slots[j].hole.sin_port));
        pthread_mutex_unlock(&slots[j].mutex);
    }
}

struct sockaddr_in * _takehole(unsigned int hashID)
{
    int i;
    for (i = 0; i < sizeof(slots)/sizeof(relay_t); i++) {//hash ID later
        if (slots[i].hashID == hashID) {//found ID
            pthread_mutex_lock(&slots[i].mutex);
            pthread_mutex_unlock(&slots[i].mutex);
            return &slots[i].hole;
        }
    }
    return 0;
}

void * _waitforhole(void *arg)
{
    int port = 11081;
    int listenfd=socket(AF_INET, SOCK_DGRAM, 0);
    if (listenfd < 0) {
        perror("socket"); return 0;
    }

    struct sockaddr_in srv_addr, cli_addr;
    srv_addr.sin_family            = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    srv_addr.sin_port                = htons(port);

    if (bind(listenfd, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0) {
        perror("bind");return 0;
    }

    while(1) {
        char buf[PIPE_BUF+4];
        socklen_t length=sizeof(struct sockaddr_in);
        printf("<%d> (%d) Listening %s:%d\n", getpid(), listenfd, (char *)inet_ntoa(srv_addr.sin_addr),ntohs(srv_addr.sin_port));
        int ret = recvfrom(listenfd, buf, PIPE_BUF, 0, (struct sockaddr*)&cli_addr, (socklen_t*)&length);
        if (ret < 0) {
            perror("recvfrom");return 0;
        }
        printf("<%d> (n%d) recvfrom %s:%d\n", getpid(), ret, (char *)inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));
        //buf[ret] = 0;//printf("%s\n", mainbuf);//dump
        dump(buf, ret);

        if (*(int *)buf == *(int *)"PING") {
            _fillhole(_hash(&buf[4], &buf[ret]), &cli_addr);
            sprintf(buf, "PANG%s:%d", (char *)inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));
            int ret = sendto(listenfd, buf, strlen(buf), 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
            if (ret < 0) {
                perror("sendto");
            }
        }
        else if (*(int *)buf == *(int *)"GET ") {
            char *p = &buf[4], *q;
            while(*p == '/' || *p == ' ') p++;q = p;
            while(*p && (*p != '/' && *p != ' ' && *p != '.')) p++;//parse sub homepage from url
            struct sockaddr_in *host = _takehole(_hash(q, p));//shake sock hands
            if (host) {//send punch request with client session
                sprintf(buf, "P2P %s:%d", (char *)inet_ntoa(cli_addr.sin_addr),ntohs(cli_addr.sin_port));
                int ret = sendto(listenfd, buf, strlen(buf), 0, (struct sockaddr*)host, sizeof(*host));
                sprintf(buf, "P2P %s:%d", (char *)inet_ntoa(host->sin_addr),ntohs(host->sin_port));
                ret = sendto(listenfd, buf, strlen(buf), 0, (struct sockaddr*)&cli_addr, sizeof(cli_addr));
            }
            else {
                printf("Not Found ID\n");
            }
        }
        fflush(stdout);
    }

    close(listenfd);
    return 0;
}

void dump(const char *buf, int len)
{
#ifdef _DEBUG
    char fix[17];fix[16] = 0;
    printf("\n          00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f  ");
    int i, j = 0;
    if ((int)buf & 0x0f) {
        printf("\n%08x: ", (int)buf & ~0x0f);
            for(i = 0; i < ((int)buf & 0x0f); i++) {
            printf("   ");
            fix[j++] = '.';
        }
    }
    for(i = 0; i < len; i++) {
        if (((int)&buf[i] & 0x0f) == 0) printf("\n%08x: ", (int)&buf[i]);
        printf("%02x ", (unsigned char)buf[i]);
        fix[j++] = (buf[i] >= 0x20 && buf[i] <= 0x7e)?buf[i]:'.';
        if (j == 16) {
            printf(" %s", fix);j = 0;
        }
    }
    if ((int)&buf[len] & 0x0f) {
        for(i = 0; i < 16 - ((int)&buf[len] & 0x0f); i++) {
            printf("   ");
            fix[j++] = '.';
        }
        printf(" %s", fix);j = 0;
    }
    printf("\n");
#endif
}
