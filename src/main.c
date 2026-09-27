#include "encfs.h"
#include "crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>

#include <unistd.h>
#include <limits.h>

/* TODO (issue #15): wire up program startup.
 *
 *   1. sodium_init() before anything else touches libsodium.
 *   2. Usage check: argc < 3 -> print "Usage: %s <backing_dir> <mountpoint>
 *      [FUSE options]" and exit.
 *   3. Resolve argv[1] to an absolute path with realpath() into
 *      ctx.backing_path -- it won't necessarily still be valid as a
 *      relative path once FUSE callbacks are running.
 *   4. Get the passphrase from the VAULT_KEY environment variable
 *      (getenv), falling back to an interactive getpass() prompt if unset.
 *   5. Derive exactly KEY_SIZE (32) bytes for ctx.key from that passphrase
 *      using crypto_generichash (a hash function, not an encryption
 *      function) -- the passphrase can be any length, the key must be
 *      exactly 32 bytes. Zero the passphrase buffer afterward
 *      (sodium_memzero) since it's sensitive.
 *   6. backing_dir (argv[1]) is not a FUSE option -- shift argv left to
 *      remove it and decrement argc before calling fuse_main, so FUSE
 *      only sees the program name, mountpoint, and any real FUSE flags.
 *   7. return fuse_main(argc, argv, &encfs_oper, &ctx);
 *
 * See docs/design.md section 4 and GitHub issue #15 for the full
 * walkthrough, including why VAULT_KEY (an env var) is safer here than a
 * plain command-line argument would be.
 */
int main(int argc, char *argv[]) {
    /* libsodium must be initialized before any other libsodium call. */
    if (sodium_init() < 0) {
        fprintf(stderr, "encfs: failed to initialize libsodium\n");
        return 1;
    }

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <backing_dir> <mountpoint> [FUSE options]\n", argv[0]);
        return 1;
    }

    struct encfs_context ctx;

    /* Resolve backing_dir to an absolute path now -- a relative path could
     * silently mean something different once FUSE callbacks are running
     * from a different working directory. */
    if (realpath(argv[1], ctx.backing_path) == NULL) {
        perror("encfs: realpath");
        return 1;
    }

    /* Prefer VAULT_KEY (an env var, not a command-line argument, so it
     * doesn't leak to anyone running `ps aux`); fall back to a hidden
     * interactive prompt if it isn't set. */
    char *passphrase = getenv("VAULT_KEY");
    if (passphrase == NULL) {
        passphrase = getpass("Passphrase: ");
    }

    if (passphrase == NULL || strlen(passphrase) == 0) {
        fprintf(stderr, "encfs: no passphrase provided\n");
        return 1;
    }

    /* Turn the any-length passphrase into a fixed KEY_SIZE (32) byte key. */
    crypto_generichash(ctx.key, KEY_SIZE,
                        (const unsigned char *) passphrase, strlen(passphrase),
                        NULL, 0);

    /* Wipe the passphrase from memory now that the key is derived. */
    sodium_memzero(passphrase, strlen(passphrase));

    /* backing_dir (argv[1]) is specific to this project, not a real FUSE
     * option -- shift it out of argv so fuse_main only sees the program
     * name, the mountpoint, and any genuine FUSE flags. */
    for (int i = 1; i < argc - 1; i++) {
        argv[i] = argv[i + 1];
    }
    argc--;

    return fuse_main(argc, argv, &encfs_oper, &ctx);
}
