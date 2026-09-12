#include "crypto.h"
#include <string.h>

/* TODO (issue #3): implement encfs_encrypt.
 *
 * Layout to produce in out_backing_data: [nonce][ciphertext+MAC]
 *   1. Generate a random NONCE_SIZE-byte nonce (randombytes_buf).
 *   2. Write it into the first NONCE_SIZE bytes of out_backing_data.
 *   3. Call crypto_secretbox_easy(...) to encrypt plaintext into the bytes
 *      after the nonce, using that nonce and the given key.
 *   4. Return 0 on success, -1 if crypto_secretbox_easy fails.
 *
 * See docs/design.md section 3 and GitHub issue #3 for the full walkthrough.
 */
int encfs_encrypt(const unsigned char *plaintext, size_t plaintext_len,
                   unsigned char *out_backing_data, const unsigned char *key) {
    (void) plaintext;
    (void) plaintext_len;
    (void) out_backing_data;
    (void) key;
    return -1;
}

/* TODO (issue #4): implement encfs_decrypt.
 *
 *   1. If backing_len < CRYPTO_OVERHEAD, the blob can't be valid -- return -1
 *      immediately without touching it further.
 *   2. Split backing_data into the nonce (first NONCE_SIZE bytes) and the
 *      ciphertext+MAC (the rest).
 *   3. Call crypto_secretbox_open_easy(...) to decrypt into out_plaintext.
 *   4. Return 0 on success, -1 if the call fails (this is also how tampered/
 *      corrupted data gets rejected -- do not write partial output on failure).
 *
 * See docs/design.md section 3 and GitHub issue #4 for the full walkthrough.
 */
int encfs_decrypt(const unsigned char *backing_data, size_t backing_len,
                   unsigned char *out_plaintext, const unsigned char *key) {
    (void) backing_data;
    (void) backing_len;
    (void) out_plaintext;
    (void) key;
    return -1;
}
