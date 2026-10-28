#include "methods.h"

#include "crypto.h"
#include "encoding.h"
#include "i18n.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kruptos {
namespace {

bool appliesToAny(ByteSpan) { return true; }
bool appliesToLongEnough(ByteSpan input) { return input.size() >= 2; }
// Base58 usa aritmética de precisión arbitraria O(n²): se reserva para buffers
// pequeños y así no penaliza mensajes largos.
bool appliesToSmallBuffer(ByteSpan input) { return input.size() <= 4096; }

template <std::string (*Encode)(ByteSpan)>
void encodeWith(ByteSpan input, Bytes& output, KeyStream&) {
    output = toBytes(Encode(input));
}

template <Bytes (*Decode)(std::string_view)>
void decodeWith(ByteSpan input, Bytes& output, KeyStream&) {
    output = Decode(toString(input));
}

// ---------------------------------------------------------------- ChaCha20

void chacha20Forward(ByteSpan input, Bytes& output, KeyStream& key) {
    const Bytes keyBytes = key.take(32);
    const Bytes nonce    = key.take(12);
    crypto::chacha20Xor(keyBytes, nonce, 0, input, output);
}

// ---------------------------------------------------------------- RC4

void rc4Xor(ByteSpan input, Bytes& output, ByteSpan key) {
    if (key.empty()) throw Error("RC4: clave vacía");

    std::uint8_t state[256];
    for (int i = 0; i < 256; ++i) state[i] = static_cast<std::uint8_t>(i);

    unsigned j = 0;
    for (int i = 0; i < 256; ++i) {
        j = (j + state[i] + key[static_cast<std::size_t>(i) % key.size()]) & 0xFFu;
        std::swap(state[i], state[j]);
    }

    output.resize(input.size());
    unsigned i = 0;
    j = 0;
    for (std::size_t k = 0; k < input.size(); ++k) {
        i = (i + 1) & 0xFFu;
        j = (j + state[i]) & 0xFFu;
        std::swap(state[i], state[j]);
        const std::uint8_t stream = state[(state[i] + state[j]) & 0xFFu];
        output[k] = static_cast<std::uint8_t>(input[k] ^ stream);
    }
}

void rc4Step(ByteSpan input, Bytes& output, KeyStream& key) {
    const std::size_t keyLength = static_cast<std::size_t>(key.rangeInclusive(8, 32));
    const Bytes keyBytes = key.take(keyLength);
    rc4Xor(input, output, keyBytes);
}

// ---------------------------------------------------------------- Flujo basado en SHA-256

void hashStreamStep(ByteSpan input, Bytes& output, KeyStream& key) {
    const std::size_t keyLength = static_cast<std::size_t>(key.rangeInclusive(16, 32));
    const Bytes keyBytes = key.take(keyLength);

    output.resize(input.size());
    std::uint64_t counter = 0;
    for (std::size_t offset = 0; offset < input.size(); offset += 32) {
        Bytes message = toBytes("Kruptos/hash-stream/v1");
        message.insert(message.end(), keyBytes.begin(), keyBytes.end());
        for (int shift = 56; shift >= 0; shift -= 8) {
            message.push_back(static_cast<std::uint8_t>(counter >> shift));
        }
        ++counter;

        const Bytes block = crypto::sha256(message);
        const std::size_t chunk = std::min<std::size_t>(block.size(), input.size() - offset);
        for (std::size_t i = 0; i < chunk; ++i) {
            output[offset + i] = static_cast<std::uint8_t>(input[offset + i] ^ block[i]);
        }
    }
}

// ---------------------------------------------------------------- Clásicos sobre bytes

void caesarForward(ByteSpan input, Bytes& output, KeyStream& key) {
    const std::uint8_t shift = key.rangeInclusive(1, 255);
    output.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = static_cast<std::uint8_t>(input[i] + shift);
    }
}

void caesarInverse(ByteSpan input, Bytes& output, KeyStream& key) {
    const std::uint8_t shift = key.rangeInclusive(1, 255);
    output.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = static_cast<std::uint8_t>(input[i] - shift);
    }
}

void vigenereForward(ByteSpan input, Bytes& output, KeyStream& key) {
    const std::size_t keyLength = static_cast<std::size_t>(key.rangeInclusive(8, 32));
    const Bytes keyBytes = key.take(keyLength);
    output.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = static_cast<std::uint8_t>(input[i] + keyBytes[i % keyLength]);
    }
}

void vigenereInverse(ByteSpan input, Bytes& output, KeyStream& key) {
    const std::size_t keyLength = static_cast<std::size_t>(key.rangeInclusive(8, 32));
    const Bytes keyBytes = key.take(keyLength);
    output.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = static_cast<std::uint8_t>(input[i] - keyBytes[i % keyLength]);
    }
}

