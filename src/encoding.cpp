#include "encoding.h"

#include "i18n.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace kruptos::enc {
namespace {

constexpr char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr char kBase64UrlAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
constexpr char kBase32Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
constexpr char kBase58Alphabet[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::string codecName(std::string_view name) {
    if (name == "url") return "URL";
    if (name == "html") return "HTML";
    std::string out(name);
    if (!out.empty()) {
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    }
    return out;
}

std::string codecError(std::string_view name, Msg message) {
    return codecName(name) + ": " + tr(message);
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int base64Value(char c, const char* alphabet) {
    for (int i = 0; i < 64; ++i) {
        if (alphabet[i] == c) return i;
    }
    return -1;
}

int base32Value(char c) {
    for (int i = 0; i < 32; ++i) {
        if (kBase32Alphabet[i] == c) return i;
    }
    return -1;
}

int base58Value(char c) {
    for (int i = 0; i < 58; ++i) {
        if (kBase58Alphabet[i] == c) return i;
    }
    return -1;
}

std::string base64EncodeImpl(ByteSpan data, const char* alphabet, bool padding) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);

    std::size_t index = 0;
    while (index + 3 <= data.size()) {
        const std::uint32_t value = (static_cast<std::uint32_t>(data[index]) << 16) |
                                    (static_cast<std::uint32_t>(data[index + 1]) << 8) |
                                    static_cast<std::uint32_t>(data[index + 2]);
        out.push_back(alphabet[(value >> 18) & 0x3F]);
        out.push_back(alphabet[(value >> 12) & 0x3F]);
        out.push_back(alphabet[(value >> 6) & 0x3F]);
        out.push_back(alphabet[value & 0x3F]);
        index += 3;
    }

    const std::size_t remaining = data.size() - index;
    if (remaining == 1) {
        const std::uint32_t value = static_cast<std::uint32_t>(data[index]) << 16;
        out.push_back(alphabet[(value >> 18) & 0x3F]);
        out.push_back(alphabet[(value >> 12) & 0x3F]);
        if (padding) {
            out.push_back('=');
            out.push_back('=');
        }
    } else if (remaining == 2) {
        const std::uint32_t value = (static_cast<std::uint32_t>(data[index]) << 16) |
                                    (static_cast<std::uint32_t>(data[index + 1]) << 8);
        out.push_back(alphabet[(value >> 18) & 0x3F]);
        out.push_back(alphabet[(value >> 12) & 0x3F]);
        out.push_back(alphabet[(value >> 6) & 0x3F]);
        if (padding) out.push_back('=');
    }
    return out;
}

Bytes base64DecodeImpl(std::string_view text, const char* alphabet) {
    std::string compact;
    compact.reserve(text.size());
    for (char c : text) {
        if (isAsciiSpace(static_cast<unsigned char>(c))) continue;
        compact.push_back(c);
    }

    while (!compact.empty() && compact.back() == '=') compact.pop_back();
    if (compact.size() % 4 == 1) {
        throw Error(codecError("base64", Msg::CodecInvalidLength));
    }

    Bytes out;
    out.reserve((compact.size() * 3) / 4);
    std::uint32_t accumulator = 0;
    int bits = 0;
    for (char c : compact) {
        const int value = base64Value(c, alphabet);
        if (value < 0) throw Error(codecError("base64", Msg::CodecInvalidCharacter));
        accumulator = (accumulator << 6) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((accumulator >> bits) & 0xFF));
            accumulator &= (bits == 0) ? 0u : ((1u << bits) - 1u);
        }
    }
    if (bits > 0 && accumulator != 0) {
        throw Error(codecError("base64", Msg::CodecResidualBits));
    }
    return out;
}

// ---------------------------------------------------------------- Base32

std::string base32EncodeImpl(ByteSpan data) {
    std::string out;
    out.reserve((data.size() * 8 + 4) / 5);
    std::uint32_t accumulator = 0;
    int bits = 0;
    for (std::uint8_t byte : data) {
        accumulator = (accumulator << 8) | byte;
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            out.push_back(kBase32Alphabet[(accumulator >> bits) & 0x1F]);
        }
        accumulator &= (bits == 0) ? 0u : ((1u << bits) - 1u);
    }
    if (bits > 0) {
        out.push_back(kBase32Alphabet[(accumulator << (5 - bits)) & 0x1F]);
    }
    return out;
}

