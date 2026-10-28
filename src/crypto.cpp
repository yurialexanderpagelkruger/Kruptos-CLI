#include "crypto.h"

#include <algorithm>
#include <cstring>

namespace kruptos::crypto {
namespace {

constexpr std::uint32_t kSha256K[64] = {
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
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

inline std::uint32_t rotr32(std::uint32_t value, int bits) {
    return (value >> bits) | (value << (32 - bits));
}

inline std::uint32_t rotl32(std::uint32_t value, int bits) {
    return (value << bits) | (value >> (32 - bits));
}

inline std::uint32_t loadBe32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

inline void storeBe32(std::uint8_t* p, std::uint32_t value) {
    p[0] = static_cast<std::uint8_t>(value >> 24);
    p[1] = static_cast<std::uint8_t>(value >> 16);
    p[2] = static_cast<std::uint8_t>(value >> 8);
    p[3] = static_cast<std::uint8_t>(value);
}

inline std::uint32_t loadLe32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0])) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

inline void storeLe32(std::uint8_t* p, std::uint32_t value) {
    p[0] = static_cast<std::uint8_t>(value);
    p[1] = static_cast<std::uint8_t>(value >> 8);
    p[2] = static_cast<std::uint8_t>(value >> 16);
    p[3] = static_cast<std::uint8_t>(value >> 24);
}

inline void chachaQuarterRound(std::uint32_t& a, std::uint32_t& b,
                               std::uint32_t& c, std::uint32_t& d) {
    a += b; d ^= a; d = rotl32(d, 16);
    c += d; b ^= c; b = rotl32(b, 12);
    a += b; d ^= a; d = rotl32(d, 8);
    c += d; b ^= c; b = rotl32(b, 7);
}

void chacha20Block(const std::uint8_t key[32], const std::uint8_t nonce[12],
                   std::uint32_t counter, std::uint8_t out[64]) {
    std::uint32_t state[16];
    state[0] = 0x61707865u; // "expa"
    state[1] = 0x3320646eu; // "nd 3"
    state[2] = 0x79622d32u; // "2-by"
    state[3] = 0x6b206574u; // "te k"
    for (int i = 0; i < 8; ++i) state[4 + i] = loadLe32(key + 4 * i);
    state[12] = counter;
    for (int i = 0; i < 3; ++i) state[13 + i] = loadLe32(nonce + 4 * i);

    std::uint32_t w[16];
    std::memcpy(w, state, sizeof(w));
    for (int round = 0; round < 10; ++round) {
        chachaQuarterRound(w[0], w[4], w[8],  w[12]);
        chachaQuarterRound(w[1], w[5], w[9],  w[13]);
        chachaQuarterRound(w[2], w[6], w[10], w[14]);
        chachaQuarterRound(w[3], w[7], w[11], w[15]);
        chachaQuarterRound(w[0], w[5], w[10], w[15]);
        chachaQuarterRound(w[1], w[6], w[11], w[12]);
        chachaQuarterRound(w[2], w[7], w[8],  w[13]);
        chachaQuarterRound(w[3], w[4], w[9],  w[14]);
    }
    for (int i = 0; i < 16; ++i) storeLe32(out + 4 * i, w[i] + state[i]);
}

} // namespace

