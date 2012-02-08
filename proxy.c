#include "http.h"

#if defined(_VRELAY)
int reverse_proxy()
{
    const char *host = "222.65.120.50";
    int port = 11080;
    printf("<%d> %s %s:%d\n", _getpid(), __FUNCTION__, host, port);
    while(++hits) {
        int ret, sock;
        struct sockaddr_in host_addr;

        /* create socket */
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("socket");
            break;
        }

        /* bind host */
        host_addr.sin_family = AF_INET;
        host_addr.sin_port = htons(port);
        host_addr.sin_addr.s_addr = inet_addr(host);
        ret = connect(sock, (struct sockaddr *)&host_addr, sizeof(host_addr));
        if (ret < 0) {
            perror("connect");
        }
        else {
            char buf[4096+4];
            printf("(%d) %s:%d connected\n", sock, host, port);
            strcpy_s(buf, sizeof(buf), "PING");
            strcat_s(buf, sizeof(buf), uid);
            ret = send(sock, buf, strlen(buf), 0);
            ret = recv(sock, buf, 4, 0);//wait here
            printf("ret = %d\n", ret);
            if (ret < 0) {
                perror("read");
            }
            else {
                buf[ret] = 0;
                printf("%s\n", buf);//dump echo

                if (*(int *)buf == *(int *)"PANG") {
                    http_req_t *req = (http_req_t *)malloc(sizeof(http_req_t));
                    req->socketfd = sock;req->bufleft = 0;
                    printf("connect %d\n", sock);
                    _fork(req, child);continue;//ok. go next
                }
            }
        }
        //sock = 0;//create new idle socket, and wait PING-PANG
        closesocket(sock);
        Sleep(20*1000);
    }

    printf("<%d> %s end.\n", _getpid(), __FUNCTION__);
    return 0;
}

int peer_proxy()
{
    const char *host = "222.65.120.50";
    int ret, sock, port = 11080+1;//p2p host
    struct sockaddr_in host_addr, svr_addr, cli_addr, out_addr;
    http_req_t *req = (http_req_t *)malloc(sizeof(http_req_t));
    printf("<%d> %s %s:%d\n", _getpid(), __FUNCTION__, host, port);

    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(port);
    host_addr.sin_addr.s_addr = inet_addr(host);

    /* create udp socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");return sock;
    }

    /* bind local server */
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    svr_addr.sin_port = htons(1984+1);
    ret = bind(sock, (struct sockaddr *)&svr_addr, sizeof(svr_addr));
    if (ret < 0) {
        perror("bind");return ret;
    }

    //login p2p host
    {char buf[4096+4];
    strcpy_s(buf, sizeof(buf), "PING");
    strcat_s(buf, sizeof(buf), uid);
    ret = sendto(sock, buf, strlen(buf), 0, (struct sockaddr *)&host_addr, sizeof(host_addr));
    printf("ret = %d\n", ret);}

    while(++hits) {
        socklen_t length=sizeof(req->hole);
        printf("<%d> (h%d) Listening %s:%d\n", _getpid(), sock, (char *)inet_ntoa(svr_addr.sin_addr),ntohs(svr_addr.sin_port));
        ret = recvfrom(sock, req->mainbuf, sizeof(req->mainbuf), 0, (struct sockaddr*)&req->hole, (socklen_t*)&length);
        if (ret <= 0) {
            perror("recvfrom");
            Sleep(20*1000);
            //return ret;
        }
        else {
            char *p = req->mainbuf, *q;
            printf("<%d> (n%d) recvfrom %s:%d\n", _getpid(), ret, (char *)inet_ntoa(req->hole.sin_addr),ntohs(req->hole.sin_port));
            //req->mainbuf[ret] = 0;
            dump(req->mainbuf, (ret > 4096)?16:ret);
            if (*(int *)p == *(int *)"PANG") {//login ok, check if from host
                p += 4;
                q = p;while(*p && *p != ':') p++;
                if (*p == ':') {
                    *p++ = 0;
                    printf("@@@login ok, out session {%s:%d}\n", q, __atoi(p, &req->mainbuf[ret]));
                    out_addr.sin_family = AF_INET;
                    out_addr.sin_port = htons(__atoi(p, &req->mainbuf[ret]));
                    out_addr.sin_addr.s_addr = inet_addr(q);
                }
            }
            else if (*(int *)p == *(int *)"P2P ") {
                p += 4;
                q = p;while(*p && *p != ':') p++;
                if (*p == ':') {
                    *p++ = 0;
                    printf("@@@ok. punch to client session {%s:%d}\n", q, __atoi(p, &req->mainbuf[ret]));
                    cli_addr.sin_family = AF_INET;
                    cli_addr.sin_port = htons(__atoi(p, &req->mainbuf[ret]));
                    cli_addr.sin_addr.s_addr = inet_addr(q);
                    sprintf_s(req->mainbuf, sizeof(req->mainbuf), "P2P %s:%d", (char *)inet_ntoa(out_addr.sin_addr),ntohs(out_addr.sin_port));
                    ret = sendto(sock, req->mainbuf, strlen(req->mainbuf), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));
                }
            }
            else if (*(int *)p == *(int *)"GET ") {//only it
                req->socketfd = sock;req->bufleft = ret;
                _fork(req, child);
                req = (http_req_t *)malloc(sizeof(http_req_t));
            }
        }
    }

    closesocket(sock);
    printf("%s end.\n", __FUNCTION__);
    return 0;
}
#endif
