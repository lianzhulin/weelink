#include "http.h"

int _fork(http_req_t *req, void *_child)
{
    if (0 == fork()) {//child
        ((int (__stdcall *)(void *))_child)(req);exit(0);//noreturn
    }
    else if (req) closesocket(req->socketfd);
    return 0;
}

int get_list(http_req_t *req, const char *dir)
{
    printf("%s {%s}\n", __FUNCTION__, dir);
    strcpy_s(req->mainbuf, sizeof(req->mainbuf), "[");
    char *p = req->mainbuf;
    *p++ = '[';

    char fullname[MAX_PATH];
    decode_uri(dir, &fullname[0]);

    if (0 != chdir(fullname)) return -1;

    struct dirent **namelist = 0;
    int i, n;
    n = scandir(".", &namelist, 0, alphasort);
    for(i = 0; i < n; i++) {
        if(0 == strcmp(".", namelist[i]->d_name) || 0 == strcmp("..", namelist[i]->d_name))
            continue;

        int isdir = 1;
        if (namelist[i]->d_type == DT_DIR || namelist[i]->d_type == DT_UNKNOWN) {
            if (namelist[i]->d_type == DT_UNKNOWN) {
                DIR *d = opendir(namelist[i]->d_name);
                if (!d) isdir = 0;
                else closedir(d);
            }
        }
        else isdir = 0;
        sprintf(p, "{\"name\":\"%s%s\"},", namelist[i]->d_name, isdir?"/":"");
        while(*p) p++;
        free(namelist[i]);
    }
    if (namelist) free(namelist);

    if (p[-1] == ',') p--;
    *p++ = ']';*p++ = 0;
    fprintf(stderr, "%s\n", req->mainbuf);//dump
    req->length = p - req->mainbuf - 1;
    res_send(req, 200, "text/plain");
    return send(req->socketfd, req->mainbuf, req->length, 0);
}

int down_file(http_req_t *req, const char *name)
{//cached
    printf("%s {%s}\n", __FUNCTION__, name);
    int ret = 0;
    FILE* fp;
    if((fp = fopen(name, "r")) == NULL) {
        //res_send(req, 404, "text/plain");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    req->filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("filesize %d, (%d, %d)\n", req->filesize, req->range[0].offset, req->range[0].length);
    // Retrieve the file times for the file.
    struct stat st;
    if (0 == stat(name, &st)) {
        SYSTEMTIME newtime = *gmtime(&st.st_mtime);
        newtime.wYear += 1900; newtime.wMonth += 1;
        if (memcmp(&newtime, &req->filetime, sizeof(SYSTEMTIME)-8) == 0) {
            res_send(req, 304, "text/plain");
            fclose(fp);
            return 0;
        }
        else memcpy(&req->filetime, &newtime, sizeof(SYSTEMTIME));
        printf("(%d) %d-%d-%d %d:%d:%d\r\n", req->filetime.wDayOfWeek,
            req->filetime.wYear, req->filetime.wMonth, req->filetime.wDay,
            req->filetime.wHour, req->filetime.wMinute, req->filetime.wSecond);
    }
#define ___min(a,b)  ( (a < b)?a:b )
    if (req->range[0].offset || req->range[0].length) {//partial
        if (req->range[0].length < 0) {
            req->length = req->filesize;
            req->length = req->range[0].length = ___min(req->length, -req->range[0].length);
            req->range[0].offset = req->filesize - req->range[0].length;
        }
        else if (req->range[0].length > 0) {
            req->length = req->filesize - req->range[0].offset;//left
            req->length = req->range[0].length = ___min(req->length, req->range[0].length);
        }
        else {
            req->length = req->range[0].length = req->filesize - req->range[0].offset;//left
        }
        if (req->length <= 0) {
            res_send(req, 416, mimeokay(name));
            return -1;
        }

        res_send(req, 206, mimeokay(name));
        fseek(fp, req->range[0].offset, SEEK_SET);
        while(req->length > 0 && (ret = fread(req->mainbuf, 1, sizeof(req->mainbuf), fp)) > 0) {
            ret=sendto(req->socketfd, req->mainbuf, ___min(ret, req->length), 0, (struct sockaddr *)&req->hole, sizeof(req->hole));
            //dump(req->mainbuf, ret);
            req->length -= ret;
        }
    }
    else {
        req->length = req->filesize;
        res_send(req, 200, mimeokay(name));
        while((ret = fread(req->mainbuf, 1, sizeof(req->mainbuf), fp)) > 0) {
            struct timeval timeout = {100, 0};
            setsockopt(req->socketfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(struct timeval));
            int len = send(req->socketfd, req->mainbuf, ret, 0);
            if (len != ret) {
                printf("send %d\n", len);break;
            }
        }
    }

    fclose(fp);
    return 0;
}







#include <assert.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>

#define MAXINTERFACES 16
int setUID()
{
    char data[4096];int len = 0;
    //HOST NAME
    {
        char *p = data;
        gethostname(data, sizeof(data));
        while(*p && *p != '-') p++;*p = 0;
        len = p - data;
    }

    register int fd, interface;
    struct ifreq buf[MAXINTERFACES];
    struct arpreq arp;
    struct ifconf ifc;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
        ifc.ifc_len = sizeof buf;
        ifc.ifc_buf = (caddr_t) buf;
        if (!ioctl(fd, SIOCGIFCONF, (char *) &ifc)) {
            interface = ifc.ifc_len / sizeof(struct ifreq);
            fprintf(stderr, "interface num is %d\n\n", interface);
            while (interface-- > 0) {
                /*Get HW ADDRESS of the net card */
                if (!(ioctl(fd, SIOCGIFHWADDR, (char *) &buf[interface]))) {
                    memcpy(&data[len], &buf[interface].ifr_hwaddr.sa_data[0], 6);
                    len += 6;
                }
            }
        }
    }
    close(fd);
    assert(len != 0);
    dump(data, len);
    return sha1sum(data, len, uid, sizeof(uid));
}

/* Confined execute bash file received from remote, all stdout wille send back
   http protocal header maybe like below:
echo HTTP/1.1 200 OK
echo Content-Type: text/plain
echo
echo content data
 */
int run_batch(http_req_t *req, const char *cmd)
{
    char tmp[256];
    char *p = (char *)cmd;
    sprintf(tmp, "%s>&%d", cmd, req->socketfd);
    printf("@@@RUN: %s\n", tmp);
    //chmod(name, 0777);
    req->keepalive = 0;

    fflush(stdout);
    int ret = system(tmp);
    return 0;
}
