#include "encfs.h"
#include "crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>

#include <unistd.h>
#include <limits.h>

int main(int argc, char *argv[]) {
    if (sodium_init() == -1) {
        fprintf(stderr, "Error: libsodium initialization failed\n");
        return 1;
    }

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <backing_dir> <mountpoint> [FUSE options]\n", argv[0]);
        return 1;
    }

    struct encfs_context ctx;
    memset(&ctx, 0, sizeof(ctx));

    // Resolve backing directory to absolute path
    if (!realpath(argv[1], ctx.backing_path)) {
        perror("Error resolving backing directory path");
        return 1;
    }

    // Get passphrase from environment or user prompt
    char *passphrase = getenv("VAULT_KEY");
    char pass_buf[256];
    memset(pass_buf, 0, sizeof(pass_buf));

    if (!passphrase || strlen(passphrase) == 0) {
        char *prompt_pass = getpass("Enter vault passphrase: ");
        if (!prompt_pass || strlen(prompt_pass) == 0) {
            fprintf(stderr, "Error: VAULT_KEY environment variable not set, and no passphrase entered.\n");
            return 1;
        }
        strncpy(pass_buf, prompt_pass, sizeof(pass_buf) - 1);
        pass_buf[sizeof(pass_buf) - 1] = '\0';
        passphrase = pass_buf;
    }

    // Hash passphrase to generate the 32-byte key
    if (crypto_generichash(ctx.key, sizeof(ctx.key), (const unsigned char *)passphrase, strlen(passphrase), NULL, 0) != 0) {
        fprintf(stderr, "Error: key derivation failed\n");
        sodium_memzero(pass_buf, sizeof(pass_buf));
        return 1;
    }
    sodium_memzero(pass_buf, sizeof(pass_buf));

    // Shift argv to remove backing directory argument
    for (int i = 1; i < argc - 1; i++) {
        argv[i] = argv[i + 1];
    }
    argc--;

    return fuse_main(argc, argv, &encfs_oper, &ctx);
}
