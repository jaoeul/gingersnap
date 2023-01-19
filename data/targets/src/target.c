#include <string.h>
#include <stdio.h>

int
main(int argc, char** argv)
{
    char  aoeu            = '\0';
    char* invalid_address = 0x00000000;

    (void) aoeu;

    // Cause a segfault if more than 3 args are provided to the program.
    if (argc > 3) {
        aoeu = *invalid_address;
    }

    printf("No crash\n");
    return 0;
}
