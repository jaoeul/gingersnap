#include <string.h>
#include <stdio.h>

static volatile aoeu = 1;

int
main(int argc, char** argv)
{
    if (aoeu == 1337) {
        // Cause a segfault.
        aoeu = *(int*)0;
    }
    printf("No crash\n");

    return 0;
}
