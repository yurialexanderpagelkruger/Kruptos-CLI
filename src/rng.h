// Kruptos - generador aleatorio seguro del sistema y derivación de claves por paso.
#pragma once

#include "common.h"

#include <cstdint>

namespace kruptos {

// Bytes aleatorios del CSPRNG del sistema operativo (BCryptGenRandom / getrandom).
Bytes secureRandom(std::size_t count);
std::uint32_t secureRandomU32();

// Flujo determinista de material clave para un paso concreto del plan.
// Se deriva con HMAC-SHA-256(semilla, etiqueta || paso || metodo || contador),
// de modo que cifrar y descifrar obtienen exactamente los mismos parámetros
// sin necesidad de almacenarlos en el ID.
class KeyStream {
public:
    KeyStream(ByteSpan seed, std::uint32_t step, std::uint8_t method);

    Bytes take(std::size_t count);
    std::uint8_t u8();
    std::uint8_t rangeInclusive(std::uint8_t low, std::uint8_t high);

private:
    void refill();

    Bytes seed_;
    std::uint32_t step_;
    std::uint8_t method_;
    std::uint32_t counter_ = 0;
    Bytes buffer_;
    std::size_t position_ = 0;
};

} // namespace kruptos
