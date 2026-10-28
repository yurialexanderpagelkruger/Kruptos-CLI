#include "token.h"

#include "crypto.h"
#include "encoding.h"
#include "i18n.h"
#include "rng.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace kruptos::token {
namespace {

constexpr std::uint8_t kVersion       = 1;
constexpr std::uint8_t kFlagProtected = 0x01;
constexpr std::size_t  kSeedLength    = 32;
constexpr std::size_t  kSaltLength    = 16;
constexpr std::size_t  kNonceLength   = 12;
constexpr std::size_t  kMacLength     = 16;
constexpr std::size_t  kMaxIdLength   = 8192;
constexpr std::size_t  kMinInnerSize  = 1 + 1 + kSeedLength + 4;

void appendBe32(Bytes& buffer, std::uint32_t value) {
    buffer.push_back(static_cast<std::uint8_t>(value >> 24));
    buffer.push_back(static_cast<std::uint8_t>(value >> 16));
    buffer.push_back(static_cast<std::uint8_t>(value >> 8));
    buffer.push_back(static_cast<std::uint8_t>(value));
}

std::uint32_t readBe32(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

Bytes buildInner(const Plan& plan, std::uint32_t cipherHash) {
    if (plan.methods.empty() || plan.methods.size() > 64) {
        throw Error(tr(Msg::ErrIdCreateLayers));
    }
    if (plan.seed.size() != kSeedLength) {
        throw Error(tr(Msg::ErrIdCreateSeed));
    }

    Bytes inner;
    inner.reserve(1 + plan.methods.size() + kSeedLength + 4);
    inner.push_back(static_cast<std::uint8_t>(plan.methods.size()));
    inner.insert(inner.end(), plan.methods.begin(), plan.methods.end());
    inner.insert(inner.end(), plan.seed.begin(), plan.seed.end());
    appendBe32(inner, cipherHash);
    return inner;
}

Decoded parseInner(ByteSpan inner) {
    if (inner.size() < kMinInnerSize) throw Error(tr(Msg::ErrIdPlanIncomplete));

    const std::size_t count = inner[0];
    if (count < 1 || count > 64) throw Error(tr(Msg::ErrIdLayerCount));
    if (inner.size() != 1 + count + kSeedLength + 4) {
        throw Error(tr(Msg::ErrIdPlanLength));
    }

    Decoded decoded;
    decoded.plan.methods.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t id = inner[1 + i];
        if (findMethod(id) == nullptr) {
            throw Error(tr(Msg::ErrIdUnknownMethod, {std::to_string(id)}));
        }
        decoded.plan.methods.push_back(id);
    }

    const std::uint8_t* seedStart = inner.data() + 1 + count;
    decoded.plan.seed.assign(seedStart, seedStart + kSeedLength);
    decoded.cipherHash = readBe32(inner.data() + 1 + count + kSeedLength);
    return decoded;
}

std::string compactId(std::string_view id) {
    if (id.size() > kMaxIdLength) throw Error(tr(Msg::ErrIdTooLong));
    std::string compact;
    compact.reserve(id.size());
    for (char c : id) {
        if (isAsciiSpace(static_cast<unsigned char>(c)) || c == '-') continue;
        compact.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return compact;
}

Bytes payloadFromId(std::string_view id) {
    const std::string compact = compactId(id);
    if (!startsWith(compact, kIdPrefix)) {
        throw Error(tr(Msg::ErrIdPrefix));
    }
    const std::string body = compact.substr(3);
    if (body.empty()) throw Error(tr(Msg::ErrIdBody));

    Bytes payload = enc::base32Decode(body);
    if (payload.size() < 3) throw Error(tr(Msg::ErrIdShort));
    return payload;
}

std::string formatId(const Bytes& payload) {
    const std::string body = enc::base32Encode(payload);
    std::string id = "KR1-";
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (i > 0 && i % 8 == 0) id.push_back('-');
        id.push_back(body[i]);
    }
    return id;
}

} // namespace

Header peek(std::string_view id) {
    const Bytes payload = payloadFromId(id);

    Header header;
    header.version = payload[0];
    if (header.version != kVersion) {
        throw Error(tr(Msg::ErrIdVersion, {std::to_string(header.version)}));
    }
    if ((payload[1] & ~kFlagProtected) != 0) throw Error(tr(Msg::ErrIdFlags));
    header.passwordProtected = (payload[1] & kFlagProtected) != 0;
    return header;
}

