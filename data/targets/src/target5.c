#include <string.h>
#include <stdio.h>

int
main(int argc, char** argv)
{
    if (argc < 2) {
        printf("Got no input argument!\n");
    }
    else {
        char input[256] = {0};
        memcpy(input, argv[1], sizeof(input));
        printf("Got %s from the argv!\n", input);
    }
    return 0;
}