Bytes base32DecodeImpl(std::string_view text) {
    std::string compact;
    compact.reserve(text.size());
    for (char c : text) {
        if (isAsciiSpace(static_cast<unsigned char>(c))) continue;
        if (c == '=') continue; // relleno opcional
        compact.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    Bytes out;
    out.reserve((compact.size() * 5) / 8);
    std::uint32_t accumulator = 0;
    int bits = 0;
    for (char c : compact) {
        const int value = base32Value(c);
        if (value < 0) throw Error(codecError("base32", Msg::CodecInvalidCharacter));
        accumulator = (accumulator << 5) | static_cast<std::uint32_t>(value);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((accumulator >> bits) & 0xFF));
            accumulator &= (bits == 0) ? 0u : ((1u << bits) - 1u);
        }
    }
    if (bits > 0 && accumulator != 0) {
        throw Error(codecError("base32", Msg::CodecResidualBits));
    }
    return out;
}

// ---------------------------------------------------------------- Base58

std::string base58EncodeImpl(ByteSpan data) {
    std::size_t zeros = 0;
    while (zeros < data.size() && data[zeros] == 0) ++zeros;

    std::vector<std::uint8_t> digits; // base 58, little endian
    digits.reserve(data.size());
    for (std::size_t i = zeros; i < data.size(); ++i) {
        unsigned carry = data[i];
        for (std::size_t j = 0; j < digits.size(); ++j) {
            carry += 256u * digits[j];
            digits[j] = static_cast<std::uint8_t>(carry % 58u);
            carry /= 58u;
        }
        while (carry > 0) {
            digits.push_back(static_cast<std::uint8_t>(carry % 58u));
            carry /= 58u;
        }
    }

    std::string out(zeros, '1');
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        out.push_back(kBase58Alphabet[*it]);
    }
    return out;
}

Bytes base58DecodeImpl(std::string_view text) {
    std::string compact;
    compact.reserve(text.size());
    for (char c : text) {
        if (!isAsciiSpace(static_cast<unsigned char>(c))) compact.push_back(c);
    }

    std::size_t zeros = 0;
    while (zeros < compact.size() && compact[zeros] == '1') ++zeros;

    std::vector<std::uint8_t> bytes; // base 256, little endian
    bytes.reserve(compact.size());
    for (std::size_t i = zeros; i < compact.size(); ++i) {
        const int value = base58Value(compact[i]);
        if (value < 0) throw Error(codecError("base58", Msg::CodecInvalidCharacter));
        unsigned carry = static_cast<unsigned>(value);
        for (std::size_t j = 0; j < bytes.size(); ++j) {
            carry += 58u * bytes[j];
            bytes[j] = static_cast<std::uint8_t>(carry & 0xFF);
            carry >>= 8;
        }
        while (carry > 0) {
            bytes.push_back(static_cast<std::uint8_t>(carry & 0xFF));
            carry >>= 8;
        }
    }

    Bytes out(zeros, 0);
    out.insert(out.end(), bytes.rbegin(), bytes.rend());
    return out;
}

// ---------------------------------------------------------------- Base85 (Ascii85)

std::string base85EncodeImpl(ByteSpan data) {
    std::string out = "<~";
    out.reserve(data.size() * 5 / 4 + 8);

    std::size_t index = 0;
    for (; index + 4 <= data.size(); index += 4) {
        const std::uint32_t value = (static_cast<std::uint32_t>(data[index]) << 24) |
                                    (static_cast<std::uint32_t>(data[index + 1]) << 16) |
                                    (static_cast<std::uint32_t>(data[index + 2]) << 8) |
                                    static_cast<std::uint32_t>(data[index + 3]);
        char chunk[5];
        std::uint32_t rest = value;
        for (int i = 4; i >= 0; --i) {
            chunk[i] = static_cast<char>(33 + rest % 85u);
            rest /= 85u;
        }
        out.append(chunk, 5);
    }

    const std::size_t remaining = data.size() - index;
    if (remaining > 0) {
        std::uint8_t padded[4] = {0, 0, 0, 0};
        for (std::size_t i = 0; i < remaining; ++i) padded[i] = data[index + i];
        const std::uint32_t value = (static_cast<std::uint32_t>(padded[0]) << 24) |
                                    (static_cast<std::uint32_t>(padded[1]) << 16) |
                                    (static_cast<std::uint32_t>(padded[2]) << 8) |
                                    static_cast<std::uint32_t>(padded[3]);
        char chunk[5];
        std::uint32_t rest = value;
        for (int i = 4; i >= 0; --i) {
            chunk[i] = static_cast<char>(33 + rest % 85u);
            rest /= 85u;
        }
        out.append(chunk, remaining + 1);
    }

    out += "~>";
    return out;
}

