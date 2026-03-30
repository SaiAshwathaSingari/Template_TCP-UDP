/*
 * crypto.h - Cryptographic utilities
 *
 * Self-contained SHA-1, SHA-256, Base64, and random token generation.
 * SHA-1 is used for the WebSocket handshake (RFC 6455).
 * SHA-256 is used for password hashing with salt.
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

/* ===== SHA-1 Implementation (FIPS 180-4) ===== */

typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t  buffer[64];
    uint32_t buf_len;
} sha1_ctx;

static inline uint32_t sha1_rotl(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static void sha1_process_block(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    uint32_t a, b, c, d, e;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8)  | (uint32_t)block[i*4+3];
    for (i = 16; i < 80; i++)
        w[i] = sha1_rotl(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

    a = state[0]; b = state[1]; c = state[2];
    d = state[3]; e = state[4];

    for (i = 0; i < 80; i++) {
        uint32_t f, k, temp;
        if (i < 20)      { f = (b & c) | ((~b) & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;             k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else              { f = b ^ c ^ d;             k = 0xCA62C1D6; }
        temp = sha1_rotl(a, 5) + f + e + k + w[i];
        e = d; d = c; c = sha1_rotl(b, 30); b = a; a = temp;
    }

    state[0] += a; state[1] += b; state[2] += c;
    state[3] += d; state[4] += e;
}

static void sha1_init(sha1_ctx *ctx) {
    ctx->state[0] = 0x67452301; ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE; ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count = 0;
    ctx->buf_len = 0;
}

static void sha1_update(sha1_ctx *ctx, const uint8_t *data, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->buf_len++] = data[i];
        if (ctx->buf_len == 64) {
            sha1_process_block(ctx->state, ctx->buffer);
            ctx->buf_len = 0;
        }
    }
    ctx->count += len;
}

static void sha1_final(sha1_ctx *ctx, uint8_t digest[20]) {
    uint64_t total_bits = ctx->count * 8;
    uint8_t pad_byte = 0x80;
    uint8_t zero = 0;
    uint8_t len_buf[8];
    int i;

    sha1_update(ctx, &pad_byte, 1);
    while (ctx->buf_len != 56) {
        sha1_update(ctx, &zero, 1);
    }

    for (i = 0; i < 8; i++)
        len_buf[i] = (uint8_t)(total_bits >> (56 - i * 8));
    sha1_update(ctx, len_buf, 8);

    for (i = 0; i < 5; i++) {
        digest[i*4]   = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4+3] = (uint8_t)(ctx->state[i]);
    }
}

static void sha1_hash(const uint8_t *data, size_t len, uint8_t digest[20]) {
    sha1_ctx ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(&ctx, digest);
}

/* ===== SHA-256 Implementation (FIPS 180-4) ===== */

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t sha256_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static inline uint32_t sha256_ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static inline uint32_t sha256_maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static inline uint32_t sha256_ep0(uint32_t x) { return sha256_rotr(x,2) ^ sha256_rotr(x,13) ^ sha256_rotr(x,22); }
static inline uint32_t sha256_ep1(uint32_t x) { return sha256_rotr(x,6) ^ sha256_rotr(x,11) ^ sha256_rotr(x,25); }
static inline uint32_t sha256_sig0(uint32_t x) { return sha256_rotr(x,7) ^ sha256_rotr(x,18) ^ (x >> 3); }
static inline uint32_t sha256_sig1(uint32_t x) { return sha256_rotr(x,17) ^ sha256_rotr(x,19) ^ (x >> 10); }

typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buffer[64];
    uint32_t buf_len;
} sha256_ctx;

