#include "http.h"

#ifndef _DEBUG
#pragma comment (linker, "/ENTRY:mainCRTStartup /subsystem:windows")
#endif
#pragma comment(lib, "ws2_32.lib")

int _fork(http_req_t *req, void *_child)
{
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)_child, req, 0, NULL);
    return 0;
}

int get_list(http_req_t *req, const char *dir)
{
    char fullname[MAX_PATH];
    sprintf_s(fullname, sizeof(fullname), "%s\\*", dir);
    WIN32_FIND_DATAA ffd;
    LARGE_INTEGER filesize;
    HANDLE hFind = FindFirstFileA(fullname, &ffd);
    if (INVALID_HANDLE_VALUE == hFind) {//maybe file
        //printf("ERR: %d\n", GetLastError());
        return -1;
    }
    printf("%s {%s}\n", __FUNCTION__, dir);
    strcpy_s(req->mainbuf, sizeof(req->mainbuf), "[");
    char *p = req->mainbuf;
    *p++ = '[';
    // List all the files in the directory with some info about them.
    do {
        if(0 == strcmp(".", ffd.cFileName) || 0 == strcmp("..", ffd.cFileName)) continue;
        int isdir = 1;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            //printf("  %s   <DIR>\n", ffd.cFileName);
        }
        else {
            filesize.LowPart = ffd.nFileSizeLow;
            filesize.HighPart = ffd.nFileSizeHigh;
            //printf("  %s   %ld bytes\n", ffd.cFileName, filesize.QuadPart);
            isdir = 0;
        }
#define _MAXMAIN ( sizeof(req->mainbuf) - (p - req->mainbuf) )
        p += sprintf_s(p, _MAXMAIN, "{\"name\":\"%s%s\"},", ffd.cFileName, isdir?"/":"");
    }
    while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);

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
    HANDLE hFile = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        //res_send(req, 404, "text/plain");
        return -1;
    }
    //full load cached resources or partial
    int ret = 0;
    DWORD dwBytesRead = 0;
    req->filesize = GetFileSize(hFile, 0);
    printf("filesize %d, (%d, %d)\n", req->filesize, req->range[0].offset, req->range[0].length);
    // Retrieve the file times for the file.
    FILETIME ftWrite;
    if (GetFileTime(hFile, NULL, NULL, &ftWrite)) {
        SYSTEMTIME newtime;
        FileTimeToSystemTime(&ftWrite, &newtime);
        if (memcmp(&newtime, &req->filetime, sizeof(SYSTEMTIME)-sizeof(WORD)) == 0) {
            res_send(req, 304, "text/plain");
            CloseHandle(hFile);
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
        SetFilePointer(hFile, req->range[0].offset, 0, FILE_BEGIN);
        while(req->length > 0 && ReadFile(hFile, req->mainbuf, sizeof(req->mainbuf), &dwBytesRead, NULL) && dwBytesRead > 0) {
            ret=sendto(req->socketfd, req->mainbuf, ___min(dwBytesRead, (DWORD)req->length), 0, (struct sockaddr *)&req->hole, sizeof(req->hole));
            //dump(req->mainbuf, ret);
            req->length -= ret;
        }
    }
    else {
        req->length = req->filesize;
        res_send(req, 200, mimeokay(name));
        while(ReadFile(hFile, req->mainbuf, sizeof(req->mainbuf), &dwBytesRead, NULL) && dwBytesRead > 0) {
            //dump(req->mainbuf, dwBytesRead);
            int len = send(req->socketfd, req->mainbuf, dwBytesRead, 0);
            if (len != dwBytesRead) {
                printf("send %d\n", len);break;
            }
        }
    }
    CloseHandle(hFile);
    return 0;
}

#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")

int setUID()
{
    // 以下代码可以取得系统特征码（网卡MAC、硬盘序列号、CPU ID、BIOS编号）
    BYTE szSystemInfo[4096]; // 在程序执行完毕后，此处存储取得的系统特征码
    UINT uSystemInfoLen = 0; // 在程序执行完毕后，此处存储取得的系统特征码的长度

    //HOST NAME
    {
        gethostname((char *)szSystemInfo, sizeof(szSystemInfo));
        BYTE *p = szSystemInfo;while(*p && *p != '-') p++;*p = 0;
        uSystemInfoLen = p - szSystemInfo;
    }
    
    // 网卡 MAC 地址，注意: MAC 地址是可以在注册表中修改的
    {
        UINT uErrorCode = 0;
        IP_ADAPTER_INFO iai;
        ULONG uSize = 0;
        DWORD dwResult = GetAdaptersInfo( &iai, &uSize );
        if( dwResult == ERROR_BUFFER_OVERFLOW ) {
            IP_ADAPTER_INFO* piai = ( IP_ADAPTER_INFO* )HeapAlloc( GetProcessHeap( ), 0, uSize );
            if( piai != NULL ) {
                dwResult = GetAdaptersInfo( piai, &uSize );
                if( ERROR_SUCCESS == dwResult ) {
                    IP_ADAPTER_INFO* piai2 = piai;
                    while( piai2 != NULL && ( uSystemInfoLen + piai2->AddressLength ) < 4096U ) {
                        CopyMemory( szSystemInfo + uSystemInfoLen, piai2->Address, piai2->AddressLength );
                        uSystemInfoLen += piai2->AddressLength;
                        piai2 = piai2->Next;
                    }
                }
                HeapFree( GetProcessHeap( ), 0, piai );
            }
        }
    }

    // CPU ID
    {
        BOOL bException = FALSE;
        BYTE szCpu[16]  = { 0 };
        UINT uCpuID     = 0U;

        __try {
            __asm {
                mov eax, 0
                cpuid
                mov dword ptr szCpu[0], ebx
                mov dword ptr szCpu[4], edx
                mov dword ptr szCpu[8], ecx
                mov eax, 1
                cpuid
                mov uCpuID, edx
            }
        }
        __except( EXCEPTION_EXECUTE_HANDLER ) {
            bException = TRUE;
        }

        if( !bException ) {
            CopyMemory( szSystemInfo + uSystemInfoLen, &uCpuID, sizeof( UINT ) );
            uSystemInfoLen += sizeof( UINT );

            uCpuID = strlen( ( char* )szCpu );
            CopyMemory( szSystemInfo + uSystemInfoLen, szCpu, uCpuID );
            uSystemInfoLen += uCpuID;
        }
    }

    // 完毕， 系统特征码已取得。
    return sha1sum((char *)szSystemInfo, uSystemInfoLen, uid, sizeof(uid));
}

