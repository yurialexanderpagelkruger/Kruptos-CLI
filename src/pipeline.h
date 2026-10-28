// Kruptos - plan de capas aleatorio y ejecución reversible del mismo.
#pragma once

#include "common.h"
#include "methods.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace kruptos {

struct PlanOptions {
    // Si se indica, se exige ese número exacto de capas.
    std::optional<std::size_t> layers;
    std::size_t minLayers = 7;
    std::size_t maxLayers = 12;
};

struct Plan {
    std::vector<std::uint8_t> methods; // ids en orden de aplicación
    Bytes seed;                        // 32 bytes, semilla maestra del plan
};

struct Encryption {
    Plan plan;
    std::string ciphertext;
};

// Construye un plan aleatorio para el mensaje y lo aplica.
Encryption encryptText(ByteSpan plaintext, const PlanOptions& options);

// Aplica / revierte un plan concreto (usado también para verificar integridad).
std::string applyPlan(ByteSpan plaintext, const Plan& plan);
Bytes revertPlan(std::string_view ciphertext, const Plan& plan);

// Huella de 32 bits del texto cifrado canónico (sin espacios).
std::uint32_t cipherFingerprint(std::string_view ciphertext);

// Huella legible de la semilla: 8 caracteres hexadecimales de SHA-256(semilla).
std::string seedFingerprint(ByteSpan seed);

void validatePlan(const Plan& plan);

} // namespace kruptos
