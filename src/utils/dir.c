#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

#include "dir.h"

bool
create_dir_ifn_exist(const char* output_dir)
{
    struct stat st = {0};
    if (stat(output_dir, &st) == -1) {
        if (!mkdir(output_dir, 0700) == 0) {
            perror("Faled to create dir!");
            return false;
        }
    }
    return true;
}
