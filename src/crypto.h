#ifndef VAULTFS_CRYPTO_H
#define VAULTFS_CRYPTO_H

#include <stddef.h>
#include <sodium.h>

#define KEY_SIZE crypto_secretbox_KEYBYTES
#define NONCE_SIZE crypto_secretbox_NONCEBYTES
#define MAC_SIZE crypto_secretbox_MACBYTES
#define CRYPTO_OVERHEAD (NONCE_SIZE + MAC_SIZE)

/*
 * Encrypts plaintext into out_backing_data.
 * out_backing_data must be at least plaintext_len + CRYPTO_OVERHEAD bytes.
 * The layout of out_backing_data will be: [nonce][ciphertext]
 * Returns 0 on success, -1 on failure.
 */
int vault_encrypt(const unsigned char *plaintext, size_t plaintext_len,
                  unsigned char *out_backing_data, const unsigned char *key);

/*
 * Decrypts backing_data (which must contain [nonce][ciphertext]).
 * out_plaintext must be at least backing_len - CRYPTO_OVERHEAD bytes.
 * Returns 0 on success, -1 on failure.
 */
int vault_decrypt(const unsigned char *backing_data, size_t backing_len,
                  unsigned char *out_plaintext, const unsigned char *key);

#endif /* VAULTFS_CRYPTO_H */
