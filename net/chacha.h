/* ChaCha20-Poly1305 (RFC 8439), portable C, no libc, no globals (safe for the read-only flash cave). */
#ifndef CHACHA_H
#define CHACHA_H
#include <stdint.h>

void cc20_block(const uint8_t key[32], uint32_t counter, const uint8_t nonce[12], uint8_t out[64]);
/* ct must have room for len + 16 (the tag is appended). pt and ct may be the same buffer. */
void aead_seal(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, int aadlen, const uint8_t *pt, int len, uint8_t *ct);
/* ct holds len bytes of ciphertext followed by the 16-byte tag. Returns 0 on success (pt written), -1 if the tag does not match. */
int aead_open(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, int aadlen, const uint8_t *ct, int len, uint8_t *pt);
#endif
