#ifndef ENCFS_PATH_UTILS_H
#define ENCFS_PATH_UTILS_H

/* Translates a virtual mountpoint path (e.g. "/notes.txt") into the real
 * path on the backing store (e.g. "/home/you/backing/notes.txt").
 * dest must be a buffer of at least PATH_MAX bytes. */
void get_backing_path(char *dest, const char *path);

#endif /* ENCFS_PATH_UTILS_H */
