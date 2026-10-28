#include "cli.h"

#include "common.h"
#include "i18n.h"
#include "methods.h"
#include "pipeline.h"
#include "selftest.h"
#include "terminal.h"
#include "token.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace kruptos::cli {
namespace {

// ------------------------------------------------------------------ interfaz

constexpr const char* kReset  = "\x1b[0m";
constexpr const char* kCyan   = "\x1b[36m";
constexpr const char* kGreen  = "\x1b[32m";
constexpr const char* kYellow = "\x1b[33m";
constexpr const char* kRed    = "\x1b[31m";
constexpr const char* kDim    = "\x1b[2m";

const char* kArt =
    " _  __                 _\n"
    "| |/ /_ __ _   _ _ __ | |_ ___  ___\n"
    "| ' /| '__| | | | '_ \\| __/ _ \\/ __|\n"
    "| . \\| |  | |_| | |_) | || (_) \\__ \\\n"
    "|_|\\_\\_|   \\__,_| .__/  \\__\\___/|___/\n"
    "                |_|";

// Longitud visual UTF-8: cuenta caracteres, no bytes (los acentos ocupan dos).
std::size_t displayLength(std::string_view text) {
    std::size_t length = 0;
    for (unsigned char c : text) {
        if ((c & 0xC0) != 0x80) ++length;
    }
    return length;
}

std::string padRight(std::string_view text, std::size_t width) {
    std::string out(text);
    std::size_t length = displayLength(out);
    while (length < width) {
        out.push_back(' ');
        ++length;
    }
    return out;
}

class Ui {
public:
    Ui(bool color, bool quiet, bool json)
        : color_(color && !quiet && !json), quiet_(quiet), json_(json) {}

    bool quiet() const { return quiet_; }
    bool json() const { return json_; }

    std::string paint(const char* color, const std::string& text) const {
        if (!color_) return text;
        return std::string(color) + text + kReset;
    }

    void banner() const {
        if (quiet_ || json_) return;
        std::cout << "\n" << paint(kCyan, kArt) << "\n";
        std::cout << "  "
                  << paint(kDim, std::string(tr(Msg::AppTagline)) + "  ·  v" + kVersion)
                  << "\n\n";
    }

    void info(const std::string& text) const { tag("*", kCyan, text); }
    void ok(const std::string& text) const { tag("+", kGreen, text); }
    void warn(const std::string& text) const { tag("!", kYellow, text); }

    void plain(const std::string& text) const {
        if (quiet_ || json_) return;
        std::cout << text << "\n";
    }

    void field(const std::string& label, const std::string& value) const {
        if (quiet_ || json_) return;
        std::cout << "    " << paint(kDim, padRight(label + ":", 13)) << " " << value << "\n";
    }

    void print(const std::string& text) const { std::cout << text; }

private:
    void tag(const char* mark, const char* color, const std::string& text) const {
        if (quiet_ || json_) return;
        std::cout << paint(color, std::string("[") + mark + "]") << " " << text << "\n";
    }

    bool color_;
    bool quiet_;
    bool json_;
};

std::string platformName() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unix";
#endif
}

// ------------------------------------------------------------------ utilidades

bool isJsonRequested(const std::vector<std::string>& arguments) {
    for (const std::string& argument : arguments) {
        if (argument == "-j" || argument == "--json") return true;
    }
    return false;
}

std::string jsonEscape(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (unsigned char c : text) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    static const char* digits = "0123456789abcdef";
                    out += "\\u00";
                    out.push_back(digits[c >> 4]);
                    out.push_back(digits[c & 0x0F]);
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

std::string quoted(const std::string& text) {
    return "\"" + jsonEscape(text) + "\"";
}

std::string jsonMethodList(const Plan& plan) {
    std::string out = "[";
    for (std::size_t i = 0; i < plan.methods.size(); ++i) {
        const Method* method = findMethod(plan.methods[i]);
        if (i > 0) out += ",";
        out += "{\"step\":" + std::to_string(i + 1);
        out += ",\"id\":" + std::to_string(static_cast<unsigned>(method->id));
        out += ",\"name\":" + quoted(method->name);
        out += ",\"kind\":\"" +
               std::string(method->kind == MethodKind::Cipher ? "cipher" : "encoding") + "\"}";
    }
    out += "]";
    return out;
}

std::filesystem::path pathFromUtf8(std::string_view text) {
#if defined(_WIN32)
    if (text.empty()) return {};
    const int length = static_cast<int>(text.size());
    const int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                           length, nullptr, 0);
    if (size <= 0) {
        throw Error("Invalid UTF-8 file path: " + std::string(text));
    }
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length,
                          wide.data(), size);
    return std::filesystem::path(wide);
