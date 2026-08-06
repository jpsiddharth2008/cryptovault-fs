#ifndef VAULTFS_H
#define VAULTFS_H

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <limits.h>

// Private context to store key and paths
struct vault_context {
    char backing_path[PATH_MAX];
    unsigned char key[32];
};

// Retrieve context pointer
#define VAULT_CTX ((struct vault_context *)fuse_get_context()->private_data)

// Resolves a mount path to the backing file path
void get_backing_path(char *dest, const char *path);

// Exported FUSE operations struct
extern struct fuse_operations vault_oper;

#endif /* VAULTFS_H */