Bytes base85DecodeImpl(std::string_view text) {
    std::string compact;
    compact.reserve(text.size());
    for (char c : text) {
        if (!isAsciiSpace(static_cast<unsigned char>(c))) compact.push_back(c);
    }
    if (startsWith(compact, "<~")) compact.erase(0, 2);
    if (endsWith(compact, "~>")) compact.erase(compact.size() - 2);

    Bytes out;
    out.reserve(compact.size() * 4 / 5 + 4);

    std::size_t index = 0;
    while (index < compact.size()) {
        const std::size_t groupSize = std::min<std::size_t>(5, compact.size() - index);
        if (groupSize == 1) throw Error(codecError("base85", Msg::CodecIncompleteSequence));

        std::uint64_t value = 0;
        for (std::size_t i = 0; i < groupSize; ++i) {
            const char c = compact[index + i];
            if (c < 33 || c > 117) throw Error(codecError("base85", Msg::CodecInvalidCharacter));
            value = value * 85u + static_cast<unsigned>(c - 33);
        }
        // Relleno estándar de Ascii85 para grupos incompletos: 'u' (84).
        for (std::size_t i = groupSize; i < 5; ++i) value = value * 85u + 84u;
        if (value > 0xFFFFFFFFull) throw Error(codecError("base85", Msg::CodecInvalidValue));

        std::uint8_t bytes[4] = {
            static_cast<std::uint8_t>(value >> 24),
            static_cast<std::uint8_t>(value >> 16),
            static_cast<std::uint8_t>(value >> 8),
            static_cast<std::uint8_t>(value),
        };
        const std::size_t produce = (groupSize == 5) ? 4 : (groupSize - 1);
        out.insert(out.end(), bytes, bytes + produce);
        index += groupSize;
    }
    return out;
}

// ---------------------------------------------------------------- Percent / QP

bool isUnreserved(std::uint8_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

std::string percentEncodeImpl(ByteSpan data) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    out.reserve(data.size());
    for (std::uint8_t byte : data) {
        if (isUnreserved(byte)) {
            out.push_back(static_cast<char>(byte));
        } else {
            out.push_back('%');
            out.push_back(digits[byte >> 4]);
            out.push_back(digits[byte & 0x0F]);
        }
    }
    return out;
}

Bytes percentDecodeImpl(std::string_view text) {
    Bytes out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '%') {
            if (i + 2 >= text.size()) throw Error(codecError("url", Msg::CodecIncompleteSequence));
            const int high = hexValue(text[i + 1]);
            const int low  = hexValue(text[i + 2]);
            if (high < 0 || low < 0) throw Error(codecError("url", Msg::CodecInvalidCharacter));
            out.push_back(static_cast<std::uint8_t>((high << 4) | low));
            i += 2;
        } else {
            out.push_back(static_cast<std::uint8_t>(c));
        }
    }
    return out;
}

std::string quotedPrintableEncodeImpl(ByteSpan data) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    out.reserve(data.size());
    for (std::uint8_t byte : data) {
        if (byte >= 33 && byte <= 126 && byte != '=') {
            out.push_back(static_cast<char>(byte));
        } else {
            out.push_back('=');
            out.push_back(digits[byte >> 4]);
            out.push_back(digits[byte & 0x0F]);
        }
    }
    return out;
}

Bytes quotedPrintableDecodeImpl(std::string_view text) {
    Bytes out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c != '=') {
            out.push_back(static_cast<std::uint8_t>(c));
            continue;
        }
        if (i + 1 < text.size() && text[i + 1] == '\r') { // salto blando tolerado
            ++i;
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            continue;
        }
        if (i + 1 < text.size() && text[i + 1] == '\n') {
            ++i;
            continue;
        }
        if (i + 2 >= text.size()) throw Error(codecError("quoted-printable", Msg::CodecIncompleteSequence));
        const int high = hexValue(text[i + 1]);
        const int low  = hexValue(text[i + 2]);
        if (high < 0 || low < 0) throw Error(codecError("quoted-printable", Msg::CodecInvalidCharacter));
        out.push_back(static_cast<std::uint8_t>((high << 4) | low));
        i += 2;
    }
    return out;
}