#else
    return std::filesystem::path(std::string(text));
#endif
}

std::string readTextFile(const std::string& path) {
    std::ifstream file(pathFromUtf8(path), std::ios::binary);
    if (!file) throw Error(tr(Msg::ErrFileOpen, {path}));
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (!file.eof() && file.bad()) throw Error(tr(Msg::ErrFileRead, {path}));

    if (startsWith(data, "\xEF\xBB\xBF")) data.erase(0, 3); // BOM UTF-8
    if (endsWith(data, "\r\n")) {
        data.erase(data.size() - 2);
    } else if (endsWith(data, "\n") || endsWith(data, "\r")) {
        data.pop_back();
    }
    return data;
}

void writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream file(pathFromUtf8(path), std::ios::binary | std::ios::trunc);
    if (!file) throw Error(tr(Msg::ErrFileCreate, {path}));
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file) throw Error(tr(Msg::ErrFileWrite, {path}));
}

std::size_t parseLayers(const std::string& text) {
    if (text.empty()) throw Error(tr(Msg::ErrLayersInvalid));
    std::size_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') throw Error(tr(Msg::ErrLayersInvalid));
        value = value * 10 + static_cast<std::size_t>(c - '0');
        if (value > kMaxLayers) {
            throw Error(tr(Msg::ErrLayersTooHigh, {std::to_string(kMaxLayers)}));
        }
    }
    if (value == 0) throw Error(tr(Msg::ErrLayersZero));
    return value;
}

// ------------------------------------------------------------------ opciones

struct Options {
    std::string command;
    std::optional<std::string> message;
    std::optional<std::string> id;
    std::optional<std::string> file;
    std::optional<std::string> output;
    std::optional<std::string> passphrase;
    std::optional<std::size_t> layers;
    bool passwordPrompt = false;
    bool json           = false;
    bool quiet          = false;
    bool noColor        = false;
    bool verbose        = false;
    bool help           = false;
    bool version        = false;
};

std::string canonicalCommand(std::string_view raw) {
    const std::string text = toLower(trim(raw));
    if (text == "encrypt" || text == "enc" || text == "cifrar" || text == "encriptar" ||
        text == "criptografar") {
        return "encrypt";
    }
    if (text == "decrypt" || text == "dec" || text == "descifrar" || text == "desencriptar" ||
        text == "descriptografar") {
        return "decrypt";
    }
    if (text == "inspect" || text == "info" || text == "inspeccionar" ||
        text == "inspecionar") {
        return "inspect";
    }
    if (text == "methods" || text == "metodos" || text == "métodos" || text == "list") {
        return "methods";
    }
    if (text == "selftest" || text == "test" || text == "pruebas" || text == "testes") {
        return "selftest";
    }
    if (text == "help" || text == "ayuda" || text == "ajuda") return "help";
    if (text == "version" || text == "versión" || text == "versao" || text == "versão") {
        return "version";
    }
    if (text == "shell" || text == "consola" || text == "console" || text == "interactive") {
        return "shell";
    }
    if (text == "language" || text == "lang" || text == "idioma" || text == "lingua" ||
        text == "língua") {
        return "language";
    }
    if (text == "exit" || text == "quit" || text == "salir" || text == "sair") return "exit";
    return {};
}

bool commandAcceptsMessage(const std::string& command) {
    return command.empty() || command == "encrypt" || command == "decrypt" ||
           command == "language";
}

void prescanLanguage(const std::vector<std::string>& arguments) {
    bool positionalOnly = false;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string& argument = arguments[i];
        if (argument == "--") {
            positionalOnly = true;
            continue;
        }
        if (positionalOnly) continue;

        std::string value;
        bool found = false;
        if (argument == "-L" || argument == "--lang" || argument == "--language") {
            if (i + 1 < arguments.size()) {
                value = arguments[i + 1];
                found = true;
            }
        } else if (startsWith(argument, "--lang=")) {
            value = argument.substr(7);
            found = true;
        } else if (startsWith(argument, "--language=")) {
            value = argument.substr(11);
            found = true;
        }
        if (found) {
            if (const auto language = parseLanguage(value)) setCurrentLanguage(*language);
        }
    }
}

