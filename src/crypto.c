#include "crypto.h"
#include <string.h>

int vault_encrypt(const unsigned char *plaintext, size_t plaintext_len,
                  unsigned char *out_backing_data, const unsigned char *key) {
    unsigned char nonce[NONCE_SIZE];
    randombytes_buf(nonce, NONCE_SIZE);

    // Write nonce to the beginning of the buffer
    memcpy(out_backing_data, nonce, NONCE_SIZE);

    // Encrypt the plaintext and write it directly after the nonce
    if (crypto_secretbox_easy(out_backing_data + NONCE_SIZE, plaintext, plaintext_len, nonce, key) != 0) {
        return -1;
    }
    return 0;
}

int vault_decrypt(const unsigned char *backing_data, size_t backing_len,
                  unsigned char *out_plaintext, const unsigned char *key) {
    if (backing_len < CRYPTO_OVERHEAD) {
        return -1;
    }

    const unsigned char *nonce = backing_data;
    const unsigned char *ciphertext = backing_data + NONCE_SIZE;
    size_t ciphertext_len = backing_len - NONCE_SIZE;

    // Decrypt the ciphertext
    if (crypto_secretbox_open_easy(out_plaintext, ciphertext, ciphertext_len, nonce, key) != 0) {
        return -1;
    }
    return 0;
}