void atbashStep(ByteSpan input, Bytes& output, KeyStream&) {
    output.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = static_cast<std::uint8_t>(255u - input[i]);
    }
}

void rot13Step(ByteSpan input, Bytes& output, KeyStream&) {
    output.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        const std::uint8_t c = input[i];
        if (c >= 'A' && c <= 'Z') {
            output[i] = static_cast<std::uint8_t>('A' + (c - 'A' + 13) % 26);
        } else if (c >= 'a' && c <= 'z') {
            output[i] = static_cast<std::uint8_t>('a' + (c - 'a' + 13) % 26);
        } else {
            output[i] = c;
        }
    }
}

void rot47Step(ByteSpan input, Bytes& output, KeyStream&) {
    output.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        const std::uint8_t c = input[i];
        if (c >= 33 && c <= 126) {
            output[i] = static_cast<std::uint8_t>(33 + (c - 33 + 47) % 94);
        } else {
            output[i] = c;
        }
    }
}

void reverseStep(ByteSpan input, Bytes& output, KeyStream&) {
    output.assign(input.rbegin(), input.rend());
}

std::uint8_t rotateLeft(std::uint8_t value, unsigned bits) {
    return static_cast<std::uint8_t>((value << bits) | (value >> (8 - bits)));
}

std::uint8_t rotateRight(std::uint8_t value, unsigned bits) {
    return static_cast<std::uint8_t>((value >> bits) | (value << (8 - bits)));
}

void bitRotateForward(ByteSpan input, Bytes& output, KeyStream& key) {
    const unsigned bits = static_cast<unsigned>(key.rangeInclusive(1, 7));
    output.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = rotateLeft(input[i], bits);
    }
}

void bitRotateInverse(ByteSpan input, Bytes& output, KeyStream& key) {
    const unsigned bits = static_cast<unsigned>(key.rangeInclusive(1, 7));
    output.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = rotateRight(input[i], bits);
    }
}

void nibbleSwapStep(ByteSpan input, Bytes& output, KeyStream&) {
    output.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = static_cast<std::uint8_t>((input[i] << 4) | (input[i] >> 4));
    }
}

// ---------------------------------------------------------------- Cerca / Rail fence

std::vector<std::size_t> railRows(std::size_t length, std::size_t rails) {
    std::vector<std::size_t> rows;
    rows.reserve(length);
    if (length == 0) return rows;
    if (rails < 2) rails = 2;

    std::size_t row = 0;
    int direction = 1;
    for (std::size_t i = 0; i < length; ++i) {
        rows.push_back(row);
        if (row == 0) {
            direction = 1;
        } else if (row + 1 == rails) {
            direction = -1;
        }
        row = static_cast<std::size_t>(static_cast<long long>(row) + direction);
    }
    return rows;
}

void railFenceForward(ByteSpan input, Bytes& output, KeyStream& key) {
    const std::size_t length = input.size();
    std::size_t rails = static_cast<std::size_t>(key.rangeInclusive(2, 6));
    if (rails > length) rails = length;

    const std::vector<std::size_t> rows = railRows(length, rails);
    output.clear();
    output.reserve(length);
    for (std::size_t rail = 0; rail < rails; ++rail) {
        for (std::size_t i = 0; i < length; ++i) {
            if (rows[i] == rail) output.push_back(input[i]);
        }
    }
}

void railFenceInverse(ByteSpan input, Bytes& output, KeyStream& key) {
    const std::size_t length = input.size();
    std::size_t rails = static_cast<std::size_t>(key.rangeInclusive(2, 6));
    if (rails > length) rails = length;

    const std::vector<std::size_t> rows = railRows(length, rails);
    std::vector<std::size_t> counts(rails, 0);
    for (std::size_t row : rows) ++counts[row];

    std::vector<std::size_t> offsets(rails, 0);
    std::size_t position = 0;
    for (std::size_t rail = 0; rail < rails; ++rail) {
        offsets[rail] = position;
        position += counts[rail];
    }

    std::vector<std::size_t> consumed(rails, 0);
    output.resize(length);
    for (std::size_t i = 0; i < length; ++i) {
        const std::size_t rail = rows[i];
        output[i] = input[offsets[rail] + consumed[rail]];
        ++consumed[rail];
    }
}

} // namespace