void parseArguments(const std::vector<std::string>& arguments, Options& options) {
    bool positionalOnly = false;

    for (std::size_t i = 0; i < arguments.size(); ++i) {
        std::string argument = arguments[i];

        if (!positionalOnly && argument == "--") {
            positionalOnly = true;
            continue;
        }

        if (!positionalOnly && argument.size() > 1 && argument.front() == '-') {
            std::string name = argument;
            std::optional<std::string> inlineValue;
            if (startsWith(name, "--")) {
                const std::size_t equals = name.find('=');
                if (equals != std::string::npos) {
                    inlineValue = name.substr(equals + 1);
                    name = name.substr(0, equals);
                }
            }

            const auto requireValue = [&](const char* optionName) -> std::string {
                if (inlineValue.has_value()) return *inlineValue;
                if (i + 1 >= arguments.size()) {
                    throw Error(tr(Msg::ErrOptionNeedsValue, {optionName}));
                }
                return arguments[++i];
            };

            const auto setOnce = [&](std::optional<std::string>& target,
                                     const std::string& value, const char* optionName) {
                if (target.has_value()) {
                    throw Error(tr(Msg::ErrOptionDuplicate, {optionName}));
                }
                target = value;
            };

            if (name == "-m" || name == "--message" || name == "--text" || name == "-t") {
                setOnce(options.message, requireValue(name.c_str()), "-m");
            } else if (name == "-i" || name == "--id") {
                setOnce(options.id, requireValue(name.c_str()), "-i");
            } else if (name == "-f" || name == "--file") {
                setOnce(options.file, requireValue(name.c_str()), "-f");
            } else if (name == "-o" || name == "--output") {
                setOnce(options.output, requireValue(name.c_str()), "-o");
            } else if (name == "-l" || name == "--layers") {
                if (options.layers.has_value()) {
                    throw Error(tr(Msg::ErrOptionDuplicate, {"-l"}));
                }
                options.layers = parseLayers(requireValue(name.c_str()));
            } else if (name == "-p" || name == "--password") {
                options.passwordPrompt = true;
                options.passphrase.reset();
            } else if (name == "--passphrase") {
                options.passphrase    = requireValue(name.c_str());
                options.passwordPrompt = false;
            } else if (name == "-L" || name == "--lang" || name == "--language") {
                const std::string value = requireValue(name.c_str());
                const auto language = parseLanguage(value);
                if (!language) throw Error(tr(Msg::ErrLanguageUnknown, {value}));
                setCurrentLanguage(*language);
            } else if (name == "-v" || name == "--verbose") {
                options.verbose = true;
            } else if (name == "-j" || name == "--json") {
                options.json = true;
            } else if (name == "-q" || name == "--quiet") {
                options.quiet = true;
            } else if (name == "--no-color") {
                options.noColor = true;
            } else if (name == "-h" || name == "--help") {
                options.help = true;
            } else if (name == "-V" || name == "--version") {
                options.version = true;
            } else {
                throw Error(tr(Msg::ErrOptionUnknown, {name}));
            }
            continue;
        }

        const std::string command = positionalOnly ? std::string() : canonicalCommand(argument);
        if (!command.empty() && options.command.empty() && !options.message.has_value()) {
            options.command = command;
            continue;
        }

        if (options.command.empty() && !positionalOnly && !options.message.has_value()) {
            const std::string retired = toLower(trim(argument));
            if (retired == "credits" || retired == "creditos" || retired == "créditos") {
                throw Error(tr(Msg::ErrCommandUnknown, {argument}));
            }
        }

        if (!commandAcceptsMessage(options.command)) {
            throw Error(tr(Msg::ErrCommandNoMessage, {options.command}));
        }
        if (options.message.has_value()) {
            throw Error(tr(Msg::ErrUnexpectedArgument, {argument}));
        }
        options.message = argument;
    }
}

// ------------------------------------------------------------------ textos

void printHelp() {
    std::cout << tr(Msg::HelpText, {kVersion});
}

