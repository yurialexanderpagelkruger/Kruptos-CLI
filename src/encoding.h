// Kruptos - codificaciones reversibles. Todas trabajan sobre bytes y producen
// texto ASCII imprimible, de modo que el resultado se pueda copiar y pegar.
#pragma once

#include "common.h"

#include <string>
#include <string_view>

namespace kruptos::enc {

std::string base64Encode(ByteSpan data);
Bytes       base64Decode(std::string_view text);

std::string base64UrlEncode(ByteSpan data);
Bytes       base64UrlDecode(std::string_view text);

std::string base32Encode(ByteSpan data);
Bytes       base32Decode(std::string_view text);

std::string base16Encode(ByteSpan data);
Bytes       base16Decode(std::string_view text);

std::string base58Encode(ByteSpan data);
Bytes       base58Decode(std::string_view text);

std::string base85Encode(ByteSpan data);
Bytes       base85Decode(std::string_view text);

std::string percentEncode(ByteSpan data);
Bytes       percentDecode(std::string_view text);

std::string quotedPrintableEncode(ByteSpan data);
Bytes       quotedPrintableDecode(std::string_view text);

std::string binaryEncode(ByteSpan data);
Bytes       binaryDecode(std::string_view text);

std::string octalEncode(ByteSpan data);
Bytes       octalDecode(std::string_view text);

std::string decimalEncode(ByteSpan data);
Bytes       decimalDecode(std::string_view text);

std::string unicodeEscapeEncode(ByteSpan data);
Bytes       unicodeEscapeDecode(std::string_view text);

std::string htmlEntityEncode(ByteSpan data);
Bytes       htmlEntityDecode(std::string_view text);

std::string morseEncode(ByteSpan data);
Bytes       morseDecode(std::string_view text);
bool        morseApplicable(ByteSpan data);

} // namespace kruptos::enc