// ---------------------------------------------------------------- Numéricas

std::string binaryEncodeImpl(ByteSpan data) {
    std::string out;
    out.reserve(data.size() * 9);
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i > 0) out.push_back(' ');
        for (int bit = 7; bit >= 0; --bit) {
            out.push_back(((data[i] >> bit) & 1u) ? '1' : '0');
        }
    }
    return out;
}

Bytes binaryDecodeImpl(std::string_view text) {
    std::string compact;
    compact.reserve(text.size());
    for (char c : text) {
        if (!isAsciiSpace(static_cast<unsigned char>(c))) compact.push_back(c);
    }
    if (compact.size() % 8 != 0) throw Error(codecError("binary", Msg::CodecInvalidLength));

    Bytes out;
    out.reserve(compact.size() / 8);
    for (std::size_t i = 0; i < compact.size(); i += 8) {
        std::uint8_t value = 0;
        for (int bit = 0; bit < 8; ++bit) {
            const char c = compact[i + static_cast<std::size_t>(bit)];
            if (c != '0' && c != '1') throw Error(codecError("binary", Msg::CodecInvalidCharacter));
            value = static_cast<std::uint8_t>((value << 1) | (c == '1' ? 1u : 0u));
        }
        out.push_back(value);
    }
    return out;
}

unsigned parseToken(std::string_view token, unsigned base, const char* name) {
    if (token.empty() || token.size() > 8) throw Error(codecError(name, Msg::CodecInvalidValue));
    unsigned value = 0;
    for (char c : token) {
        int digit = -1;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        if (digit < 0 || static_cast<unsigned>(digit) >= base) {
            throw Error(codecError(name, Msg::CodecInvalidDigit));
        }
        value = value * base + static_cast<unsigned>(digit);
        if (value > 255u) throw Error(codecError(name, Msg::CodecInvalidValue));
    }
    return value;
}

std::vector<std::string_view> splitTokens(std::string_view text) {
    std::vector<std::string_view> tokens;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && isAsciiSpace(static_cast<unsigned char>(text[i]))) ++i;
        const std::size_t begin = i;
        while (i < text.size() && !isAsciiSpace(static_cast<unsigned char>(text[i]))) ++i;
        if (i > begin) tokens.push_back(text.substr(begin, i - begin));
    }
    return tokens;
}

std::string octalEncodeImpl(ByteSpan data) {
    std::string out;
    out.reserve(data.size() * 4);
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i > 0) out.push_back(' ');
        const unsigned value = data[i];
        out.push_back(static_cast<char>('0' + ((value >> 6) & 7u)));
        out.push_back(static_cast<char>('0' + ((value >> 3) & 7u)));
        out.push_back(static_cast<char>('0' + (value & 7u)));
    }
    return out;
}

Bytes octalDecodeImpl(std::string_view text) {
    Bytes out;
    for (std::string_view token : splitTokens(text)) {
        out.push_back(static_cast<std::uint8_t>(parseToken(token, 8, "octal")));
    }
    return out;
}

std::string decimalEncodeImpl(ByteSpan data) {
    std::string out;
    out.reserve(data.size() * 4);
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i > 0) out.push_back(' ');
        const unsigned value = data[i];
        if (value >= 100) out.push_back(static_cast<char>('0' + value / 100));
        if (value >= 10) out.push_back(static_cast<char>('0' + (value / 10) % 10));
        out.push_back(static_cast<char>('0' + value % 10));
    }
    return out;
}

Bytes decimalDecodeImpl(std::string_view text) {
    Bytes out;
    for (std::string_view token : splitTokens(text)) {
        out.push_back(static_cast<std::uint8_t>(parseToken(token, 10, "decimal")));
    }
    return out;
}

// ---------------------------------------------------------------- Unicode / HTML

std::string unicodeEscapeEncodeImpl(ByteSpan data) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    out.reserve(data.size() * 6);
    for (std::uint8_t byte : data) {
        out += "\\u00";
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0F]);
    }
    return out;
}

