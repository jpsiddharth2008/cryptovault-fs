/* Unit test for crypto.c — round-trips plaintext through encrypt/decrypt
 * and checks it matches, plus checks that tampering is detected.
 *
 * Build/run: make test-crypto && ./test_crypto
 * encfs_encrypt/encfs_decrypt are currently stubs that just return -1
 * (see issues #3 and #4), so every check here will print FAIL until
 * they're implemented — that's expected, not a bug in this test file. */

#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>

static int check(const char *label, int condition) {
    printf("[%s] %s\n", condition ? "PASS" : "FAIL", label);
    return condition;
}

int main(void) {
    if (sodium_init() == -1) {
        fprintf(stderr, "libsodium init failed\n");
        return 1;
    }

    int all_passed = 1;

    unsigned char key[KEY_SIZE];
    randombytes_buf(key, sizeof(key));

    const char *message = "the quick brown fox jumps over the lazy dog";
    size_t plaintext_len = strlen(message);
    size_t backing_len = plaintext_len + CRYPTO_OVERHEAD;

    unsigned char *backing = malloc(backing_len);
    unsigned char *recovered = malloc(plaintext_len);
    if (!backing || !recovered) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    /* Test 1: round trip */
    int enc_result = encfs_encrypt((const unsigned char *)message, plaintext_len, backing, key);
    all_passed &= check("encrypt returns 0", enc_result == 0);

    int dec_result = encfs_decrypt(backing, backing_len, recovered, key);
    all_passed &= check("decrypt returns 0", dec_result == 0);

    all_passed &= check("recovered plaintext matches original",
                         memcmp(message, recovered, plaintext_len) == 0);

    /* Test 2: two encryptions of the same plaintext use different nonces
     * (the ciphertext bytes should differ even though input is identical) */
    unsigned char *backing2 = malloc(backing_len);
    encfs_encrypt((const unsigned char *)message, plaintext_len, backing2, key);
    all_passed &= check("nonces differ across calls (ciphertext differs)",
                         memcmp(backing, backing2, backing_len) != 0);

    /* Test 3: tampering is detected */
    backing[NONCE_SIZE] ^= 0xFF; /* flip a bit in the ciphertext */
    int tamper_result = encfs_decrypt(backing, backing_len, recovered, key);
    all_passed &= check("tampered ciphertext fails to decrypt", tamper_result != 0);

    /* Test 4: wrong key fails */
    unsigned char wrong_key[KEY_SIZE];
    randombytes_buf(wrong_key, sizeof(wrong_key));
    int wrongkey_result = encfs_decrypt(backing2, backing_len, recovered, wrong_key);
    all_passed &= check("wrong key fails to decrypt", wrongkey_result != 0);

    free(backing);
    free(backing2);
    free(recovered);

    printf("\n%s\n", all_passed ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return all_passed ? 0 : 1;
}