void printVersion() {
    const std::size_t total = methods().size();
    const std::size_t ciphers = static_cast<std::size_t>(
        std::count_if(methods().begin(), methods().end(),
                      [](const Method& method) { return method.kind == MethodKind::Cipher; }));
    const std::size_t encodings = total - ciphers;

    std::cout << "Kruptos " << kVersion << "\n";
    std::cout << "  " << padRight(std::string(tr(Msg::VersionEngine)) + ":", 13) << " "
              << "C++20, ChaCha20, SHA-256, HMAC-SHA-256, PBKDF2\n";
    std::cout << "  " << padRight(std::string(tr(Msg::VersionPlatform)) + ":", 13) << " "
              << platformName() << "\n";
    std::cout << "  " << padRight(std::string(tr(Msg::VersionMethods)) + ":", 13) << " "
              << tr(Msg::VersionMethodsSummary, {std::to_string(total),
                                                 std::to_string(ciphers),
                                                 std::to_string(encodings)})
              << "\n";
}

void printMethods(const Ui& ui) {
    if (ui.json()) {
        std::string out = "{\"ok\":true,\"operation\":\"methods\",\"methods\":[";
        for (std::size_t i = 0; i < methods().size(); ++i) {
            const Method& method = methods()[i];
            if (i > 0) out += ",";
            out += "{\"id\":" + std::to_string(static_cast<unsigned>(method.id)) +
                   ",\"name\":" + quoted(method.name) +
                   ",\"kind\":\"" +
                   std::string(method.kind == MethodKind::Cipher ? "cipher" : "encoding") +
                   "\"" +
                   ",\"description\":" + quoted(tr(method.summary)) + "}";
        }
        out += "]}\n";
        std::cout << out;
        return;
    }

    if (ui.quiet()) {
        for (const Method& method : methods()) std::cout << method.name << "\n";
        return;
    }

    ui.info(tr(Msg::MethodsTitle, {std::to_string(methods().size())}));
    ui.plain("");
    ui.plain("   " + padRight(tr(Msg::MethodsHeaderId), 4) +
             padRight(tr(Msg::MethodsHeaderMethod), 18) +
             padRight(tr(Msg::MethodsHeaderType), 16) +
             tr(Msg::MethodsHeaderDescription));
    ui.plain("   " + padRight("--", 4) + padRight(std::string(16, '-'), 18) +
             padRight(std::string(14, '-'), 16) + std::string(41, '-'));
    for (const Method& method : methods()) {
        std::cout << "   " << padRight(std::to_string(static_cast<unsigned>(method.id)), 4)
                  << padRight(method.name, 18)
                  << padRight(methodKindName(method.kind), 16)
                  << tr(method.summary) << "\n";
    }
    ui.plain("");
    ui.info(tr(Msg::MethodsFooter));
}

void printLayerList(const Plan& plan, const Ui& ui, bool reverseOrder) {
    if (ui.quiet() || ui.json()) return;
    for (std::size_t index = 0; index < plan.methods.size(); ++index) {
        const std::size_t step = reverseOrder ? plan.methods.size() - 1 - index : index;
        const Method* method = findMethod(plan.methods[step]);
        std::cout << "      " << padRight(std::to_string(index + 1) + ".", 5)
                  << padRight(method->name, 18)
                  << methodKindName(method->kind)
                  << "\n";
    }
}

// ------------------------------------------------------------------ entrada

std::string resolveMessage(const Options& options, const Ui& ui, Msg hint) {
    if (options.message.has_value() && options.file.has_value()) {
        throw Error(tr(Msg::ErrUseMessageOrFile));
    }
    if (options.message.has_value()) return *options.message;
    if (options.file.has_value()) return readTextFile(*options.file);

    if (!term::stdinIsTty()) {
        std::string data = term::readAllStdin();
        if (endsWith(data, "\r\n")) {
            data.erase(data.size() - 2);
        } else if (endsWith(data, "\n") || endsWith(data, "\r")) {
            data.pop_back();
        }
        return data;
    }

    ui.info(tr(hint));
    std::string line;
    if (!term::readLine("  > ", line)) throw Error(tr(Msg::ErrNoText));
    return line;
}

std::string resolveId(const Options& options, const Ui& ui) {
    if (options.id.has_value()) return *options.id;
    if (!term::stdinIsTty()) {
        throw Error(tr(Msg::ErrMissingId));
    }
    ui.info(tr(Msg::HintId));
    std::string line;
    if (!term::readLine("  ID> ", line)) throw Error(tr(Msg::ErrNoIdReceived));
    line = trim(line);
    if (line.empty()) throw Error(tr(Msg::ErrEmptyId));
    return line;
}

