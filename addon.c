#include "http.h"

int get_uid(http_req_t *req, const char *name)
{
    char *p = req->uri;
    if (p[0] == '/' && p[1] == 'u' && p[2] == '?') {
        char *p = req->mainbuf;
        p += sprintf_s(p, sizeof(req->mainbuf), "%s", uid);
        req->length = p - req->mainbuf;
        res_send(req, 200, "text/plain");
        return send(req->socketfd, req->mainbuf, req->length, 0);
    }
    return -1;
}
