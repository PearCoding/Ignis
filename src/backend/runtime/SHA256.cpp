#include "SHA256.h"

#include <cstring>

// Based on http://www.zedwood.com/article/cpp-sha256-function

namespace IG {
static const std::array<uint32, 64> SHA256_K = { 0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
                                                 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                                                 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
                                                 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                                                 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
                                                 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                                                 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
                                                 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                                                 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
                                                 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                                                 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
                                                 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                                                 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
                                                 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                                                 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
                                                 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

static inline uint32 rotr(uint32 x, uint32 n) { return (x >> n) | (x << ((sizeof(x) << 3) - n)); }
static inline uint32 ch(uint32 x, uint32 y, uint32 z) { return (x & y) ^ (~x & z); }
static inline uint32 maj(uint32 x, uint32 y, uint32 z) { return (x & y) ^ (x & z) ^ (y & z); }

static inline uint32 f1(uint32 x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
static inline uint32 f2(uint32 x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
static inline uint32 f3(uint32 x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
static inline uint32 f4(uint32 x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

SHA256::SHA256()
    : mTotalLength(0)
    , mCurrentLength(0)
    , mState{ 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 }
{
}

constexpr size_t SHA224_256_BLOCK_SIZE = 512 / 8;
void SHA256::update(const uint8* message, size_t len)
{
    size_t avl_len = SHA224_256_BLOCK_SIZE - mCurrentLength;
    size_t rem_len = len < avl_len ? len : avl_len;
    memcpy(&mBlock[mCurrentLength], message, rem_len);

    if (mCurrentLength + len < SHA224_256_BLOCK_SIZE) {
        mCurrentLength += len;
        return;
    }

    const size_t new_len         = len - rem_len;
    const size_t block_nb        = new_len / SHA224_256_BLOCK_SIZE;
    const uint8* shifted_message = message + rem_len;

    transform(mBlock.data(), 1);
    transform(shifted_message, block_nb);

    rem_len = new_len % SHA224_256_BLOCK_SIZE;
    memcpy(mBlock.data(), &shifted_message[block_nb << 6], rem_len);

    mCurrentLength = rem_len;
    mTotalLength += (block_nb + 1) << 6;
}

void SHA256::update(const std::string_view& str)
{
    update(reinterpret_cast<const uint8*>(str.data()), str.length());
}

std::string SHA256::final()
{
    const size_t block_nb = (1 + ((SHA224_256_BLOCK_SIZE - 9) < (mCurrentLength % SHA224_256_BLOCK_SIZE)));
    const size_t len_b    = (mTotalLength + mCurrentLength) << 3;
    const size_t pm_len   = block_nb << 6;
    memset(&mBlock[mCurrentLength], 0, pm_len - mCurrentLength);

    mBlock[mCurrentLength] = 0x80;
    mBlock[pm_len - 1]     = (uint8)len_b;
    mBlock[pm_len - 2]     = (uint8)(len_b >> 8);
    mBlock[pm_len - 3]     = (uint8)(len_b >> 16);
    mBlock[pm_len - 4]     = (uint8)(len_b >> 24);
    transform(mBlock.data(), block_nb);

    static const char characters[] = "0123456789ABCDEF";
    std::string output(64, '0');
    for (int i = 0; i < 8; ++i) {
        uint8 b3 = (uint8)mState[i];
        uint8 b2 = (uint8)(mState[i] >> 8);
        uint8 b1 = (uint8)(mState[i] >> 16);
        uint8 b0 = (uint8)(mState[i] >> 24);

        output[i * 8 + 7] = characters[b3 & 0xF];
        output[i * 8 + 6] = characters[b3 >> 4];

        output[i * 8 + 5] = characters[b2 & 0xF];
        output[i * 8 + 4] = characters[b2 >> 4];

        output[i * 8 + 3] = characters[b1 & 0xF];
        output[i * 8 + 2] = characters[b1 >> 4];

        output[i * 8 + 1] = characters[b0 & 0xF];
        output[i * 8 + 0] = characters[b0 >> 4];
    }

    return output;
}

void SHA256::transform(const uint8* message, size_t block_nb)
{
    std::array<uint32, 64> w;
    std::array<uint32, 8> wv;

    for (size_t i = 0; i < block_nb; i++) {
        const uint8* sub_block = message + (i << 6);
        for (int j = 0; j < 16; ++j) {
            const uint8* block = &sub_block[j << 2];

            w[j] = ((uint32) * (block + 3))
                   | ((uint32) * (block + 2) << 8)
                   | ((uint32) * (block + 1) << 16)
                   | ((uint32) * (block + 0) << 24);
        }

        for (int j = 16; j < 64; ++j)
            w[j] = f4(w[j - 2]) + w[j - 7] + f3(w[j - 15]) + w[j - 16];

        for (int j = 0; j < 8; ++j)
            wv[j] = mState[j];

        for (int j = 0; j < 64; ++j) {
            uint32 t1 = wv[7] + f2(wv[4]) + ch(wv[4], wv[5], wv[6])
                        + SHA256_K[j] + w[j];
            uint32 t2 = f1(wv[0]) + maj(wv[0], wv[1], wv[2]);
            wv[7]     = wv[6];
            wv[6]     = wv[5];
            wv[5]     = wv[4];
            wv[4]     = wv[3] + t1;
            wv[3]     = wv[2];
            wv[2]     = wv[1];
            wv[1]     = wv[0];
            wv[0]     = t1 + t2;
        }

        for (int j = 0; j < 8; ++j)
            mState[j] += wv[j];
    }
}

} // namespace IG