// The MIPS cross compiler I built does not have a standard C library. That's
// why we have to implement some basic functions ourselves.

#include <stdio.h>

void
memcpy_kuk(char* dst, char* src, int len)
{
    for (int i = 0; i < len; i++) {
        dst[i] = src[i];
    }
}

int
strncmp(char* a, char* b, int len)
{
    for (int i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return -1;
        }
    }
    return 0;
}

int
strlen(char* str)
{
    int i = 0;

    while (str[i] != '\0')
    {
        i++;
    }
    return i;
}

int
main(int argc, char** argv)
{
    char input[256] = {0};

    if (argc < 2) {
        printf("Usage: %s <string>\n", argv[0]);
        return 0;
    }
    else {
        memcpy_kuk(input, argv[1], strlen(argv[1]));
    }

    if (strncmp(input, "aoeu", 4) == 0) {
        int* segfault = *(int*)0;
    }

    printf("No crash!\n");

    return 0;
}
