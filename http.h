
#ifdef __cplusplus
extern "C"{
#endif

#ifdef _WIN32
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <winsock2.h>
typedef int socklen_t;
#define getpid()    GetCurrentProcessId()
#define _getpid()   GetCurrentThreadId()
#define getcwd(s, l)    GetCurrentDirectoryA(l, s)
#define access(f, m)    _access(f, m)
#define chdir(s)        _chdir(s)
#define SLASH   '\\'

////////////////////////////////////////////////////////////
#else//_GNUC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct tm SYSTEMTIME;
#define wYear           tm_year
#define wMonth          tm_mon
#define wDay            tm_mday
#define wHour           tm_hour
#define wMinute         tm_min
#define wSecond         tm_sec
#define wDayOfWeek      tm_wday

#define __stdcall
#define __inline inline
#define MAX_PATH    (256)
#define SLASH   '/'

#define	sprintf_s(buffer, buffer_size, stringbuffer, ...) (sprintf(buffer, stringbuffer, __VA_ARGS__))
#define strcpy_s(src, len, dst) strcpy(src, dst)
#define strcat_s(src, len, dst) strcat(src, dst)
#define strncpy_s(src, len, dst, siz)   strncpy(src, dst, siz)
#define _getpid()    getpid()
#define closesocket(s) close(s)
#define Sleep(ms)   usleep(ms*1000)
static __inline unsigned int GetTickCount()
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) return 0;
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

static __inline int fopen_s(FILE** pFile, const char *filename, const char *mode)
{
    *pFile = fopen(filename, mode);
    return 0;
}

#endif

////////////////////////////////////////////////////////////
typedef int (*cb_t)(void *);

typedef struct {
    int method, version, expect, keepalive, length, filesize;
    struct {
        int offset, length;
    }range[1];
    int (*cb)(void *);
    struct {
        char name[32], data[256];
    }form[1];
    int socketfd, bufleft;//global variable
    struct sockaddr_in hole;//udp client
    SYSTEMTIME filetime;
    char uri[1024*2], filename[256], boundary[128], host[128], referer[1024*2];
    char mainbuf[1024*1024], padding1[8], resbuf[1024*4], padding2[8];
}http_req_t;

typedef struct {
    void *cb, *next;
}cbc_t;

typedef struct {
    unsigned int length;
    unsigned char buffer[1];
}ezbuf_t;

#define _skip_blank    {while(*p == ' ') p++;}

static __inline int _ctoi(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static __inline int __atoi(const char *s, const char *t)
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

//Global function
extern char uid[MAX_PATH], root[MAX_PATH];
extern int uidlen, hits;
void dump(const char *buf, int len);
int sha1sum(char *str, int ilen, char *out, int len);
char * get_uri(http_req_t *req, char *msg);
char * set_ver(http_req_t *req, char *msg);
char * decode_uri(const char *s, char *t);
const char *mimeokay(const char* fext);
int get_mtime(const char *str, SYSTEMTIME *);
int res_send(http_req_t *req, int status, const char *fstr);
int parse_req(http_req_t *req, char *msg );
int __stdcall child(http_req_t *req);
int todo_get(http_req_t *req);
int reverse_proxy();
int peer_proxy();

//addon
int _about(http_req_t *req, const char *name);
int get_uid(http_req_t *req, const char *name);
int todo_pin(http_req_t *req, const char *name);
int load_js(http_req_t *req, const char *name);
int get_validate(http_req_t *req, const char *name);
int down_boxfile(http_req_t *req, const char *name);
int des_copy(void *ks, ezbuf_t *in, ezbuf_t *out, int enc);
int rsa_encrypt(const char *str, int len, char *out);
int rsa_decrypt(const char *str, int len, char *out);

//Depended on target platform
int _fork(http_req_t *req, void *_child);
int get_list(http_req_t *req, const char *dir);
int down_file(http_req_t *req, const char *name);
int setUID();
int run_batch(http_req_t *req, const char *cmd);
int run_boxhost(http_req_t *req, const char *cmd);

#ifdef __cplusplus
}
#endif
