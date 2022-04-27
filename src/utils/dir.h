#ifndef DIR_H
#define DIR_H

#include <stdbool.h>

// If the dir was created, or if it already exists, return true. Otherwise, if
// we failed to create the dir, return false.
bool
create_dir_ifn_exist(const char* output_dir);

#endif