const std::vector<Method>& methods() {
    static const std::vector<Method> table = {
        // --- Cifrados (preservan o casi preservan el tamaño) ---
        {1, "chacha20", Msg::MethodChaCha20,
         MethodKind::Cipher, false, &appliesToAny, &chacha20Forward, &chacha20Forward},
        {2, "rc4", Msg::MethodRc4,
         MethodKind::Cipher, false, &appliesToAny, &rc4Step, &rc4Step},
        {3, "sha256-ctr", Msg::MethodSha256Ctr,
         MethodKind::Cipher, false, &appliesToAny, &hashStreamStep, &hashStreamStep},
        {4, "caesar", Msg::MethodCaesar,
         MethodKind::Cipher, false, &appliesToAny, &caesarForward, &caesarInverse},
        {5, "vigenere", Msg::MethodVigenere,
         MethodKind::Cipher, false, &appliesToAny, &vigenereForward, &vigenereInverse},
        {6, "atbash", Msg::MethodAtbash,
         MethodKind::Cipher, false, &appliesToAny, &atbashStep, &atbashStep},
        {7, "rot13", Msg::MethodRot13,
         MethodKind::Cipher, false, &appliesToAny, &rot13Step, &rot13Step},
        {8, "rot47", Msg::MethodRot47,
         MethodKind::Cipher, false, &appliesToAny, &rot47Step, &rot47Step},
        {9, "reverse", Msg::MethodReverse,
         MethodKind::Cipher, false, &appliesToAny, &reverseStep, &reverseStep},
        {10, "bit-rotate", Msg::MethodBitRotate,
         MethodKind::Cipher, false, &appliesToAny, &bitRotateForward, &bitRotateInverse},
        {11, "nibble-swap", Msg::MethodNibbleSwap,
         MethodKind::Cipher, false, &appliesToAny, &nibbleSwapStep, &nibbleSwapStep},
        {12, "rail-fence", Msg::MethodRailFence,
         MethodKind::Cipher, false, &appliesToLongEnough, &railFenceForward, &railFenceInverse},

        // --- Codificaciones (todas reversibles y en ASCII imprimible) ---
        {32, "base64", Msg::MethodBase64,
         MethodKind::Encoding, true, &appliesToAny,
         &encodeWith<enc::base64Encode>, &decodeWith<enc::base64Decode>},
        {33, "base64url", Msg::MethodBase64Url,
         MethodKind::Encoding, true, &appliesToAny,
         &encodeWith<enc::base64UrlEncode>, &decodeWith<enc::base64UrlDecode>},
        {34, "base32", Msg::MethodBase32,
         MethodKind::Encoding, true, &appliesToAny,
         &encodeWith<enc::base32Encode>, &decodeWith<enc::base32Decode>},
        {35, "base16", Msg::MethodBase16,
         MethodKind::Encoding, true, &appliesToAny,
         &encodeWith<enc::base16Encode>, &decodeWith<enc::base16Decode>},
        {36, "base58", Msg::MethodBase58,
         MethodKind::Encoding, true, &appliesToSmallBuffer,
         &encodeWith<enc::base58Encode>, &decodeWith<enc::base58Decode>},
        {37, "base85", Msg::MethodBase85,
         MethodKind::Encoding, true, &appliesToAny,
         &encodeWith<enc::base85Encode>, &decodeWith<enc::base85Decode>},
        {38, "percent", Msg::MethodPercent,
         MethodKind::Encoding, true, &appliesToAny,
         &encodeWith<enc::percentEncode>, &decodeWith<enc::percentDecode>},
        {39, "quoted-printable", Msg::MethodQuotedPrintable,
         MethodKind::Encoding, true, &appliesToAny,
         &encodeWith<enc::quotedPrintableEncode>, &decodeWith<enc::quotedPrintableDecode>},
        {40, "binary", Msg::MethodBinary,
         MethodKind::Encoding, false, &appliesToAny,
         &encodeWith<enc::binaryEncode>, &decodeWith<enc::binaryDecode>},
        {41, "octal", Msg::MethodOctal,
         MethodKind::Encoding, false, &appliesToAny,
         &encodeWith<enc::octalEncode>, &decodeWith<enc::octalDecode>},
        {42, "decimal", Msg::MethodDecimal,
         MethodKind::Encoding, false, &appliesToAny,
         &encodeWith<enc::decimalEncode>, &decodeWith<enc::decimalDecode>},
        {43, "unicode-escape", Msg::MethodUnicodeEscape,
         MethodKind::Encoding, true, &appliesToAny,
         &encodeWith<enc::unicodeEscapeEncode>, &decodeWith<enc::unicodeEscapeDecode>},
        {44, "html-entities", Msg::MethodHtmlEntities,
         MethodKind::Encoding, true, &appliesToAny,
         &encodeWith<enc::htmlEntityEncode>, &decodeWith<enc::htmlEntityDecode>},
        {45, "morse", Msg::MethodMorse,
         MethodKind::Encoding, false, &enc::morseApplicable,
         &encodeWith<enc::morseEncode>, &decodeWith<enc::morseDecode>},
    };
    return table;
}

const Method* findMethod(std::uint8_t id) {
    for (const Method& method : methods()) {
        if (method.id == id) return &method;
    }
    return nullptr;
}

const char* methodKindName(MethodKind kind) {
    return tr(kind == MethodKind::Cipher ? Msg::KindCipher : Msg::KindEncoding);
}

} // namespace kruptos