static void sha256_process_block(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64], a, b, c, d, e, f, g, h;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|
               ((uint32_t)block[i*4+2]<<8)|(uint32_t)block[i*4+3];
    for (i = 16; i < 64; i++)
        w[i] = sha256_sig1(w[i-2]) + w[i-7] + sha256_sig0(w[i-15]) + w[i-16];

    a=state[0]; b=state[1]; c=state[2]; d=state[3];
    e=state[4]; f=state[5]; g=state[6]; h=state[7];

    for (i = 0; i < 64; i++) {
        uint32_t t1 = h + sha256_ep1(e) + sha256_ch(e,f,g) + sha256_k[i] + w[i];
        uint32_t t2 = sha256_ep0(a) + sha256_maj(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

static void sha256_init(sha256_ctx *ctx) {
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
    ctx->count = 0;
    ctx->buf_len = 0;
}

static void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        ctx->buffer[ctx->buf_len++] = data[i];
        if (ctx->buf_len == 64) {
            sha256_process_block(ctx->state, ctx->buffer);
            ctx->buf_len = 0;
        }
    }
    ctx->count += len;
}

static void sha256_final(sha256_ctx *ctx, uint8_t digest[32]) {
    uint64_t total_bits = ctx->count * 8;
    uint8_t pad_byte = 0x80;
    uint8_t zero = 0;
    uint8_t len_buf[8];
    int i;

    sha256_update(ctx, &pad_byte, 1);
    while (ctx->buf_len != 56) {
        sha256_update(ctx, &zero, 1);
    }

    for (i = 0; i < 8; i++)
        len_buf[i] = (uint8_t)(total_bits >> (56 - i * 8));
    sha256_update(ctx, len_buf, 8);

    for (i = 0; i < 8; i++) {
        digest[i*4]   = (uint8_t)(ctx->state[i]>>24);
        digest[i*4+1] = (uint8_t)(ctx->state[i]>>16);
        digest[i*4+2] = (uint8_t)(ctx->state[i]>>8);
        digest[i*4+3] = (uint8_t)(ctx->state[i]);
    }
}

static void sha256_hash(const uint8_t *data, size_t len, uint8_t digest[32]) {
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

static void sha256_to_hex(const uint8_t digest[32], char hex[65]) {
    int i;
    for (i = 0; i < 32; i++) sprintf(hex + i*2, "%02x", digest[i]);
    hex[64] = '\0';
}

/* ===== Base64 Encoding ===== */

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_encode(const uint8_t *input, int input_len, char *output, int output_max) {
    int i, j = 0;
    for (i = 0; i < input_len; i += 3) {
        uint32_t octet_a = i < input_len ? input[i] : 0;
        uint32_t octet_b = (i+1) < input_len ? input[i+1] : 0;
        uint32_t octet_c = (i+2) < input_len ? input[i+2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        if (j + 4 >= output_max) return -1;
        output[j++] = b64_table[(triple >> 18) & 0x3F];
        output[j++] = b64_table[(triple >> 12) & 0x3F];
        output[j++] = (i+1 < input_len) ? b64_table[(triple >> 6) & 0x3F] : '=';
        output[j++] = (i+2 < input_len) ? b64_table[triple & 0x3F] : '=';
    }
    output[j] = '\0';
    return j;
}

/* ===== Random Token Generation ===== */

/* Requires platform.h to be included before crypto.h (via common.h) */
static void generate_random_token(char *token, int len) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    unsigned char buf[256];
    int i;
    platform_random_bytes(buf, len < 256 ? len : 256);
    for (i = 0; i < len; i++)
        token[i] = charset[buf[i] % (sizeof(charset) - 1)];
    token[len] = '\0';
}

/* ===== Password Hashing (SHA-256 + salt) ===== */

static void generate_salt(char *salt, int len) {
    generate_random_token(salt, len);
}

static void hash_password(const char *password, const char *salt, char *hash_out) {
    char combined[512];
    uint8_t digest[32];
    snprintf(combined, sizeof(combined), "%s:%s", salt, password);
    sha256_hash((const uint8_t *)combined, strlen(combined), digest);
    sha256_to_hex(digest, hash_out);
}

#endif /* CRYPTO_H */