__inline void create_child(LPSTR szCmdline, BOOL bInheritHandles, HANDLE hChildStd_IN, HANDLE hChildStd_OUT)
{
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
    ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = hChildStd_OUT;
    siStartInfo.hStdOutput = hChildStd_OUT;
    siStartInfo.hStdInput = hChildStd_IN;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    if (!CreateProcessA(NULL, szCmdline, NULL, NULL, bInheritHandles, 0, NULL, NULL, &siStartInfo, &piProcInfo)) return;
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    return ;
}

int run_batch(http_req_t *req, const char *cmd)
{
    HANDLE hreadpipe = NULL, hwritepipe = NULL;
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;// Set the bInheritHandle flag so pipe handles are inherited.
    saAttr.lpSecurityDescriptor = NULL;
    // Create an anonymous pipe for the child process's STDOUT.
    if ( ! CreatePipe(&hreadpipe, &hwritepipe, &saAttr, 0) ) return -1;
    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if ( ! SetHandleInformation(hreadpipe, HANDLE_FLAG_INHERIT, 0) ) return -1;
    // Create the child process.
    create_child((LPSTR)cmd, TRUE, INVALID_HANDLE_VALUE, hwritepipe);
    // Close the write end of the pipe before reading from the read end of the pipe, to control child process execution.
    if ( ! CloseHandle(hwritepipe) ) return -1;
    for (;;) {
        DWORD dwRead;
        BOOL bSuccess = ReadFile( hreadpipe, req->mainbuf, sizeof(req->mainbuf), &dwRead, NULL);
        if( ! bSuccess || dwRead == 0 ) break;
        dump(req->mainbuf, (dwRead > 4096)?16:dwRead);
        int ret = send(req->socketfd, req->mainbuf, dwRead, 0);
        if (! ret ) break;
    }
    return 0;
}

int run_boxhost(http_req_t *req, const char *cmd)
{
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    char pipename[MAX_PATH] = "\\\\.\\pipe\\mynamedpipe";//public
    // Try to open a named pipe; wait for it, if necessary.
    while (1) {
        int wait = 0;//milliseconds
        while ( ! WaitNamedPipeA(pipename, 0) && wait < 1000) {
            printf("Could not open pipe {%s}: 0 second wait timed out. GLE=%d\n", pipename, GetLastError() );
            // Create the child process at first loopwait.
            if (!wait) create_child((LPSTR)cmd, FALSE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE);
            Sleep(100);wait += 100;
        }
        if ( (hPipe = CreateFileA(pipename, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE) break;
        // Exit if an error other than ERROR_PIPE_BUSY occurs.
        if (GetLastError() != ERROR_PIPE_BUSY) {
            printf("Could not open pipe {%s}. GLE=%d\n", pipename, GetLastError() );return -1;
        }
        // All pipe instances are busy, so wait for 20 seconds.
        if ( ! WaitNamedPipeA(pipename, 20000) ) {
            printf("Could not open pipe {%s}: 20 second wait timed out. GLE=%d\n", pipename, GetLastError() );return -1;
        }
    }
    BOOL fSuccess = FALSE;
    DWORD  cbRead;
    // The pipe connected; change to message-read mode.
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    if ( ! SetNamedPipeHandleState(hPipe, &dwMode, NULL, NULL) ) {
        printf("SetNamedPipeHandleState failed. GLE=%d\n", GetLastError() );return -1;
    }
    // Send request to the pipe server.
    if ( ! WriteFile(hPipe, req->uri, strlen(req->uri), &cbRead, NULL) ) {
        printf("WriteFile to pipe failed. GLE=%d\n", GetLastError() );return -1;
    }
    // Read from the pipe, and sendto through sock handle.
    do {
        fSuccess = ReadFile(hPipe, req->mainbuf, sizeof(req->mainbuf), &cbRead, NULL);
        if ( ! fSuccess && GetLastError() != ERROR_MORE_DATA ) break;
        if (cbRead > 0) {
            dump(req->mainbuf, (cbRead > 4096)?16:cbRead);
            int ret = send(req->socketfd, req->mainbuf, cbRead, 0);
            if (! ret ) break;
        }
    } while ( ! fSuccess || 1);  // repeat loop if ERROR_MORE_DATA or not
    if ( ! fSuccess) {
        //printf("ReadFile from pipe failed. GLE=%d\n", GetLastError() );return -1;
    }
    CloseHandle(hPipe);
    return 0;
}
