/*
 * core_scene_compile_digest.c
 * Part of the CodeWork Shared Libraries
 */

#include "core_scene_compile.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Sha256Context {
    uint32_t state[8];
    uint64_t bit_count;
    unsigned char block[64];
    size_t block_size;
} Sha256Context;

static uint32_t rotate_right(uint32_t value, unsigned int count) {
    return (value >> count) | (value << (32u - count));
}

static void sha256_transform(Sha256Context *context, const unsigned char block[64]) {
    static const uint32_t constants[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };
    uint32_t words[64];
    uint32_t a, b, c, d, e, f, g, h;
    for (size_t i = 0; i < 16; ++i) {
        words[i] = ((uint32_t)block[i * 4] << 24u) | ((uint32_t)block[i * 4 + 1] << 16u) |
                   ((uint32_t)block[i * 4 + 2] << 8u) | (uint32_t)block[i * 4 + 3];
    }
    for (size_t i = 16; i < 64; ++i) {
        uint32_t s0 = rotate_right(words[i - 15], 7u) ^ rotate_right(words[i - 15], 18u) ^
                      (words[i - 15] >> 3u);
        uint32_t s1 = rotate_right(words[i - 2], 17u) ^ rotate_right(words[i - 2], 19u) ^
                      (words[i - 2] >> 10u);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }
    a = context->state[0]; b = context->state[1]; c = context->state[2]; d = context->state[3];
    e = context->state[4]; f = context->state[5]; g = context->state[6]; h = context->state[7];
    for (size_t i = 0; i < 64; ++i) {
        uint32_t s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
        uint32_t choice = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + choice + constants[i] + words[i];
        uint32_t s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + majority;
        h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
    }
    context->state[0] += a; context->state[1] += b; context->state[2] += c;
    context->state[3] += d; context->state[4] += e; context->state[5] += f;
    context->state[6] += g; context->state[7] += h;
}

static void sha256_update(Sha256Context *context, const unsigned char *data, size_t size) {
    while (size > 0u) {
        size_t available = sizeof(context->block) - context->block_size;
        size_t amount = size < available ? size : available;
        memcpy(context->block + context->block_size, data, amount);
        context->block_size += amount;
        context->bit_count += (uint64_t)amount * 8u;
        data += amount;
        size -= amount;
        if (context->block_size == sizeof(context->block)) {
            sha256_transform(context, context->block);
            context->block_size = 0u;
        }
    }
}

CoreResult core_scene_compile_sha256(const void *data,
                                     size_t data_size,
                                     char out_hex[CORE_SCENE_COMPILE_SHA256_HEX_SIZE]) {
    static const char hex[] = "0123456789abcdef";
    Sha256Context context = {{0}, 0u, {0}, 0u};
    unsigned char digest[32];
    uint64_t bit_count;
    if ((!data && data_size > 0u) || !out_hex) return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    context.state[0] = 0x6a09e667u; context.state[1] = 0xbb67ae85u;
    context.state[2] = 0x3c6ef372u; context.state[3] = 0xa54ff53au;
    context.state[4] = 0x510e527fu; context.state[5] = 0x9b05688cu;
    context.state[6] = 0x1f83d9abu; context.state[7] = 0x5be0cd19u;
    sha256_update(&context, (const unsigned char *)data, data_size);
    bit_count = context.bit_count;
    context.block[context.block_size++] = 0x80u;
    if (context.block_size > 56u) {
        memset(context.block + context.block_size, 0, 64u - context.block_size);
        sha256_transform(&context, context.block);
        context.block_size = 0u;
    }
    memset(context.block + context.block_size, 0, 56u - context.block_size);
    for (size_t i = 0; i < 8u; ++i) context.block[63u - i] = (unsigned char)(bit_count >> (i * 8u));
    sha256_transform(&context, context.block);
    for (size_t i = 0; i < 8u; ++i) {
        digest[i * 4] = (unsigned char)(context.state[i] >> 24u);
        digest[i * 4 + 1] = (unsigned char)(context.state[i] >> 16u);
        digest[i * 4 + 2] = (unsigned char)(context.state[i] >> 8u);
        digest[i * 4 + 3] = (unsigned char)context.state[i];
    }
    for (size_t i = 0; i < sizeof(digest); ++i) {
        out_hex[i * 2] = hex[digest[i] >> 4u];
        out_hex[i * 2 + 1] = hex[digest[i] & 0x0fu];
    }
    out_hex[64] = '\0';
    return core_result_ok();
}
