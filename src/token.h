// Kruptos - identificador autodescriptivo del plan (KR1-...).
#pragma once

#include "common.h"
#include "pipeline.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace kruptos::token {

inline constexpr std::uint32_t kPbkdf2Iterations = 150000;

struct Header {
    std::uint8_t version = 1;
    bool passwordProtected = false;
};

struct Decoded {
    Plan plan;
    std::uint32_t cipherHash = 0;
    bool passwordProtected = false;
};

// Lee solo la cabecera (sin contraseña) para saber si hay que pedirla.
Header peek(std::string_view id);

// Construye el ID. Si passphrase no está vacía, el plan y la semilla se cifran
// con ChaCha20 y se autentican con HMAC-SHA-256.
std::string encode(const Plan& plan, std::uint32_t cipherHash, std::string_view passphrase);

Decoded decode(std::string_view id, std::string_view passphrase);

} // namespace kruptos::token
