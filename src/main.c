#include "vaultfs.h"
#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>

int main(int argc, char *argv[]) {
    // Initialize libsodium
    if (sodium_init() == -1) {
        fprintf(stderr, "Error: libsodium initialization failed\n");
        return 1;
    }

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <backing_dir> <mountpoint> [FUSE options]\n", argv[0]);
        return 1;
    }

    struct vault_context ctx;
    memset(&ctx, 0, sizeof(ctx));

    // Resolve backing directory path to absolute path
    if (realpath(argv[1], ctx.backing_path) == NULL) {
        perror("Error resolving backing directory path");
        return 1;
    }

    // Retrieve the secret key from the environment variable "VAULT_KEY"
    char *key_env = getenv("VAULT_KEY");
    if (!key_env) {
        fprintf(stderr, "Error: VAULT_KEY environment variable not set. Please export VAULT_KEY before running.\n");
        return 1;
    }

    // Derive a cryptographically secure 32-byte key from the user key string using SHA256
    crypto_generichash(ctx.key, sizeof(ctx.key), (unsigned char *)key_env, strlen(key_env), NULL, 0);

    // Prepare arguments for FUSE by removing the backing directory parameter
    char **fuse_argv = malloc(sizeof(char *) * argc);
    if (!fuse_argv) {
        return 1;
    }
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++) {
        fuse_argv[i - 1] = argv[i];
    }
    int fuse_argc = argc - 1;

    printf("Mounting VaultFS...\n");
    printf("Backing directory: %s\n", ctx.backing_path);
    printf("Mount point: %s\n", fuse_argv[fuse_argc - 1]);

    int ret = fuse_main(fuse_argc, fuse_argv, &vault_oper, &ctx);

    free(fuse_argv);
    return ret;
}
