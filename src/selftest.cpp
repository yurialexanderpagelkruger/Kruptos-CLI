#include "selftest.h"

#include "common.h"
#include "crypto.h"
#include "encoding.h"
#include "i18n.h"
#include "methods.h"
#include "pipeline.h"
#include "rng.h"
#include "token.h"

#include <cctype>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace kruptos {
namespace {

class Harness {
public:
    explicit Harness(bool verbose) : verbose_(verbose) {}

    void section(const std::string& name) {
        std::cout << "[*] " << name << "\n";
    }

    void check(bool condition, const std::string& description) {
        ++total_;
        if (condition) {
            ++passed_;
            if (verbose_) std::cout << "    OK    " << description << "\n";
        } else {
            ++failed_;
            std::cout << "    " << tr(Msg::SelfTestFail) << " " << description << "\n";
        }
    }

    int summary() const {
        std::cout << "\n";
        if (failed_ == 0) {
            std::cout << "[+] "
                      << tr(Msg::SelfTestPassed,
                            {std::to_string(passed_), std::to_string(total_)})
                      << "\n";
        } else {
            std::cout << "[-] "
                      << tr(Msg::SelfTestFailed,
                            {std::to_string(failed_), std::to_string(total_)})
                      << "\n";
        }
        return failed_ == 0 ? 0 : 1;
    }

private:
    bool verbose_;
    int total_  = 0;
    int passed_ = 0;
    int failed_ = 0;
};

bool hexEquals(ByteSpan data, const char* expected) {
    return toHex(data) == expected;
}

void planSanity(Harness& harness);

std::string methodLabel(const Method& method) {
    std::string label = method.name;
    if (!label.empty()) {
        label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
    }
    return label;
}

Bytes randomBytes(std::size_t count) {
    return secureRandom(count);
}

std::string randomText(std::size_t length) {
    static const char* kAlphabet =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        " .,;:!?()[]{}-_/\\\"'@#$%&*+=<>áéíóúñÑüÜ¿¡€";
    const std::size_t alphabetLength = std::char_traits<char>::length(kAlphabet);
    std::string text;
    text.reserve(length);
    const Bytes noise = randomBytes(length);
    for (std::size_t i = 0; i < length; ++i) {
        text.push_back(kAlphabet[noise[i] % alphabetLength]);
    }
    return text;
}

Bytes randomMorseText(std::size_t length) {
    static const char* kAlphabet =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .,?!/()&:;=+-_\"$@";
    const std::size_t alphabetLength = std::char_traits<char>::length(kAlphabet);
    Bytes text;
    text.reserve(length);
    const Bytes noise = randomBytes(length);
    for (std::size_t i = 0; i < length; ++i) {
        text.push_back(static_cast<std::uint8_t>(kAlphabet[noise[i] % alphabetLength]));
    }
    return text;
}

void testCryptoVectors(Harness& harness) {
    harness.section(tr(Msg::SelfTestCrypto));

    harness.check(hexEquals(crypto::sha256(toBytes("")),
                            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
                  "SHA-256 of the empty string");
    harness.check(hexEquals(crypto::sha256(toBytes("abc")),
                            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
                  "SHA-256 of \"abc\"");
    harness.check(hexEquals(crypto::sha256(toBytes(
                                "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
                            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"),
                  "SHA-256 of a 56-byte message");

    harness.check(hexEquals(
                      crypto::hmacSha256(toBytes("Jefe"), toBytes("what do ya want for nothing?")),
                      "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"),
                  "HMAC-SHA-256 (RFC 4231, case 2)");

    harness.check(hexEquals(
                      crypto::pbkdf2Sha256("password", toBytes("salt"), 1, 32),
                      "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"),
                  "PBKDF2-HMAC-SHA-256 (1 iteration)");

    const Bytes key = fromHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
    const Bytes nonce = fromHex("000000090000004a00000000");
    Bytes zeros(64, 0x00);
    Bytes stream;
    crypto::chacha20Xor(key, nonce, 1, zeros, stream);
    harness.check(toHex(ByteSpan(stream.data(), 16)) == "10f1e7e4d13b5915500fdd1fa32071c4",
                  "ChaCha20 (RFC 8439, block vector)");

    const Bytes a = fromHex("aabbccdd");
    const Bytes b = fromHex("aabbccdd");
    const Bytes c = fromHex("aabbccde");
    harness.check(crypto::constantTimeEqual(a, b) && !crypto::constantTimeEqual(a, c),
                  "Constant-time comparison");
}

void testEncodings(Harness& harness) {
    harness.section(tr(Msg::SelfTestEncodings));

    const Bytes seed(32, 0x5A);

    for (const Method& method : methods()) {
        if (method.kind != MethodKind::Encoding) continue;

        for (int trial = 0; trial < 6; ++trial) {
            const std::size_t length = static_cast<std::size_t>(trial) * 9 + 1;

            Bytes data;
            if (method.name == std::string("morse")) {
                data = randomMorseText(length);
            } else if (trial % 3 == 0) {
                data = randomBytes(length);
            } else {
                data = toBytes(randomText(length));
            }

            if (!method.applies(data)) continue;

            KeyStream forwardKey(seed, static_cast<std::uint32_t>(trial), method.id);
            Bytes encoded;
            method.forward(data, encoded, forwardKey);

            harness.check(isPrintableAscii(encoded),
                          methodLabel(method) + ": Printable ASCII output");

            KeyStream inverseKey(seed, static_cast<std::uint32_t>(trial), method.id);
            Bytes decoded;
            method.inverse(encoded, decoded, inverseKey);

            harness.check(decoded == data,
                          methodLabel(method) + ": Round-trip, length " +
                              std::to_string(length));
        }

        const Bytes empty;
        KeyStream forwardKey(seed, 99, method.id);
        Bytes encoded;
        method.forward(empty, encoded, forwardKey);
        KeyStream inverseKey(seed, 99, method.id);
        Bytes decoded;
        method.inverse(encoded, decoded, inverseKey);
        harness.check(decoded.empty(), methodLabel(method) + ": Empty input");
    }

    // Errores de formato esperados.
    bool threw = false;
    try { enc::base64Decode("A"); } catch (const Error&) { threw = true; }
    harness.check(threw, "Base64 rejects an invalid length");

    threw = false;
    try { enc::base16Decode("abc"); } catch (const Error&) { threw = true; }
    harness.check(threw, "Base16 rejects an odd length");

    threw = false;
    try { enc::binaryDecode("0101"); } catch (const Error&) { threw = true; }
    harness.check(threw, "Binary rejects lengths that are not multiples of 8");

    threw = false;
    try { enc::quotedPrintableDecode("=ZZ"); } catch (const Error&) { threw = true; }
    harness.check(threw, "Quoted-printable rejects invalid sequences");
}

void testCiphers(Harness& harness) {
    harness.section(tr(Msg::SelfTestCiphers));

    const Bytes fixedSeed(32, 0x3C);

    for (const Method& method : methods()) {
        if (method.kind != MethodKind::Cipher) continue;

        for (int trial = 0; trial < 8; ++trial) {
            const std::size_t length = static_cast<std::size_t>(trial) * 13 + 2;
            const Bytes data = randomBytes(length);
            if (!method.applies(data)) continue;

            KeyStream forwardKey(fixedSeed, static_cast<std::uint32_t>(trial), method.id);
            Bytes encrypted;
            method.forward(data, encrypted, forwardKey);
            harness.check(encrypted.size() == data.size(),
                          methodLabel(method) + ": Preserves size");

            KeyStream inverseKey(fixedSeed, static_cast<std::uint32_t>(trial), method.id);
            Bytes decrypted;
            method.inverse(encrypted, decrypted, inverseKey);
            harness.check(decrypted == data,
                          methodLabel(method) + ": Round-trip, length " +
                              std::to_string(length));
        }
    }
}

void testToken(Harness& harness) {
    harness.section(tr(Msg::SelfTestTokens));

    Plan plan;
    plan.seed = randomBytes(32);
    plan.methods = {1, 32, 9};

    const std::uint32_t hash = 0xDEADBEEFu;
    const std::string id = token::encode(plan, hash, "");
    harness.check(startsWith(id, "KR1-"), "ID starts with KR1-");

    const token::Header header = token::peek(id);
    harness.check(header.version == 1 && !header.passwordProtected,
                  "ID header without a password");

    const token::Decoded decoded = token::decode(id, "");
    harness.check(decoded.plan.methods == plan.methods, "ID keeps the layer order");
    harness.check(decoded.plan.seed == plan.seed, "ID keeps the seed");
    harness.check(decoded.cipherHash == hash, "ID keeps the ciphertext fingerprint");

    std::string compact;
    for (char c : id) {
        if (c != '-') {
            compact.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
    }
    const token::Decoded decodedCompact = token::decode(compact, "");
    harness.check(decodedCompact.plan.methods == plan.methods,
                  "ID is accepted in lowercase and without dashes");

    std::string damaged = id;
    damaged[damaged.size() - 5] = (damaged[damaged.size() - 5] == 'A') ? 'B' : 'A';
    bool threw = false;
    try { (void)token::decode(damaged, ""); } catch (const Error&) { threw = true; }
    harness.check(threw, "An altered ID is rejected");

    threw = false;
    try { (void)token::decode("KR1-AAAA", ""); } catch (const Error&) { threw = true; }
    harness.check(threw, "A truncated ID is rejected");

    const std::string protectedId = token::encode(plan, hash, "test-password");
    harness.check(token::peek(protectedId).passwordProtected,
                  "A protected ID announces a password");

    const token::Decoded opened = token::decode(protectedId, "test-password");
    harness.check(opened.plan.methods == plan.methods && opened.plan.seed == plan.seed,
                  "A protected ID opens with the correct password");

    threw = false;
    try { (void)token::decode(protectedId, "other-password"); } catch (const Error&) { threw = true; }
    harness.check(threw, "A protected ID rejects a wrong password");

    threw = false;
    try { (void)token::decode(protectedId, ""); } catch (const Error&) { threw = true; }
    harness.check(threw, "A protected ID requires a password");
}

void testPipeline(Harness& harness) {
    harness.section(tr(Msg::SelfTestEngine));

    // Todas las capas deben ser alcanzables con un mensaje normal.
    PlanOptions forced;
    forced.layers = 10;
    const Encryption forcedEncryption = encryptText(toBytes("layer test"), forced);
    harness.check(forcedEncryption.plan.methods.size() == 10,
                  "Exactly 10 layers can be forced");

    for (int iteration = 0; iteration < 25; ++iteration) {
        std::string text;
        if (iteration % 5 == 4) {
            const Bytes raw = randomBytes(1 + static_cast<std::size_t>(iteration) * 3);
            text.assign(reinterpret_cast<const char*>(raw.data()), raw.size());
        } else {
            text = randomText(1 + static_cast<std::size_t>(iteration) * 5);
        }

        const Encryption encryption = encryptText(toBytes(text), PlanOptions{});
        harness.check(!encryption.ciphertext.empty(),
                      "Ciphertext is not empty (iteration " + std::to_string(iteration) + ")");
        harness.check(isPrintableAsciiNoSpace(toBytes(encryption.ciphertext)),
                      "Ciphertext is printable ASCII without spaces (iteration " +
                          std::to_string(iteration) + ")");
        harness.check(encryption.plan.methods.size() >= 7 &&
                          encryption.plan.methods.size() <= 12,
                      "Layer count is within range (iteration " +
                          std::to_string(iteration) + ")");

        const Bytes recovered = revertPlan(encryption.ciphertext, encryption.plan);
        harness.check(recovered == toBytes(text),
                      "Full round-trip (iteration " + std::to_string(iteration) + ")");

        harness.check(applyPlan(recovered, encryption.plan) == encryption.ciphertext,
                      "The plan is deterministic (iteration " +
                          std::to_string(iteration) + ")");

        const std::uint32_t hash = cipherFingerprint(encryption.ciphertext);
        const std::string id = token::encode(encryption.plan, hash, "");
        const token::Decoded decoded = token::decode(id, "");
        harness.check(cipherFingerprint(applyPlan(revertPlan(encryption.ciphertext, decoded.plan),
                                                  decoded.plan)) == decoded.cipherHash,
                      "Integrity verification (iteration " +
                          std::to_string(iteration) + ")");

        std::string broken = encryption.ciphertext;
        const std::size_t mutation = broken.size() / 2;
        broken[mutation] = (broken[mutation] == '~') ? '!' : '~';
        bool detected = false;
        try {
            const Bytes wrong = revertPlan(broken, encryption.plan);
            const std::string reencrypted = applyPlan(wrong, encryption.plan);
            detected = cipherFingerprint(reencrypted) != hash;
            if (!detected) {
                std::cout << "      [Diagnostic] plan:";
                for (std::uint8_t methodId : encryption.plan.methods) {
                    std::cout << " " << findMethod(methodId)->name;
                }
                std::cout << "\n      Original : " << encryption.ciphertext.substr(0, 48)
                          << " (len " << encryption.ciphertext.size() << ")\n"
                          << "      Altered  : " << broken.substr(0, 48) << "\n"
                          << "      Reenc    : " << reencrypted.substr(0, 48) << "\n"
                          << "      Plain    : " << text.substr(0, 48) << "\n";
            }
        } catch (const Error&) {
            detected = true;
        }
        harness.check(detected, "Ciphertext alteration is detected (iteration " +
                                std::to_string(iteration) + ")");
    }

    bool threw = false;
    try { (void)encryptText(ByteSpan{}, PlanOptions{}); } catch (const Error&) { threw = true; }
    harness.check(threw, "Empty message is rejected");

    threw = false;
    try {
        PlanOptions zeroLayers;
        zeroLayers.layers = 0;
        (void)encryptText(toBytes("x"), zeroLayers);
    } catch (const Error&) {
        threw = true;
    }
    harness.check(threw, "Zero layers is rejected");

    planSanity(harness);
}

void planSanity(Harness& harness) {
    harness.section(tr(Msg::SelfTestRepertoire));

    harness.check(methods().size() >= 20, "At least 20 methods are available");

    std::vector<std::uint8_t> ids;
    std::vector<std::string> names;
    bool uniqueIds = true;
    bool uniqueNames = true;
    for (const Method& method : methods()) {
        for (std::uint8_t id : ids) {
            if (id == method.id) uniqueIds = false;
        }
        for (const std::string& name : names) {
            if (name == method.name) uniqueNames = false;
        }
        ids.push_back(method.id);
        names.emplace_back(method.name);
        if (findMethod(method.id) != &method) uniqueIds = false;
    }
    harness.check(uniqueIds, "Method IDs are unique and resolvable");
    harness.check(uniqueNames, "Method names are unique");
    harness.check(findMethod(250) == nullptr, "An unknown method does not resolve");
}

} // namespace
} // namespace kruptos

namespace kruptos {

int runSelfTest(bool verbose) {
    std::cout << tr(Msg::SelfTestTitle, {kVersion}) << "\n\n";

    Harness harness(verbose);
    testCryptoVectors(harness);
    testEncodings(harness);
    testCiphers(harness);
    testToken(harness);
    testPipeline(harness);

    return harness.summary();
}

} // namespace kruptos