std::string resolvePassphrase(const Options& options, bool confirm) {
    if (options.passphrase.has_value()) {
        if (options.passphrase->empty()) throw Error(tr(Msg::ErrPasswordEmpty));
        return *options.passphrase;
    }
    if (!options.passwordPrompt) return {};

    if (!term::stdinIsTty()) {
        throw Error(tr(Msg::ErrNoConsolePassword));
    }
    const std::string first = term::readPassword(tr(Msg::PromptPassword));
    if (first.empty()) throw Error(tr(Msg::ErrPasswordEmpty));
    if (confirm) {
        const std::string second = term::readPassword(tr(Msg::PromptRepeatPassword));
        if (first != second) throw Error(tr(Msg::ErrPasswordsDiffer));
    }
    return first;
}

// ------------------------------------------------------------------ comandos

void commandEncrypt(const Options& options, const Ui& ui) {
    const std::string plaintext =
        resolveMessage(options, ui, Msg::HintPlaintext);
    if (plaintext.empty()) throw Error(tr(Msg::ErrEmptyMessage));
    const std::string passphrase = resolvePassphrase(options, true);

    PlanOptions planOptions;
    planOptions.layers = options.layers;

    const Encryption encryption = encryptText(toBytes(plaintext), planOptions);
    const std::string id = token::encode(encryption.plan,
                                         cipherFingerprint(encryption.ciphertext),
                                         passphrase);

    if (options.output.has_value()) writeTextFile(*options.output, encryption.ciphertext);

    if (ui.json()) {
        std::string out = "{\"ok\":true,\"operation\":\"encrypt\",\"version\":" +
                          quoted(kVersion) +
                          ",\"layers\":" + std::to_string(encryption.plan.methods.size()) +
                          ",\"protected\":" + (passphrase.empty() ? "false" : "true") +
                          ",\"seedFingerprint\":" +
                          quoted(seedFingerprint(encryption.plan.seed)) +
                          ",\"id\":" + quoted(id) +
                          ",\"ciphertext\":" + quoted(encryption.ciphertext) +
                          ",\"methods\":" + jsonMethodList(encryption.plan) + "}\n";
        std::cout << out;
        return;
    }

    if (ui.quiet()) {
        std::cout << id << "\n" << encryption.ciphertext << "\n";
        return;
    }

    ui.ok(tr(Msg::EncryptHeading, {std::to_string(encryption.plan.methods.size())}));
    printLayerList(encryption.plan, ui, false);
    ui.plain("");
    if (!passphrase.empty()) {
        ui.ok(tr(Msg::PasswordProtectedId));
    }
    ui.info(tr(Msg::IdLabel));
    ui.plain(id);
    ui.plain("");
    ui.info(tr(Msg::SeedFingerprintLabel, {seedFingerprint(encryption.plan.seed)}));
    ui.info(tr(Msg::CiphertextLabel));
    ui.plain(encryption.ciphertext);
    ui.plain("");
    if (options.output.has_value()) {
        ui.ok(tr(Msg::SavedCiphertext, {*options.output}));
    }
    ui.warn(tr(Msg::SaveIdWarning));
}

