#include "path_utils.h"
#include "encfs.h"
#include <stdio.h>

/* TODO (issue #2): implement get_backing_path.
 *
 *   Combine ENCFS_CTX->backing_path and path into dest, using a BOUNDED
 *   string function (e.g. snprintf(dest, PATH_MAX, "%s%s", ...)) -- never
 *   raw strcat/strcpy here. dest is a fixed-size PATH_MAX buffer; an
 *   unbounded copy is a buffer overflow the first time a long path comes
 *   through this mount.
 *
 * See GitHub issue #2 for the full walkthrough.
 */
void get_backing_path(char *dest, const char *path) {
    (void) path;
    dest[0] = '\0';
}
