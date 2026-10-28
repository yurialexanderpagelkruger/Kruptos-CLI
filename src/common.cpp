#include "common.h"

#include "i18n.h"

#include <cctype>

namespace kruptos {

Bytes toBytes(std::string_view text) {
    Bytes out(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(text[i]);
    }
    return out;
}

std::string toString(ByteSpan data) {
    std::string out(data.size(), '\0');
    for (std::size_t i = 0; i < data.size(); ++i) {
        out[i] = static_cast<char>(data[i]);
    }
    return out;
}

std::string toHex(ByteSpan data) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (std::uint8_t byte : data) {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 0x0F]);
    }
    return out;
}

Bytes fromHex(std::string_view hex) {
    auto value = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    Bytes out;
    out.reserve(hex.size() / 2);
    int high = -1;
    for (char c : hex) {
        if (isAsciiSpace(static_cast<unsigned char>(c))) continue;
        const int v = value(c);
        if (v < 0) throw Error(std::string("Base16: ") + tr(Msg::CodecInvalidCharacter));
        if (high < 0) {
            high = v;
        } else {
            out.push_back(static_cast<std::uint8_t>((high << 4) | v));
            high = -1;
        }
    }
    if (high >= 0) throw Error(std::string("Base16: ") + tr(Msg::CodecInvalidLength));
    return out;
}

bool isAsciiSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

bool isPrintableAscii(ByteSpan data) {
    for (std::uint8_t byte : data) {
        if (byte < 0x20 || byte > 0x7E) return false;
    }
    return true;
}

bool isPrintableAsciiNoSpace(ByteSpan data) {
    for (std::uint8_t byte : data) {
        if (byte <= 0x20 || byte > 0x7E) return false;
    }
    return true;
}

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end   = text.size();
    while (begin < end && isAsciiSpace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && isAsciiSpace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(begin, end - begin));
}

std::string stripWhitespace(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (!isAsciiSpace(static_cast<unsigned char>(c))) out.push_back(c);
    }
    return out;
}

std::string toLower(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string toUpper(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

} // namespace kruptos