void commandDecrypt(const Options& options, const Ui& ui) {
    const std::string ciphertext = resolveMessage(options, ui, Msg::HintCiphertext);
    if (stripWhitespace(ciphertext).empty()) throw Error(tr(Msg::ErrEmptyCiphertext));
    const std::string id = resolveId(options, ui);

    const token::Header header = token::peek(id);

    std::string passphrase;
    if (header.passwordProtected) {
        if (options.passphrase.has_value()) {
            passphrase = *options.passphrase;
            if (passphrase.empty()) throw Error(tr(Msg::ErrPasswordEmpty));
        } else if (options.passwordPrompt || term::stdinIsTty()) {
            if (!term::stdinIsTty()) {
                throw Error(tr(Msg::ProtectedIdNeedsPassword));
            }
            passphrase = term::readPassword(tr(Msg::PromptPassword));
            if (passphrase.empty()) throw Error(tr(Msg::ErrPasswordEmpty));
        } else {
            throw Error(tr(Msg::ProtectedIdNeedsPassword));
        }
    }

    const token::Decoded decoded = token::decode(id, passphrase);
    const Bytes plaintext = revertPlan(ciphertext, decoded.plan);

    std::string canonical;
    try {
        canonical = applyPlan(plaintext, decoded.plan);
    } catch (const Error&) {
        throw Error(tr(Msg::CiphertextMismatch));
    }
    if (cipherFingerprint(canonical) != decoded.cipherHash) {
        throw Error(tr(Msg::IntegrityFailed));
    }

    const std::string message = toString(plaintext);
    if (options.output.has_value()) writeTextFile(*options.output, message);

    if (ui.json()) {
        std::string out = "{\"ok\":true,\"operation\":\"decrypt\"" +
                          std::string(",\"layers\":") +
                          std::to_string(decoded.plan.methods.size()) +
                          ",\"protected\":" + (decoded.passwordProtected ? "true" : "false") +
                          ",\"id\":" + quoted(trim(id)) +
                          ",\"message\":" + quoted(message) +
                          ",\"methods\":" + jsonMethodList(decoded.plan) + "}\n";
        std::cout << out;
        return;
    }

    if (ui.quiet()) {
        std::cout << message << "\n";
        return;
    }

    ui.ok(tr(Msg::DecryptHeading, {std::to_string(decoded.plan.methods.size())}));
    if (options.verbose) {
        ui.info(tr(Msg::ReverseOrderHeading));
        printLayerList(decoded.plan, ui, true);
        ui.plain("");
    }
    ui.info(tr(Msg::OriginalMessageLabel));
    std::cout << message << "\n";
    if (options.output.has_value()) {
        ui.ok(tr(Msg::SavedMessage, {*options.output}));
    }
}

void commandInspect(const Options& options, const Ui& ui) {
    const std::string id = resolveId(options, ui);
    const token::Header header = token::peek(id);

    std::optional<token::Decoded> decoded;
    if (!header.passwordProtected) {
        decoded = token::decode(id, "");
    } else if (options.passphrase.has_value() || options.passwordPrompt) {
        std::string passphrase;
        if (options.passphrase.has_value()) {
            passphrase = *options.passphrase;
        } else {
            if (!term::stdinIsTty()) {
                throw Error(tr(Msg::ErrNoConsolePassword));
            }
            passphrase = term::readPassword(tr(Msg::PromptPassword));
        }
        decoded = token::decode(id, passphrase);
    }

    if (ui.json()) {
        std::string out = "{\"ok\":true,\"operation\":\"inspect\",\"version\":" +
                          std::to_string(header.version) +
                          ",\"protected\":" +
                          std::string(header.passwordProtected ? "true" : "false");
        out += ",\"id\":" + quoted(trim(id));
        if (decoded.has_value()) {
            out += ",\"layers\":" + std::to_string(decoded->plan.methods.size());
            out += ",\"seedFingerprint\":" + quoted(seedFingerprint(decoded->plan.seed));
            out += ",\"methods\":" + jsonMethodList(decoded->plan);
        }
        out += "}\n";
        std::cout << out;
        return;
    }

    if (ui.quiet()) {
        if (!decoded.has_value()) {
            std::cout << tr(Msg::ProtectedShort) << "\n";
            return;
        }
        for (std::size_t i = 0; i < decoded->plan.methods.size(); ++i) {
            if (i > 0) std::cout << "\n";
            std::cout << findMethod(decoded->plan.methods[i])->name;
        }
        std::cout << "\n";
        return;
    }

    ui.info(tr(Msg::InspectHeading));
    ui.plain("  " + trim(id));
    ui.plain("");
    ui.field(tr(Msg::FieldVersion), "1");
    ui.field(tr(Msg::FieldProtection),
             header.passwordProtected ? tr(Msg::ProtectionPassword) : tr(Msg::ProtectionNone));
    if (!decoded.has_value()) {
        ui.plain("");
        ui.warn(tr(Msg::InspectProtectedHint));
        return;
    }
    ui.field(tr(Msg::FieldLayers), std::to_string(decoded->plan.methods.size()));
    ui.field(tr(Msg::FieldSeed),
             tr(Msg::SeedValue, {seedFingerprint(decoded->plan.seed)}));
    ui.plain("");
    ui.info(tr(Msg::ApplicationOrderHeading));
    printLayerList(decoded->plan, ui, false);
}

