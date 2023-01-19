#include <string.h>
#include <stdio.h>

int
main(int argc, char** argv)
{
    char input[256] = {0};

    if (argc < 2) {
        printf("Got no input argument!\n");
    }
    else {
        memcpy(input, argv[1], strlen(argv[1]));
        printf("Got %s from the argv!\n", input);
    }

    if (strncmp(input, "aoeu", 4) == 0) {
        int* segfault = *(int*)0;
    }

    return 0;
}
