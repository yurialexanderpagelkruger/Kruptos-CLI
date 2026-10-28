#include "pipeline.h"

#include "crypto.h"
#include "i18n.h"
#include "rng.h"

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace kruptos {
namespace {

struct SystemUrbg {
    using result_type = std::uint32_t;
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return 0xFFFFFFFFu; }
    result_type operator()() const { return secureRandomU32(); }
};

std::vector<const Method*> candidatesFor(ByteSpan buffer, bool lastStep,
                                         const std::vector<bool>& used) {
    std::vector<const Method*> candidates;
    for (const Method& method : methods()) {
        if (used[method.id]) continue;
        if (!method.applies(buffer)) continue;
        if (lastStep && !method.finalSafe) continue;
        candidates.push_back(&method);
    }
    std::shuffle(candidates.begin(), candidates.end(), SystemUrbg{});
    return candidates;
}

} // namespace

void validatePlan(const Plan& plan) {
    if (plan.seed.size() != 32) {
        throw Error(tr(Msg::ErrPlanInvalidSeed));
    }
    if (plan.methods.empty() || plan.methods.size() > 64) {
        throw Error(tr(Msg::ErrPlanInvalidCount));
    }
    for (std::uint8_t id : plan.methods) {
        if (findMethod(id) == nullptr) {
            throw Error(tr(Msg::ErrPlanUnknownMethod, {std::to_string(id)}));
        }
    }
}

std::string applyPlan(ByteSpan plaintext, const Plan& plan) {
    validatePlan(plan);

    Bytes buffer(plaintext.begin(), plaintext.end());
    for (std::size_t step = 0; step < plan.methods.size(); ++step) {
        const Method* method = findMethod(plan.methods[step]);
        KeyStream key(plan.seed, static_cast<std::uint32_t>(step), method->id);

        Bytes output;
        method->forward(buffer, output, key);
        buffer.swap(output);

        if (buffer.size() > kMaxCiphertextBytes) {
            throw Error(tr(Msg::ErrLayerTooLarge, {std::to_string(step + 1)}));
        }
    }

    if (!isPrintableAscii(buffer)) {
        throw Error(tr(Msg::ErrInternalNotPrintable));
    }
    return toString(buffer);
}

Bytes revertPlan(std::string_view ciphertext, const Plan& plan) {
    validatePlan(plan);

    const std::string compact = stripWhitespace(ciphertext);
    if (compact.empty()) throw Error(tr(Msg::ErrEmptyCiphertext));
    if (compact.size() > kMaxCiphertextBytes) {
        throw Error(tr(Msg::ErrCiphertextTooLarge));
    }

    Bytes buffer = toBytes(compact);
    for (std::size_t step = plan.methods.size(); step-- > 0;) {
        const Method* method = findMethod(plan.methods[step]);
        KeyStream key(plan.seed, static_cast<std::uint32_t>(step), method->id);

        Bytes output;
        try {
            method->inverse(buffer, output, key);
        } catch (const Error& error) {
            throw Error(tr(Msg::ErrLayerRevert, {std::to_string(step + 1),
                                                 method->name, error.what()}));
        }
        buffer.swap(output);
    }
    return buffer;
}