void commandLanguage(const Options& options, const Ui& ui) {
    const auto reportCurrent = [&]() {
        if (ui.json()) {
            std::cout << "{\"ok\":true,\"operation\":\"language\",\"current\":"
                      << quoted(languageCode(currentLanguage()))
                      << ",\"available\":[\"en\",\"es\",\"pt\"]}\n";
        } else if (ui.quiet()) {
            std::cout << languageCode(currentLanguage()) << "\n";
        } else {
            ui.info(tr(Msg::LanguageCurrent, {languageName(currentLanguage())}));
            ui.info(tr(Msg::LanguageAvailable));
        }
    };

    if (options.message.has_value()) {
        const auto language = parseLanguage(*options.message);
        if (!language) throw Error(tr(Msg::ErrLanguageUnknown, {*options.message}));
        setCurrentLanguage(*language);
        if (ui.json()) {
            std::cout << "{\"ok\":true,\"operation\":\"language\",\"current\":"
                      << quoted(languageCode(*language))
                      << ",\"available\":[\"en\",\"es\",\"pt\"]}\n";
        } else if (ui.quiet()) {
            std::cout << languageCode(*language) << "\n";
        } else {
            ui.ok(tr(Msg::LanguageSet, {languageName(*language)}));
        }
        return;
    }

    if (ui.quiet() || ui.json()) {
        reportCurrent();
        return;
    }

    if (!term::stdinIsTty()) {
        reportCurrent();
        return;
    }

    ui.info(tr(Msg::LanguageHeading));
    const Lang order[] = {Lang::English, Lang::Spanish, Lang::Portuguese};
    for (int i = 0; i < 3; ++i) {
        std::cout << "    " << (i + 1) << ". " << languageName(order[i]) << "\n";
    }

    std::string line;
    if (!term::readLine("    " + std::string(tr(Msg::LanguagePrompt)) + " ", line)) {
        ui.info(tr(Msg::LanguageCancelled));
        return;
    }
    line = trim(line);
    if (line.empty()) {
        ui.info(tr(Msg::LanguageCancelled));
        return;
    }

    Lang selected;
    if (line == "1") {
        selected = Lang::English;
    } else if (line == "2") {
        selected = Lang::Spanish;
    } else if (line == "3") {
        selected = Lang::Portuguese;
    } else {
        const auto parsed = parseLanguage(line);
        if (!parsed) {
            ui.warn(tr(Msg::LanguageInvalid));
            return;
        }
        selected = *parsed;
    }

    setCurrentLanguage(selected);
    ui.ok(tr(Msg::LanguageSet, {languageName(selected)}));
}

// ------------------------------------------------------------------ shell

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inSingle = false;
    bool inDouble = false;
    bool hasToken = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (inDouble) {
            if (c == '\\' && i + 1 < line.size() &&
                (line[i + 1] == '"' || line[i + 1] == '\\')) {
                current.push_back(line[++i]);
            } else if (c == '"') {
                inDouble = false;
            } else {
                current.push_back(c);
            }
        } else if (inSingle) {
            if (c == '\'') inSingle = false;
            else current.push_back(c);
        } else if (c == '"') {
            inDouble = true;
            hasToken = true;
        } else if (c == '\'') {
            inSingle = true;
            hasToken = true;
        } else if (isAsciiSpace(static_cast<unsigned char>(c))) {
            if (hasToken) {
                tokens.push_back(current);
                current.clear();
                hasToken = false;
            }
        } else {
            current.push_back(c);
            hasToken = true;
        }
    }
    if (inSingle || inDouble) throw Error(tr(Msg::ErrUnclosedQuotes));
    if (hasToken) tokens.push_back(current);
    return tokens;
}

int execute(const Options& options, const Ui& ui);

