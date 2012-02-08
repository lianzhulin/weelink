// Tiny Cloud Application ported by robot lian. Based on "NWEB" by Nigel Griffiths.
#include "http.h"

char uid[MAX_PATH], root[MAX_PATH] = {0};int uidlen, hits = 0;
const char* about=
            "<html>\n<head>\n<title>Tiny Cloud Application</title>\n</head>\n"
            "<body>\n<h3>Tiny Cloud Application</h3><br>"
            "Cloud is a simple application that enables web-based browsing of the Android file system "
            "and is intended for use by people who like to explore the lowest levels of their device. It "
            "runs on a rooted Android phone and is only for browsing the file system. See the IBM "
            "developerWorks article for a full description of this application.<br>"
            "</body></html>\n";
int _about(http_req_t *req, const char *name)
{
    printf("%s\n", __FUNCTION__);
    if (0 == strcmp(req->uri, "/about")) {
        req->length = strlen(about);
        res_send(req, 200, "text/html");
        return send(req->socketfd, about, req->length, 0);
    }
    return -1;
}
struct {
    const char *ext, *mimetype;
}mimes[] = {
    {".???",    "application/octet-stream"},
    {".3gp",    "video/3gpp" },
    {".css",    "text/css"  },
    {".gif",    "image/gif"  },
    {".htm",    "text/html"  },{".html",   "text/html"  },
    {".ico",    "image/x-icon" },
    {".jpg",    "image/jpeg" },{".jpeg",   "image/jpeg" },
    {".js",     "application/javascript" },
    {".json",   "text/plain" },
    {".m3u",    "audio/x-mpegurl" },
    {".mp3",    "audio/mpeg" },
    {".mp4",    "video/mp4" },
    {".png",    "image/png"  },
    {".swf",    "application/x-shockwave-flash" },
    {".txt",    "text/plain" },
    {".xml",    "text/xml"   },
    {".zsync",  "text/plain" },
    {0, 0}
};

const char *mimeokay(const char* fext)
{
    int i, buflen = strlen(fext);
    const char *fstr = mimes[0].mimetype;
    for(i = 1; mimes[i].ext != 0; i++) {
        int len = strlen(mimes[i].ext);
        if(!strncmp(&fext[buflen-len], mimes[i].ext, len)) {
            fstr=mimes[i].mimetype;
            break;
        }
    }
    return fstr;
}

