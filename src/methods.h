// Kruptos - repertorio de métodos de cifrado y codificación.
#pragma once

#include "common.h"
#include "i18n.h"
#include "rng.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace kruptos {

enum class MethodKind : std::uint8_t {
    Cipher   = 0,
    Encoding = 1,
};

struct Method {
    std::uint8_t id;
    const char* name;
    Msg summary;
    MethodKind kind;
    bool finalSafe; // salida ASCII imprimible y sin espacios: válida como última capa
    bool (*applies)(ByteSpan);
    void (*forward)(ByteSpan input, Bytes& output, KeyStream& key);
    void (*inverse)(ByteSpan input, Bytes& output, KeyStream& key);
};

const std::vector<Method>& methods();
const Method* findMethod(std::uint8_t id);
const char* methodKindName(MethodKind kind);

// Límites defensivos para evitar que una cadena de codificaciones expansivas
// consuma memoria sin control.
inline constexpr std::size_t kMaxPlaintextBytes    = 128u * 1024u;
inline constexpr std::size_t kMaxIntermediateBytes = 4u * 1024u * 1024u;
inline constexpr std::size_t kMaxCiphertextBytes   = 16u * 1024u * 1024u;
inline constexpr std::size_t kMaxLayers            = 16;

} // namespace kruptos
