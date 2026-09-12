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
    (void) argc;
    (void) argv;
    fprintf(stderr, "encfs: not yet implemented (see GitHub issue #15)\n");
    return 1;
}
