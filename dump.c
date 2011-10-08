#include <stdio.h>

void dump(const char *buf, int len)
{
#ifdef _DEBUG
    int i, j = 0;
    char fix[17];fix[16] = 0;
    printf("\n          00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f  ");
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
