#include "path_utils.h"
#include "encfs.h"
#include <stdio.h>

void get_backing_path(char *dest, const char *path) {
    snprintf(dest, PATH_MAX, "%s%s", ENCFS_CTX->backing_path, path);
}