int shell(bool useColor) {
    const bool color = useColor && term::supportsColor();
    const Ui ui(color, false, false);

    if (term::supportsColor()) term::clearScreen();
    ui.banner();
    ui.info(tr(Msg::ShellWelcome));
    ui.plain("");

    for (;;) {
        const std::string prompt = color
            ? std::string(kCyan) + "kruptos>" + kReset + " "
            : std::string("kruptos> ");
        std::string line;
        if (!term::readLine(prompt, line)) {
            std::cout << "\n";
            break;
        }
        line = trim(line);
        if (line.empty()) continue;

        try {
            std::vector<std::string> tokens = tokenize(line);
            if (tokens.empty()) continue;

            const std::string command = canonicalCommand(tokens[0]);
            if (command == "exit") break;
            if (command == "shell") continue;

            Options options;
            options.json    = false;
            options.noColor = !color;
            parseArguments(tokens, options);
            if (options.command == "shell") continue;
            execute(options, ui);
        } catch (const Error& error) {
            std::cerr << ui.paint(kRed, "kruptos: error:") << " " << error.what() << "\n";
        } catch (const std::exception& error) {
            std::cerr << ui.paint(kRed, "kruptos: error:") << " " << error.what() << "\n";
        }
        std::cout << "\n";
    }

    ui.info(tr(Msg::ShellGoodbye));
    return 0;
}

// ------------------------------------------------------------------ despacho

void validateOptions(const Options& options, const std::string& command) {
    const auto reject = [&](bool present, const char* option) {
        if (present) {
            throw Error(tr(Msg::ErrOptionUnused, {option, command}));
        }
    };

    if (command == "encrypt") {
        reject(options.id.has_value(), "-i");
    } else if (command == "decrypt") {
        reject(options.layers.has_value(), "-l");
    } else if (command == "inspect") {
        reject(options.message.has_value(), "-m");
        reject(options.file.has_value(), "-f");
        reject(options.output.has_value(), "-o");
        reject(options.layers.has_value(), "-l");
    } else if (command == "language") {
        reject(options.id.has_value(), "-i");
        reject(options.file.has_value(), "-f");
        reject(options.output.has_value(), "-o");
        reject(options.layers.has_value(), "-l");
        reject(options.passwordPrompt || options.passphrase.has_value(), "-p");
    } else if (command == "methods" || command == "selftest" || command == "help" ||
               command == "version" || command == "shell" || command == "exit") {
        reject(options.message.has_value(), "-m");
        reject(options.id.has_value(), "-i");
        reject(options.file.has_value(), "-f");
        reject(options.output.has_value(), "-o");
        reject(options.layers.has_value(), "-l");
        reject(options.passwordPrompt || options.passphrase.has_value(), "-p");
    }
}

int execute(const Options& options, const Ui& ui) {
    if (options.help) {
        printHelp();
        return 0;
    }
    if (options.version) {
        printVersion();
        return 0;
    }

    std::string command = options.command;
    if (command.empty()) {
        command = options.id.has_value() ? "decrypt" : "encrypt";
    }

    validateOptions(options, command);

    if (command == "help") {
        printHelp();
    } else if (command == "version") {
        printVersion();
    } else if (command == "language") {
        commandLanguage(options, ui);
    } else if (command == "methods") {
        printMethods(ui);
    } else if (command == "selftest") {
        return runSelfTest(options.verbose);
    } else if (command == "inspect") {
        commandInspect(options, ui);
    } else if (command == "encrypt") {
        commandEncrypt(options, ui);
    } else if (command == "decrypt") {
        commandDecrypt(options, ui);
    } else if (command == "shell") {
        return shell(!options.noColor);
    } else if (command == "exit") {
        return 0;
    } else {
        throw Error(tr(Msg::ErrCommandUnknown, {command}));
    }
    return 0;
}

void reportError(const std::string& message, bool json) {
    if (json) {
        std::cerr << "{\"ok\":false,\"error\":" << quoted(message) << "}\n";
        return;
    }
    const std::string prefix = term::supportsColor()
        ? std::string(kRed) + "kruptos: error:" + kReset
        : std::string("kruptos: error:");
    std::cerr << prefix << " " << message << "\n";
    std::cerr << tr(Msg::ErrHelpHint) << "\n";
}

} // namespace

int run(const std::vector<std::string>& arguments) {
    setCurrentLanguage(detectLanguage());
    prescanLanguage(arguments);
    const bool json = isJsonRequested(arguments);

    try {
        if (arguments.empty()) return shell(true);

        Options options;
        parseArguments(arguments, options);

        if (options.help) {
            printHelp();
            return 0;
        }
        if (options.version) {
            printVersion();
            return 0;
        }

        Ui ui(term::supportsColor() && !options.noColor, options.quiet, options.json);
        return execute(options, ui);
    } catch (const Error& error) {
        reportError(error.what(), json);
        return 1;
    } catch (const std::exception& error) {
        reportError(error.what(), json);
        return 1;
    }
}

} // namespace kruptos::cli
