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
    /* 1. Generate a fresh random nonce and write it to the front of the buffer */
    randombytes_buf(out_backing_data, NONCE_SIZE);

    /* 2. Encrypt plaintext into the bytes right after the nonce.
     *    crypto_secretbox_easy writes ciphertext + MAC (plaintext_len + MAC_SIZE bytes)
     *    using the nonce we just generated and the caller's key. */
    if (crypto_secretbox_easy(out_backing_data + NONCE_SIZE,
                              plaintext, plaintext_len,
                              out_backing_data, key) != 0) {
        return -1;
    }

    return 0;
}

int encfs_decrypt(const unsigned char *backing_data, size_t backing_len,
                   unsigned char *out_plaintext, const unsigned char *key) {
    if (backing_len < CRYPTO_OVERHEAD) {
        return -1;
    }

    const unsigned char *nonce = backing_data;
    const unsigned char *ciphertext = backing_data + NONCE_SIZE;
    size_t ciphertext_len = backing_len - NONCE_SIZE;

    if (crypto_secretbox_open_easy(out_plaintext, ciphertext, ciphertext_len,
                                    nonce, key) != 0) {
        return -1;
    }

    return 0;
}