int res_send(http_req_t *req, int status, const char *fstr)
{
    char *p = req->resbuf;
    const char *errstr = "OK";
    switch(status) {
        case 100:errstr = "Continue";break;
        case 201:errstr = "File Created";break;
        case 206:errstr = "Partial Content";break;
        case 301:errstr = "Moved Permanently";break;
        case 302:errstr = "Temporarily Moved";break;
        case 304:errstr = "Not Modified";break;
        case 400:errstr = "Bad Request";break;
        case 403:errstr = "Forbidden";break;
        case 404:errstr = "Not Found";break;
        case 405:errstr = "Method Not Allowed";break;
        case 408:errstr = "Request Timeout";break;
        case 416:errstr = "Requested Range Not Satisfiable";break;
        case 417:errstr = "Expect is not support";break;
        default:break;
    }
#define _MAXREQ ( sizeof(req->resbuf) - (p - req->resbuf) )
    p += sprintf_s(p, _MAXREQ, "HTTP/1.%d %d %s\r\n", req->version, status, errstr);
    if ((status == 200 || status == 206) && req->version != 0) {
        if (req->method == *(int *)"GET " && req->filetime.wYear > 0) {
            const char *abday[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
            const char *abmon[12+1] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            p += sprintf_s(p, _MAXREQ,
                "Last-Modified: %s, %2d %s %4d %02d:%02d:%02d %s\r\n",
                (req->filetime.wDayOfWeek >= 0 && req->filetime.wDayOfWeek < sizeof(abday)/sizeof(char *))?abday[req->filetime.wDayOfWeek]:0,
                req->filetime.wDay, (req->filetime.wMonth >= 1 && req->filetime.wMonth < sizeof(abmon)/sizeof(char *))?abmon[req->filetime.wMonth]:0,
                req->filetime.wYear, req->filetime.wHour, req->filetime.wMinute, req->filetime.wSecond, "GMT");
        }
        p += sprintf_s(p, _MAXREQ, "Content-Length: %d\r\n", req->length);
        if (status == 206) {//Partial Content
            p += sprintf_s(p, _MAXREQ, "Content-Range: bytes %d-%d/%d\r\n",
                req->range[0].offset, req->range[0].offset+req->range[0].length-1, req->filesize);
        }
        //p += sprintf_s(p, _MAXREQ, "Connection: %s\r\n", (req->keepalive)?"Keep-Alive":"Close");
        p += sprintf_s(p, _MAXREQ, "Content-Type: %s\r\n", fstr);
    }
    else if (status == 301 || status == 302) {
        p += sprintf_s(p, _MAXREQ, "%s", req->mainbuf);
    }
    p += sprintf_s(p, _MAXREQ, "%s\r\n", "");
    printf("@@@RET: %s\n", req->resbuf);
    //dump(req->resbuf, p-req->resbuf);
    return sendto(req->socketfd, req->resbuf, p - req->resbuf, 0, (struct sockaddr *)&req->hole, sizeof(req->hole));
}

char * get_uri(http_req_t *req, char *msg)
{
    char *p = msg, *q = req->uri;_skip_blank;
    if (p[0] == '/' && 0 == strncmp(&p[1], uid, uidlen)) p += uidlen + 1;//skip uid
    while(*p && *p != ' ') *q++ = *p++;*q = 0;_skip_blank;
    return p;
}

char * set_ver(http_req_t *req, char *msg)
{
    char *p = msg;
    if (p[0] == 'H' && p[1] == 'T' && p[2] == 'T' && p[3] == 'P' && p[4] == '/' && p[5] == '1' && p[6] == '.' && p[7]) {
        p += 7;req->version = *p++ - '0';
        printf("version %d\n", req->version);
    }
    else req->version = 0;
    return p;
}

char * decode_uri(const char *s, char *t)
{
    t += sprintf_s(t, sizeof(root), "%s", root);
    if (s[0] != '/') *t++ = SLASH;//concat

    while(*s) {
        if (s[0] == '%' && s[1] && s[2]) {
            int c = (_ctoi(s[1]) << 4) + _ctoi(s[2]);
            char i = *t++ = (c == '/')?SLASH:(char)c;
            s += 3;
            if (i == '%' && s[0] && s[1]) {
                t--;
                c = (_ctoi(s[0]) << 4) + _ctoi(s[1]);
                i = *t++ = (c == '/')?SLASH:(char)c;
                s += 2;
            }
        }
        else if (s[0] == '+') {
            *t++ = ' ';s++;
        }
        else if (s[0] == '/') {
            *t++ = SLASH;s++;//dos style
        }
        else if (s[0] == '?' || s[0] == '&' /*|| (s[0] == '.' && s[1] == '.')*/) {
            *t = 0;s++;break;
        }
        else *t++ = *s++;
    }
    *t = 0;
    return (char *)s;
}

int get_mtime(const char *str, SYSTEMTIME *mt)
{
    const char *abday[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    const char *abmon[12+1] = {"Xxx", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int i;char *p = (char *)str, *q;

    _skip_blank;q = p;while (*p && *p != ',') p++;
    if (*p == ',') p++;//dayofweek
    for(i = 0; i < sizeof(abday)/sizeof(char *); i++) {
        if (strncmp(q, abday[i], strlen(abday[i])) == 0) {mt->wDayOfWeek = i;break;}
    }
    _skip_blank;q = p;while(*p && *p != ' ') p++;//day
    mt->wDay = __atoi(q, p);

    _skip_blank;q = p;while(*p && *p != ' ') p++;//mon
    for(i = 0; i < sizeof(abmon)/sizeof(char *); i++) {
        if (strncmp(q, abmon[i], strlen(abmon[i])) == 0) {mt->wMonth = i;break;}
    }
    _skip_blank;q = p;while(*p && *p != ' ') p++;//year
    mt->wYear = __atoi(q, p);
    _skip_blank;q = p;while(*p && *p != ':') p++;//hour
    mt->wHour = __atoi(q, p);
    if (*p == ':') p++;q = p;while(*p && *p != ':') p++;//minute
    mt->wMinute = __atoi(q, p);
    if (*p == ':') p++;q = p;while(*p && *p != ' ') p++;;//second
    mt->wSecond = __atoi(q, p);
    _skip_blank;q = p;//GMT
    return 0;
}

static cbc_t get_cbchain;

void _append(cbc_t *head, void *cb)
{
    cbc_t *p = head;
    while(p && p->next) p = p->next;
    p->next = (void *)malloc(sizeof(cbc_t));
    ((cbc_t *)(p->next))->cb = cb;((cbc_t *)(p->next))->next = 0;
}

int todo_get(http_req_t *req)
{
    char fullname[MAX_PATH], *p = req->uri;
    printf("%s {%s}\n", __FUNCTION__, req->uri);
    if (0 == req->uri[0] || 0 == strcmp(req->uri, "/")) {//default homepage
        strcat_s(req->uri, sizeof(req->uri), "index.html");
    }
    else if (p[0] == '/' && p[1] == 'd' && p[2] == '?') {//get list directory contents
        p += 3;if (0 == strncmp(p, "dir=", 4)) {//search dir, optimize later
            p += 4;strcat_s(p, sizeof(req->uri)- (p-req->uri), "/");//force slash eos
        }
    }
    decode_uri(p, &fullname[0]);
    {cbc_t *cbc = &get_cbchain;
    while(cbc) {
        if (cbc->cb && 0 <= ((int (*)(void *, void *))(cbc->cb))(req, fullname)) return 0;//ok
        cbc = cbc->next;
    }}
    res_send(req, 404, "text/plain");
    return -1;
}

int parse_req(http_req_t *req, char *msg )
{
    printf("GOT {%s}\n", msg);
    if (0 == strncmp(msg, "GET ", 4)) {
        req->method = *(int *)msg;
        set_ver(req, get_uri(req, &msg[4]));
        req->cb = (cb_t)todo_get;
        req->range[0].offset = req->range[0].length = 0;
        req->filetime.wYear = 0;//set default
    }
    else if (0 == strncmp(msg, "Range:", sizeof("Range:")-1)) {
        char *p = &msg[sizeof("Range:")-1];_skip_blank;
        if (0 == strncmp(p, "bytes=", sizeof("bytes=")-1)) {
            p += sizeof("bytes=")-1;
            {char *q=p;while (*q && *q != '-') q++;
            if (*q == '-') {
                *q = '\0';req->range[0].offset = atoi(p);
                if (q[1]) req->range[0].length = (*p)?(atoi(&q[1]) - req->range[0].offset + 1):-atoi(&q[1]);
                else req->range[0].length = 0x7fffffff;//largest filesize
            }}
        }
    }
    else if (0 == strncmp(msg, "Connection:", sizeof("Connection:")-1)) {
        char *p = &msg[sizeof("Connection:")-1];_skip_blank;
        req->keepalive = (0 == strncmp(p, "keep-alive", sizeof("keep-alive")-1));
        //printf("keepalive %d\n", req->keepalive);
    }
    else if (0 == strncmp(msg, "If-Modified-Since:", sizeof("If-Modified-Since:")-1)) {
        char *p = &msg[sizeof("If-Modified-Since:")-1];_skip_blank;
        get_mtime(p, &req->filetime);
    }
    else if (0 == strncmp(msg, "Host:", sizeof("Host:")-1)) {
        char *p = &msg[sizeof("Host:")-1];_skip_blank;
        strcpy_s(req->host, sizeof(req->host), p);
    }
    else if (0 == strncmp(msg, "Referer:", sizeof("Referer:")-1)) {
        char *p = &msg[sizeof("Referer:")-1];_skip_blank;
        strcpy_s(req->referer, sizeof(req->referer), p);
    }
    return 0;
}

int __stdcall child(http_req_t *req)
{
    req->keepalive = 0;req->cb = (int (*)(void *))0;
    while(1) {
        int ret;unsigned int used = GetTickCount();
        printf("<%d> %s @(%d) ...\n", _getpid(), __FUNCTION__, req->socketfd);
        if (req->bufleft) {
            ret = req->bufleft;req->bufleft = 0;
        }
        else {
            socklen_t length=sizeof(req->hole);
            ret = recv(req->socketfd, req->mainbuf, sizeof(req->mainbuf), 0);
            if (ret <= 0) break;//something wrong
            dump(req->mainbuf, (ret > 4096)?16:ret);
        }
        {char *p = req->mainbuf, *msg = p;
        while(p < &req->mainbuf[ret]) {
            if (p[0] == '\r' && p[1] == '\n') {
                *p = 0;p+=2;
                if (!*msg) break;
                parse_req(req, msg);
                msg = p;
            }
            else p++;
        }
        req->bufleft = &req->mainbuf[ret] - p;
        if (req->bufleft) {
            printf("@@@LEFT: %d\n", req->bufleft);
            memcpy(&req->mainbuf[0], p, req->bufleft);
        }}
        printf("@@@ACT: %C%C%C {%s}\n", ((char *)&req->method)[0], ((char *)&req->method)[1], ((char *)&req->method)[2], req->uri);
        if (req->cb) req->cb(req);
        else res_send(req, 405, "text/plain");
        printf("USED %ums\n", GetTickCount() - used);
        if (!req->keepalive) break;
    }
    printf("<%d> %s @(%d) ...END.\n", _getpid(), __FUNCTION__, req->socketfd);
    {int optVal, optLen = sizeof(optVal);
    if (getsockopt(req->socketfd, SOL_SOCKET, SO_TYPE, (char*)&optVal, &optLen) == 0 && optVal == SOCK_STREAM) {
        closesocket(req->socketfd);
    }}
    free(req);return 0;
}

int main(int argc, char* argv[])
{
    int listenfd, port = 1984;
    struct sockaddr_in cli_addr, srv_addr;
#ifdef _WIN32
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2,2), &wsadata) != 0) {
        perror("WSAStartup");return -1;
    }
#endif
    //global variable
    if (argc > 1) {
        strcpy_s(root, MAX_PATH, argv[1]);
        if (chdir(root)) return -1;
    }
    getcwd(root, MAX_PATH);
    printf("The fake root directory is: %s\n", root);
    uidlen = setUID();
    printf("{%s %s} <%d> %s @port=%d,\nuid=%s (%d)\n", __DATE__, __TIME__, getpid(), argv[0], port, uid, uidlen);

    _append(&get_cbchain, get_list);
    _append(&get_cbchain, down_file);
    _append(&get_cbchain, get_uid);
    _append(&get_cbchain, _about);

#if defined(_VRELAY)
    _fork(0, reverse_proxy);_fork(0, peer_proxy);
#endif

    if ( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");return -1;
    }

#ifndef _WIN32
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    setpgrp();

    int optval = 1;
    int optlen = sizeof(optval);
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, optlen);
#endif

    srv_addr.sin_family         = AF_INET;
    srv_addr.sin_addr.s_addr    = htonl(INADDR_ANY);inet_addr("127.0.0.1");
    srv_addr.sin_port           = htons(port);
    if ( bind(listenfd, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) || listen(listenfd, 64)) {
        perror("bind");return -1;
    }

    // Loopwait for sock connection
    while(++hits) {
        int socketfd, length = sizeof(cli_addr);
        if ( (socketfd = accept(listenfd, (struct sockaddr*)&cli_addr, (socklen_t*)&length)) < 0) {
            perror("accept");//continue wait
        }
        else {
            http_req_t *req = (http_req_t *)malloc(sizeof(http_req_t));
            req->socketfd = socketfd;req->bufleft = 0;
            _fork(req, child);
        }
    }
    closesocket(listenfd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