Bytes unicodeEscapeDecodeImpl(std::string_view text) {
    Bytes out;
    out.reserve(text.size() / 6);
    std::size_t i = 0;
    while (i < text.size()) {
        if (isAsciiSpace(static_cast<unsigned char>(text[i]))) {
            ++i;
            continue;
        }
        if (text[i] != '\\' || i + 1 >= text.size() ||
            (text[i + 1] != 'u' && text[i + 1] != 'U')) {
            throw Error(codecError("unicode", Msg::CodecIncompleteSequence));
        }
        if (i + 6 > text.size()) throw Error(codecError("unicode", Msg::CodecIncompleteSequence));
        int value = 0;
        for (int k = 2; k < 6; ++k) {
            const int digit = hexValue(text[i + static_cast<std::size_t>(k)]);
            if (digit < 0) throw Error(codecError("unicode", Msg::CodecInvalidDigit));
            value = (value << 4) | digit;
        }
        if (value > 0xFF) throw Error(codecError("unicode", Msg::CodecInvalidValue));
        out.push_back(static_cast<std::uint8_t>(value));
        i += 6;
    }
    return out;
}

std::string htmlEntityEncodeImpl(ByteSpan data) {
    std::string out;
    out.reserve(data.size() * 5);
    for (std::uint8_t byte : data) {
        out += "&#";
        const unsigned value = byte;
        if (value >= 100) out.push_back(static_cast<char>('0' + value / 100));
        if (value >= 10) out.push_back(static_cast<char>('0' + (value / 10) % 10));
        out.push_back(static_cast<char>('0' + value % 10));
        out.push_back(';');
    }
    return out;
}

Bytes htmlEntityDecodeImpl(std::string_view text) {
    Bytes out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (isAsciiSpace(static_cast<unsigned char>(text[i]))) {
            ++i;
            continue;
        }
        if (text[i] != '&' || i + 2 >= text.size() || text[i + 1] != '#') {
            throw Error(codecError("html", Msg::CodecIncompleteSequence));
        }
        i += 2;
        unsigned base = 10;
        if (i < text.size() && (text[i] == 'x' || text[i] == 'X')) {
            base = 16;
            ++i;
        }
        const std::size_t begin = i;
        while (i < text.size() && ((text[i] >= '0' && text[i] <= '9') ||
                                   (base == 16 && ((text[i] >= 'a' && text[i] <= 'f') ||
                                                   (text[i] >= 'A' && text[i] <= 'F'))))) {
            ++i;
        }
        if (i == begin || i >= text.size() || text[i] != ';') {
            throw Error(codecError("html", Msg::CodecIncompleteSequence));
        }
        const unsigned value = parseToken(text.substr(begin, i - begin), base, "html");
        out.push_back(static_cast<std::uint8_t>(value));
        ++i; // ';'
    }
    return out;
}

// ---------------------------------------------------------------- Morse

struct MorseEntry {
    char character;
    const char* code;
};

constexpr MorseEntry kMorseTable[] = {
    {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},   {'E', "."},
    {'F', "..-."},  {'G', "--."},   {'H', "...."},  {'I', ".."},    {'J', ".---"},
    {'K', "-.-"},   {'L', ".-.."},  {'M', "--"},    {'N', "-."},    {'O', "---"},
    {'P', ".--."},  {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
    {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},  {'Y', "-.--"},
    {'Z', "--.."},
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"},
    {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
    {'.', ".-.-.-"}, {',', "--..--"}, {'?', "..--.."}, {'\'', ".----."}, {'!', "-.-.--"},
    {'/', "-..-."},  {'(', "-.--."},  {')', "-.--.-"}, {'&', ".-..."},  {':', "---..."},
    {';', "-.-.-."}, {'=', "-...-"},  {'+', ".-.-."},  {'-', "-....-"}, {'_', "..--.-"},
    {'"', ".-..-."}, {'$', "...-..-"}, {'@', ".--.-."},
};

const char* morseCode(char character) {
    for (const MorseEntry& entry : kMorseTable) {
        if (entry.character == character) return entry.code;
    }
    return nullptr;
}

bool morseRepresentable(std::uint8_t byte) {
    if (byte == ' ') return true;
    if (byte >= 'a' && byte <= 'z') {
        return morseCode(static_cast<char>(byte - 'a' + 'A')) != nullptr;
    }
    return morseCode(static_cast<char>(byte)) != nullptr;
}

std::string morseEncodeImpl(ByteSpan data) {
    std::string out;
    out.reserve(data.size() * 6);
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (i > 0) out.push_back(' ');
        const std::uint8_t byte = data[i];
        if (byte == ' ') {
            out.push_back('/');
        } else if (byte >= 'a' && byte <= 'z') {
            out.push_back('*'); // marca de minúscula (extensión Kruptos)
            out += morseCode(static_cast<char>(byte - 'a' + 'A'));
        } else {
            const char* code = morseCode(static_cast<char>(byte));
            if (!code) throw Error(codecError("morse", Msg::CodecNotRepresentable));
            out += code;
        }
    }
    return out;
}

Bytes morseDecodeImpl(std::string_view text) {
    Bytes out;
    for (std::string_view token : splitTokens(text)) {
        if (token == "/") {
            out.push_back(' ');
            continue;
        }
        bool lowercase = false;
        if (!token.empty() && token.front() == '*') {
            lowercase = true;
            token.remove_prefix(1);
        }
        if (token.empty()) throw Error(codecError("morse", Msg::CodecEmptySymbol));

        bool found = false;
        for (const MorseEntry& entry : kMorseTable) {
            if (token == entry.code) {
                char character = entry.character;
                if (lowercase) {
                    if (character < 'A' || character > 'Z') {
                        throw Error(codecError("morse", Msg::CodecMorseCase));
                    }
                    character = static_cast<char>(character - 'A' + 'a');
                }
                out.push_back(static_cast<std::uint8_t>(character));
                found = true;
                break;
            }
        }
        if (!found) throw Error(codecError("morse", Msg::CodecUnknownSymbol));
    }
    return out;
}

} // namespace

