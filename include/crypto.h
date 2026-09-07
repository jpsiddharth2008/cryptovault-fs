#ifndef ENCFS_CRYPTO_H
#define ENCFS_CRYPTO_H

#include <stddef.h>
#include <sodium.h>

/* Using libsodium's crypto_secretbox (XSalsa20-Poly1305): authenticated
 * encryption, so tampering with ciphertext is detected on decrypt rather
 * than silently producing corrupted plaintext (unlike plain AES-CBC). */

#define KEY_SIZE   crypto_secretbox_KEYBYTES
#define NONCE_SIZE crypto_secretbox_NONCEBYTES
#define MAC_SIZE   crypto_secretbox_MACBYTES
#define CRYPTO_OVERHEAD (NONCE_SIZE + MAC_SIZE)

/*
 * Encrypts plaintext into out_backing_data.
 * out_backing_data must be at least plaintext_len + CRYPTO_OVERHEAD bytes.
 * Layout produced: [nonce][ciphertext+MAC]
 * Returns 0 on success, -1 on failure.
 */
int encfs_encrypt(const unsigned char *plaintext, size_t plaintext_len,
                   unsigned char *out_backing_data, const unsigned char *key);

/*
 * Decrypts backing_data (must contain [nonce][ciphertext+MAC]).
 * out_plaintext must be at least backing_len - CRYPTO_OVERHEAD bytes.
 * Returns 0 on success, -1 on failure (including tamper/corruption detected).
 */
int encfs_decrypt(const unsigned char *backing_data, size_t backing_len,
                   unsigned char *out_plaintext, const unsigned char *key);

#endif /* ENCFS_CRYPTO_H */