std::string encode(const Plan& plan, std::uint32_t cipherHash, std::string_view passphrase) {
    const Bytes inner = buildInner(plan, cipherHash);

    Bytes payload;
    if (passphrase.empty()) {
        payload.push_back(kVersion);
        payload.push_back(0);
        payload.insert(payload.end(), inner.begin(), inner.end());

        const Bytes digest = crypto::sha256(payload);
        payload.insert(payload.end(), digest.begin(), digest.begin() + 4);
    } else {
        const Bytes salt  = secureRandom(kSaltLength);
        const Bytes nonce = secureRandom(kNonceLength);

        payload.push_back(kVersion);
        payload.push_back(kFlagProtected);
        payload.insert(payload.end(), salt.begin(), salt.end());
        payload.insert(payload.end(), nonce.begin(), nonce.end());

        Bytes keyMaterial = crypto::pbkdf2Sha256(passphrase, salt, kPbkdf2Iterations, 64);
        const ByteSpan encryptionKey(keyMaterial.data(), 32);
        const ByteSpan authenticationKey(keyMaterial.data() + 32, 32);

        Bytes ciphertext;
        crypto::chacha20Xor(encryptionKey, nonce, 1, inner, ciphertext);
        payload.insert(payload.end(), ciphertext.begin(), ciphertext.end());

        const Bytes mac = crypto::hmacSha256(authenticationKey, payload);
        payload.insert(payload.end(), mac.begin(), mac.begin() + kMacLength);

        std::fill(keyMaterial.begin(), keyMaterial.end(), 0);
    }

    return formatId(payload);
}

Decoded decode(std::string_view id, std::string_view passphrase) {
    const Bytes payload = payloadFromId(id);

    if (payload[0] != kVersion) {
        throw Error(tr(Msg::ErrIdVersion, {std::to_string(payload[0])}));
    }
    const std::uint8_t flags = payload[1];
    if ((flags & ~kFlagProtected) != 0) throw Error(tr(Msg::ErrIdFlags));

    Decoded decoded;
    decoded.passwordProtected = (flags & kFlagProtected) != 0;

    if (!decoded.passwordProtected) {
        constexpr std::size_t checksumLength = 4;
        if (payload.size() < 2 + kMinInnerSize + checksumLength) {
            throw Error(tr(Msg::ErrIdIncomplete));
        }
        const std::size_t bodyLength = payload.size() - checksumLength;
        const Bytes expected(payload.begin() + static_cast<std::ptrdiff_t>(bodyLength), payload.end());
        const Bytes digest = crypto::sha256(ByteSpan(payload.data(), bodyLength));

        if (!crypto::constantTimeEqual(expected, ByteSpan(digest.data(), checksumLength))) {
            throw Error(tr(Msg::ErrIdChecksum));
        }

        Decoded parsed = parseInner(ByteSpan(payload.data() + 2, bodyLength - 2));
        decoded.plan       = std::move(parsed.plan);
        decoded.cipherHash = parsed.cipherHash;
        return decoded;
    }

    if (passphrase.empty()) {
        throw Error(tr(Msg::ErrIdProtected));
    }
    const std::size_t minimum = 2 + kSaltLength + kNonceLength + kMinInnerSize + kMacLength;
    if (payload.size() < minimum) throw Error(tr(Msg::ErrIdProtectedDamaged));

    const std::size_t macOffset = payload.size() - kMacLength;
    const std::uint8_t* salt  = payload.data() + 2;
    const std::uint8_t* nonce = payload.data() + 2 + kSaltLength;
    const std::size_t ciphertextOffset = 2 + kSaltLength + kNonceLength;
    const std::size_t ciphertextLength = macOffset - ciphertextOffset;

    Bytes keyMaterial = crypto::pbkdf2Sha256(
        passphrase, ByteSpan(salt, kSaltLength), kPbkdf2Iterations, 64);
    const ByteSpan encryptionKey(keyMaterial.data(), 32);
    const ByteSpan authenticationKey(keyMaterial.data() + 32, 32);

    const Bytes expectedMac = crypto::hmacSha256(
        authenticationKey, ByteSpan(payload.data(), macOffset));
    if (!crypto::constantTimeEqual(ByteSpan(payload.data() + macOffset, kMacLength),
                                   ByteSpan(expectedMac.data(), kMacLength))) {
        std::fill(keyMaterial.begin(), keyMaterial.end(), 0);
        throw Error(tr(Msg::ErrWrongPassword));
    }

    Bytes inner;
    crypto::chacha20Xor(encryptionKey, ByteSpan(nonce, kNonceLength), 1,
                        ByteSpan(payload.data() + ciphertextOffset, ciphertextLength), inner);
    std::fill(keyMaterial.begin(), keyMaterial.end(), 0);

    Decoded parsed = parseInner(inner);
    decoded.plan       = std::move(parsed.plan);
    decoded.cipherHash = parsed.cipherHash;
    return decoded;
}

} // namespace kruptos::token
