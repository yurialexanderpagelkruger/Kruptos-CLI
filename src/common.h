// Kruptos - tipos y utilidades comunes.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kruptos {

inline constexpr const char* kAppName  = "Kruptos";
inline constexpr const char* kVersion  = "1.0.0";
inline constexpr const char* kIdPrefix = "KR1";

using Bytes    = std::vector<std::uint8_t>;
using ByteSpan = std::span<const std::uint8_t>;

// Error controlado: se muestra al usuario sin traza ni volcado.
class Error : public std::runtime_error {
public:
    explicit Error(const std::string& message) : std::runtime_error(message) {}
};

Bytes toBytes(std::string_view text);
std::string toString(ByteSpan data);
std::string toHex(ByteSpan data);
Bytes fromHex(std::string_view hex);

bool isAsciiSpace(unsigned char c);
bool isPrintableAscii(ByteSpan data);
bool isPrintableAsciiNoSpace(ByteSpan data);

std::string trim(std::string_view text);
std::string stripWhitespace(std::string_view text);
std::string toLower(std::string_view text);
std::string toUpper(std::string_view text);

bool startsWith(std::string_view text, std::string_view prefix);
bool endsWith(std::string_view text, std::string_view suffix);

} // namespace kruptos