Bytes sha256(ByteSpan data) {
    std::uint32_t h[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };

    Bytes message(data.begin(), data.end());
    const std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8ull;
    message.push_back(0x80);
    while (message.size() % 64 != 56) message.push_back(0x00);
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>(bitLength >> shift));
    }

    std::uint32_t w[64];
    for (std::size_t offset = 0; offset < message.size(); offset += 64) {
        for (int i = 0; i < 16; ++i) {
            w[i] = loadBe32(message.data() + offset + 4 * i);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = hh + s1 + ch + kSha256K[i] + w[i];
            const std::uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + maj;

            hh = g;
            g  = f;
            f  = e;
            e  = d + temp1;
            d  = c;
            c  = b;
            b  = a;
            a  = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    Bytes digest(32);
    for (int i = 0; i < 8; ++i) storeBe32(digest.data() + 4 * i, h[i]);
    return digest;
}

Bytes hmacSha256(ByteSpan key, ByteSpan data) {
    std::uint8_t block[64] = {0};
    if (key.size() > 64) {
        const Bytes hashed = sha256(key);
        std::memcpy(block, hashed.data(), hashed.size());
    } else if (!key.empty()) {
        std::memcpy(block, key.data(), key.size());
    }

    std::uint8_t innerPad[64];
    std::uint8_t outerPad[64];
    for (int i = 0; i < 64; ++i) {
        innerPad[i] = static_cast<std::uint8_t>(block[i] ^ 0x36);
        outerPad[i] = static_cast<std::uint8_t>(block[i] ^ 0x5C);
    }

    Bytes inner;
    inner.reserve(64 + data.size());
    inner.insert(inner.end(), innerPad, innerPad + 64);
    inner.insert(inner.end(), data.begin(), data.end());
    const Bytes innerHash = sha256(inner);

    Bytes outer;
    outer.reserve(96);
    outer.insert(outer.end(), outerPad, outerPad + 64);
    outer.insert(outer.end(), innerHash.begin(), innerHash.end());
    return sha256(outer);
}

Bytes pbkdf2Sha256(std::string_view password, ByteSpan salt,
                   std::uint32_t iterations, std::size_t outputLength) {
    if (iterations == 0) throw Error("PBKDF2: Invalid iteration count");
    if (outputLength == 0) throw Error("PBKDF2: Invalid output length");

    const ByteSpan passwordBytes(reinterpret_cast<const std::uint8_t*>(password.data()),
                                 password.size());

    Bytes output;
    output.reserve(outputLength);

    std::uint32_t blockIndex = 1;
    while (output.size() < outputLength) {
        Bytes message(salt.begin(), salt.end());
        message.push_back(static_cast<std::uint8_t>(blockIndex >> 24));
        message.push_back(static_cast<std::uint8_t>(blockIndex >> 16));
        message.push_back(static_cast<std::uint8_t>(blockIndex >> 8));
        message.push_back(static_cast<std::uint8_t>(blockIndex));

        Bytes u = hmacSha256(passwordBytes, message);
        Bytes accumulator = u;

        for (std::uint32_t round = 1; round < iterations; ++round) {
            u = hmacSha256(passwordBytes, u);
            for (std::size_t i = 0; i < accumulator.size(); ++i) {
                accumulator[i] ^= u[i];
            }
        }

        const std::size_t remaining = outputLength - output.size();
        const std::size_t take = std::min<std::size_t>(accumulator.size(), remaining);
        output.insert(output.end(), accumulator.begin(), accumulator.begin() + static_cast<std::ptrdiff_t>(take));
        ++blockIndex;
    }
    return output;
}

void chacha20Xor(ByteSpan key, ByteSpan nonce, std::uint32_t counter,
                 ByteSpan input, Bytes& output) {
    if (key.size() != 32) throw Error("ChaCha20: The key must be 32 bytes");
    if (nonce.size() != 12) throw Error("ChaCha20: The nonce must be 12 bytes");

    output.assign(input.size(), 0);
    std::uint8_t stream[64];
    std::size_t offset = 0;
    while (offset < input.size()) {
        chacha20Block(key.data(), nonce.data(), counter, stream);
        const std::size_t chunk = std::min<std::size_t>(64, input.size() - offset);
        for (std::size_t i = 0; i < chunk; ++i) {
            output[offset + i] = static_cast<std::uint8_t>(input[offset + i] ^ stream[i]);
        }
        offset += chunk;
        ++counter;
    }
}

bool constantTimeEqual(ByteSpan a, ByteSpan b) {
    if (a.size() != b.size()) return false;
    std::uint8_t difference = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        difference = static_cast<std::uint8_t>(difference | (a[i] ^ b[i]));
    }
    return difference == 0;
}

} // namespace kruptos::crypto
