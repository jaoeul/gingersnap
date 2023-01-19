#include <string.h>
#include <stdio.h>

int
main(int argc, char** argv)
{
    int aoeu = 1;
    if (argc > 1) {
        // Cause a segfault.
        aoeu = *(int*)0;
    }
    printf("No crash\n");

    return 0;
}