Encryption encryptText(ByteSpan plaintext, const PlanOptions& options) {
    if (plaintext.empty()) throw Error(tr(Msg::ErrEmptyMessage));
    if (plaintext.size() > kMaxPlaintextBytes) {
        throw Error(tr(Msg::ErrMessageTooLong, {std::to_string(kMaxPlaintextBytes)}));
    }

    const std::size_t available = std::min<std::size_t>(methods().size(), kMaxLayers);

    std::size_t minimum = std::max<std::size_t>(1, options.minLayers);
    std::size_t maximum = std::max<std::size_t>(1, options.maxLayers);
    if (minimum > maximum) std::swap(minimum, maximum);
    minimum = std::min(minimum, available);
    maximum = std::min(maximum, available);

    std::size_t target = 0;
    if (options.layers.has_value()) {
        target = *options.layers;
        if (target == 0) throw Error(tr(Msg::ErrLayersZero));
        target = std::min(target, available);
    } else {
        target = minimum + static_cast<std::size_t>(
                       secureRandomU32() % static_cast<std::uint32_t>(maximum - minimum + 1));
    }

    // Presupuesto de expansión: evita que una cadena de codificaciones convierta
    // un mensaje corto en cientos de kilobytes.
    const bool forced = options.layers.has_value();
    const std::size_t stepBudget = std::min<std::size_t>(
        kMaxIntermediateBytes,
        std::max<std::size_t>(4096, plaintext.size() * 32));
    const std::size_t finalBudget = std::min<std::size_t>(
        kMaxCiphertextBytes,
        std::max<std::size_t>(16u * 1024u, stepBudget * 2));

    std::size_t bestPartial = 0;
    const int attempts = forced ? 5 : 1;

    for (int attempt = 0; attempt < attempts; ++attempt) {
        Plan plan;
        plan.seed = secureRandom(32);
        std::vector<bool> used(256, false);
        Bytes buffer(plaintext.begin(), plaintext.end());
        bool complete = true;

        for (std::size_t step = 0; step < target; ++step) {
            const bool lastStep = (step + 1 == target);
            const std::vector<const Method*> candidates = candidatesFor(buffer, lastStep, used);

            bool applied = false;
            for (const Method* method : candidates) {
                Bytes output;
                KeyStream key(plan.seed, static_cast<std::uint32_t>(step), method->id);
                try {
                    method->forward(buffer, output, key);
                } catch (const Error&) {
                    continue; // p. ej. morse con bytes no representables
                }
                if (lastStep && !isPrintableAsciiNoSpace(output)) continue;
                const std::size_t limit = lastStep ? finalBudget : stepBudget;
                if (output.size() > limit) continue;

                buffer.swap(output);
                plan.methods.push_back(method->id);
                used[method->id] = true;
                applied = true;
                break;
            }

            if (!applied) {
                bestPartial = std::max(bestPartial, plan.methods.size());
                complete = false;
                break;
            }
        }

        if (complete) {
            Encryption result;
            result.ciphertext = toString(buffer);
            result.plan      = std::move(plan);
            return result;
        }
        if (forced) continue; // se prueba otro orden aleatorio

        if (plan.methods.empty()) {
            throw Error(tr(Msg::ErrNoLayer));
        }

        // Plan aleatorio más corto: se completa con base64 para que el resultado
        // siempre sea ASCII sin espacios.
        if (!isPrintableAsciiNoSpace(buffer)) {
            const Method* fallback = findMethod(32);
            Bytes output;
            KeyStream key(plan.seed, static_cast<std::uint32_t>(plan.methods.size()),
                          fallback->id);
            fallback->forward(buffer, output, key);
            if (output.size() > kMaxCiphertextBytes) {
                throw Error(tr(Msg::ErrResultTooLarge));
            }
            buffer.swap(output);
            plan.methods.push_back(fallback->id);
        }

        Encryption result;
        result.ciphertext = toString(buffer);
        result.plan      = std::move(plan);
        return result;
    }

    throw Error(tr(Msg::ErrPlanTooShort, {std::to_string(bestPartial),
                                          std::to_string(target)}));
}

std::uint32_t cipherFingerprint(std::string_view ciphertext) {
    const Bytes digest = crypto::sha256(toBytes(ciphertext));
    return (static_cast<std::uint32_t>(digest[0]) << 24) |
           (static_cast<std::uint32_t>(digest[1]) << 16) |
           (static_cast<std::uint32_t>(digest[2]) << 8) |
           static_cast<std::uint32_t>(digest[3]);
}

std::string seedFingerprint(ByteSpan seed) {
    const Bytes digest = crypto::sha256(seed);
    Bytes prefix(digest.begin(), digest.begin() + 4);
    return toHex(prefix);
}

} // namespace kruptos