std::string base64Encode(ByteSpan data) {
    return base64EncodeImpl(data, kBase64Alphabet, true);
}

Bytes base64Decode(std::string_view text) {
    return base64DecodeImpl(text, kBase64Alphabet);
}

std::string base64UrlEncode(ByteSpan data) {
    return base64EncodeImpl(data, kBase64UrlAlphabet, false);
}

Bytes base64UrlDecode(std::string_view text) {
    return base64DecodeImpl(text, kBase64UrlAlphabet);
}

std::string base32Encode(ByteSpan data) { return base32EncodeImpl(data); }
Bytes base32Decode(std::string_view text) { return base32DecodeImpl(text); }

std::string base16Encode(ByteSpan data) { return toHex(data); }
Bytes base16Decode(std::string_view text) { return fromHex(text); }

std::string base58Encode(ByteSpan data) { return base58EncodeImpl(data); }
Bytes base58Decode(std::string_view text) { return base58DecodeImpl(text); }

std::string base85Encode(ByteSpan data) { return base85EncodeImpl(data); }
Bytes base85Decode(std::string_view text) { return base85DecodeImpl(text); }

std::string percentEncode(ByteSpan data) { return percentEncodeImpl(data); }
Bytes percentDecode(std::string_view text) { return percentDecodeImpl(text); }

std::string quotedPrintableEncode(ByteSpan data) { return quotedPrintableEncodeImpl(data); }
Bytes quotedPrintableDecode(std::string_view text) { return quotedPrintableDecodeImpl(text); }

std::string binaryEncode(ByteSpan data) { return binaryEncodeImpl(data); }
Bytes binaryDecode(std::string_view text) { return binaryDecodeImpl(text); }

std::string octalEncode(ByteSpan data) { return octalEncodeImpl(data); }
Bytes octalDecode(std::string_view text) { return octalDecodeImpl(text); }

std::string decimalEncode(ByteSpan data) { return decimalEncodeImpl(data); }
Bytes decimalDecode(std::string_view text) { return decimalDecodeImpl(text); }

std::string unicodeEscapeEncode(ByteSpan data) { return unicodeEscapeEncodeImpl(data); }
Bytes unicodeEscapeDecode(std::string_view text) { return unicodeEscapeDecodeImpl(text); }

std::string htmlEntityEncode(ByteSpan data) { return htmlEntityEncodeImpl(data); }
Bytes htmlEntityDecode(std::string_view text) { return htmlEntityDecodeImpl(text); }

std::string morseEncode(ByteSpan data) { return morseEncodeImpl(data); }
Bytes morseDecode(std::string_view text) { return morseDecodeImpl(text); }

bool morseApplicable(ByteSpan data) {
    for (std::uint8_t byte : data) {
        if (!morseRepresentable(byte)) return false;
    }
    return true;
}

} // namespace kruptos::enc
