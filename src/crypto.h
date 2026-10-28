// Kruptos - primitivas criptográficas portables (sin dependencias externas).
#pragma once

#include "common.h"

#include <cstdint>
#include <string_view>

namespace kruptos::crypto {

// SHA-256 (FIPS 180-4).
Bytes sha256(ByteSpan data);

// HMAC-SHA-256 (RFC 2104).
Bytes hmacSha256(ByteSpan key, ByteSpan data);

// PBKDF2-HMAC-SHA-256 (RFC 8018).
Bytes pbkdf2Sha256(std::string_view password, ByteSpan salt,
                   std::uint32_t iterations, std::size_t outputLength);

// ChaCha20 (RFC 8439), cifrado por XOR en flujo. La misma llamada cifra y descifra.
// key debe tener 32 bytes y nonce 12 bytes.
void chacha20Xor(ByteSpan key, ByteSpan nonce, std::uint32_t counter,
                 ByteSpan input, Bytes& output);

// Comparación en tiempo constante (longitudes distintas => false).
bool constantTimeEqual(ByteSpan a, ByteSpan b);

} // namespace kruptos::crypto
