#include "path_utils.h"
#include "encfs.h"
#include <stdio.h>

/* Combines ENCFS_CTX->backing_path and the virtual path into dest.
 * snprintf is bounded to PATH_MAX, so a path longer than the buffer gets
 * truncated (and NUL-terminated) instead of overflowing dest. */
void get_backing_path(char *dest, const char *path) {
    snprintf(dest, PATH_MAX, "%s%s", ENCFS_CTX->backing_path, path);
}
