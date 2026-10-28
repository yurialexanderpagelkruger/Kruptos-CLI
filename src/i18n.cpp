#include "i18n.h"

#include "common.h"

#include <cstdlib>
#include <string>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace kruptos {
namespace {

Lang g_language = Lang::Spanish;

const char* pick(const char* spanish, const char* english, const char* portuguese) {
    switch (g_language) {
        case Lang::Spanish:    return spanish;
        case Lang::English:    return english;
        case Lang::Portuguese: return portuguese;
    }
    return english;
}

const char* kHelpSpanish =
    "Kruptos {1} - Cifrado multicapa adaptativo\n"
    "\n"
    "USO\n"
    "  kruptos [comando] [opciones]\n"
    "\n"
    "COMANDOS\n"
    "  encrypt, cifrar        Cifra un mensaje (comando por defecto)\n"
    "  decrypt, descifrar     Descifra un mensaje usando su ID\n"
    "  inspect, inspeccionar  Muestra el plan de capas de un ID\n"
    "  methods, metodos       Lista los métodos disponibles\n"
    "  selftest, pruebas      Ejecuta las pruebas internas\n"
    "  language, idioma       Cambia el idioma (es, en, pt)\n"
    "  help, ayuda            Muestra esta ayuda\n"
    "  version                Muestra la versión\n"
    "  shell, consola         Inicia el modo interactivo\n"
    "\n"
    "  Sin argumentos se abre el modo interactivo.\n"
    "\n"
    "OPCIONES\n"
    "  -m, --message <texto>   Mensaje o texto cifrado (si falta, se pide por consola)\n"
    "  -i, --id <ID>           Identificador KR1-... (necesario para descifrar)\n"
    "  -f, --file <ruta>       Lee el mensaje de un archivo UTF-8\n"
    "  -o, --output <ruta>     Guarda el resultado en un archivo\n"
    "  -l, --layers <n>        Fuerza el número de capas (1-16)\n"
    "  -p, --password          Pide una contraseña para proteger el ID\n"
    "      --passphrase <txt>  Contraseña en la línea de comandos (menos segura)\n"
    "  -L, --lang <idioma>     Idioma: es, en o pt\n"
    "  -v, --verbose           Muestra el detalle de las capas\n"
    "  -j, --json              Salida JSON\n"
    "  -q, --quiet             Solo el resultado\n"
    "      --no-color          Desactiva los colores ANSI\n"
    "  -h, --help              Muestra esta ayuda\n"
    "  -V, --version           Muestra la versión\n"
    "\n"
    "EJEMPLOS\n"
    "  kruptos encrypt -m \"Hola mundo\"\n"
    "  kruptos encrypt -m \"Hola mundo\" -l 10\n"
    "  kruptos encrypt -m \"Hola mundo\" -p\n"
    "  kruptos decrypt -i KR1-XXXX... -m \"3FQ8...\"\n"
    "  kruptos decrypt -f cifrado.txt -i KR1-XXXX...\n"
    "  Get-Content mensaje.txt | kruptos encrypt -o cifrado.txt\n"
    "  kruptos inspect -i KR1-XXXX...\n"
    "  kruptos language en\n"
    "\n"
    "SEGURIDAD\n"
    "  - Sin contraseña, el ID contiene la semilla del plan: quien tenga el ID puede descifrar.\n"
    "  - Con -p, el plan y la semilla se cifran con ChaCha20 y se autentican con HMAC-SHA-256.\n"
    "  - La contraseña se deriva con PBKDF2-HMAC-SHA256 (150000 iteraciones).\n"
    "  - Las capas aportan difusión y oscurecimiento, pero no sustituyen a un cifrado moderno si el ID viaja con el mensaje.\n"
    "\n"
    "CRÉDITOS\n"
    "  by Yuri Alexander Pagel Krüger\n";

const char* kHelpEnglish =
    "Kruptos {1} - Adaptive multi-layer encryption\n"
    "\n"
    "USAGE\n"
    "  kruptos [command] [options]\n"
    "\n"
    "COMMANDS\n"
    "  encrypt                Encrypt a message (default command)\n"
    "  decrypt                Decrypt a message with its ID\n"
    "  inspect                Show the layer plan of an ID\n"
    "  methods                List the available methods\n"
    "  selftest               Run the internal tests\n"
    "  language               Change the language (es, en, pt)\n"
    "  help                   Show this help\n"
    "  version                Show the version\n"
    "  shell                  Start interactive mode\n"
    "\n"
    "  With no arguments the interactive shell starts.\n"
    "\n"
    "OPTIONS\n"
    "  -m, --message <text>    Message or ciphertext (prompted if omitted)\n"
    "  -i, --id <ID>           Plan identifier KR1-... (required to decrypt)\n"
    "  -f, --file <path>       Read the message from a UTF-8 file\n"
    "  -o, --output <path>     Save the result to a file\n"
    "  -l, --layers <n>        Force the number of layers (1-16)\n"
    "  -p, --password          Prompt for a password to protect the ID\n"
    "      --passphrase <txt>  Password on the command line (less secure)\n"
    "  -L, --lang <language>   Language: es, en or pt\n"
    "  -v, --verbose           Show layer details\n"
    "  -j, --json              JSON output\n"
    "  -q, --quiet             Result only\n"
    "      --no-color          Disable ANSI colors\n"
    "  -h, --help              Show this help\n"
    "  -V, --version           Show the version\n"
    "\n"
    "EXAMPLES\n"
    "  kruptos encrypt -m \"Hello world\"\n"
    "  kruptos encrypt -m \"Hello world\" -l 10\n"
    "  kruptos encrypt -m \"Hello world\" -p\n"
    "  kruptos decrypt -i KR1-XXXX... -m \"3FQ8...\"\n"
    "  kruptos decrypt -f cipher.txt -i KR1-XXXX...\n"
    "  Get-Content message.txt | kruptos encrypt -o cipher.txt\n"
    "  kruptos inspect -i KR1-XXXX...\n"
    "  kruptos language pt\n"
    "\n"
    "SECURITY\n"
    "  - Without a password, the ID contains the plan seed: whoever has the ID can decrypt.\n"
    "  - With -p, the plan and seed are encrypted with ChaCha20 and authenticated with HMAC-SHA-256.\n"
    "  - The password is derived with PBKDF2-HMAC-SHA256 (150000 iterations).\n"
    "  - The layer stack adds diffusion and obfuscation, but it does not replace modern encryption if the ID travels with the message.\n"
    "\n"
    "CREDITS\n"
    "  by Yuri Alexander Pagel Krüger\n";

const char* kHelpPortuguese =
    "Kruptos {1} - Criptografia multicamada adaptativa\n"
    "\n"
    "USO\n"
    "  kruptos [comando] [opções]\n"
    "\n"
    "COMANDOS\n"
    "  encrypt, cifrar        Criptografa uma mensagem (comando padrão)\n"
    "  decrypt, descifrar     Descriptografa uma mensagem usando seu ID\n"
    "  inspect, inspecionar   Mostra o plano de camadas de um ID\n"
    "  methods, metodos       Lista os métodos disponíveis\n"
    "  selftest, testes       Executa os testes internos\n"
    "  language, idioma       Altera o idioma (es, en, pt)\n"
    "  help, ajuda            Mostra esta ajuda\n"
    "  version                Mostra a versão\n"
    "  shell, console         Inicia o modo interativo\n"
    "\n"
    "  Sem argumentos, o modo interativo é iniciado.\n"
    "\n"
    "OPÇÕES\n"
    "  -m, --message <texto>   Mensagem ou texto cifrado (solicitado se omitido)\n"
    "  -i, --id <ID>           Identificador do plano KR1-... (obrigatório para descriptografar)\n"
    "  -f, --file <caminho>    Lê a mensagem de um arquivo UTF-8\n"
    "  -o, --output <caminho>  Salva o resultado em um arquivo\n"
    "  -l, --layers <n>        Força o número de camadas (1-16)\n"
    "  -p, --password          Solicita uma senha para proteger o ID\n"
    "      --passphrase <txt>  Senha na linha de comando (menos segura)\n"
    "  -L, --lang <idioma>     Idioma: es, en ou pt\n"
    "  -v, --verbose           Mostra detalhes das camadas\n"
    "  -j, --json              Saída JSON\n"
    "  -q, --quiet             Apenas o resultado\n"
    "      --no-color          Desativa as cores ANSI\n"
    "  -h, --help              Mostra esta ajuda\n"
    "  -V, --version           Mostra a versão\n"
    "\n"
    "EXEMPLOS\n"
    "  kruptos encrypt -m \"Olá mundo\"\n"
    "  kruptos encrypt -m \"Olá mundo\" -l 10\n"
    "  kruptos encrypt -m \"Olá mundo\" -p\n"
    "  kruptos decrypt -i KR1-XXXX... -m \"3FQ8...\"\n"
    "  kruptos decrypt -f cifrado.txt -i KR1-XXXX...\n"
    "  Get-Content mensagem.txt | kruptos encrypt -o cifrado.txt\n"
    "  kruptos inspect -i KR1-XXXX...\n"
    "  kruptos language es\n"
    "\n"
    "SEGURANÇA\n"
    "  - Sem senha, o ID contém a semente do plano: quem tiver o ID pode descriptografar.\n"
    "  - Com -p, o plano e a semente são cifrados com ChaCha20 e autenticados com HMAC-SHA-256.\n"
    "  - A senha é derivada com PBKDF2-HMAC-SHA256 (150000 iterações).\n"
    "  - As camadas adicionam difusão e obscurecimento, mas não substituem a criptografia moderna se o ID viajar com a mensagem.\n"
    "\n"
    "CRÉDITOS\n"
    "  by Yuri Alexander Pagel Krüger\n";

} // namespace

Lang currentLanguage() {
    return g_language;
}

void setCurrentLanguage(Lang language) {
    g_language = language;
}

std::optional<Lang> parseLanguage(std::string_view text) {
    const std::string normalized = toLower(trim(text));
    if (normalized.empty()) return std::nullopt;

    if (normalized == "english" || normalized == "inglés" || normalized == "ingles" ||
        normalized == "eng") {
        return Lang::English;
    }
    if (normalized == "spanish" || normalized == "español" || normalized == "espanol" ||
        normalized == "castellano" || normalized == "spa" || normalized == "esp") {
        return Lang::Spanish;
    }
    if (normalized == "portuguese" || normalized == "português" || normalized == "portugues" ||
        normalized == "por") {
        return Lang::Portuguese;
    }

    if (normalized.size() >= 2) {
        const std::string two = normalized.substr(0, 2);
        const bool boundary = normalized.size() == 2 ||
                              normalized[2] == '-' || normalized[2] == '_' ||
                              normalized[2] == '.' || normalized[2] == '@';
        if (boundary) {
            if (two == "es") return Lang::Spanish;
            if (two == "en") return Lang::English;
            if (two == "pt") return Lang::Portuguese;
        }
    }
    return std::nullopt;
}

Lang detectLanguage() {
    if (const char* configured = std::getenv("KRUPTOS_LANG"); configured != nullptr) {
        if (const auto parsed = parseLanguage(configured)) return *parsed;
    }

#if defined(_WIN32)
    const LANGID languageId = ::GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(languageId)) {
        case LANG_SPANISH:    return Lang::Spanish;
        case LANG_ENGLISH:    return Lang::English;
        case LANG_PORTUGUESE: return Lang::Portuguese;
        default: break;
    }
#else
    const char* variables[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    for (const char* variable : variables) {
        const char* value = std::getenv(variable);
        if (value == nullptr || *value == '\0') continue;
        if (const auto parsed = parseLanguage(value)) return *parsed;
    }
#endif

    return Lang::English;
}

const char* languageCode(Lang language) {
    switch (language) {
        case Lang::Spanish:    return "es";
        case Lang::English:    return "en";
        case Lang::Portuguese: return "pt";
    }
    return "en";
}

const char* languageName(Lang language) {
    switch (language) {
        case Lang::Spanish:    return "Español";
        case Lang::English:    return "English";
        case Lang::Portuguese: return "Português";
    }
    return "English";
}

const char* tr(Msg message) {
    switch (message) {
        // ------------------------------------------------------------ Generales
        case Msg::AppTagline:
            return pick("Cifrado multicapa adaptativo",
                        "Adaptive multi-layer encryption",
                        "Criptografia multicamada adaptativa");

        // ------------------------------------------------------------ Versión
        case Msg::VersionEngine:
            return pick("Motor", "Engine", "Motor");
        case Msg::VersionPlatform:
            return pick("Plataforma", "Platform", "Plataforma");
        case Msg::VersionMethods:
            return pick("Métodos", "Methods", "Métodos");
        case Msg::VersionMethodsSummary:
            return pick("{1} ({2} cifrados, {3} codificaciones)",
                        "{1} ({2} ciphers, {3} encodings)",
                        "{1} ({2} cifras, {3} codificações)");

        // ------------------------------------------------------------ Ayuda
        case Msg::HelpText:
            return pick(kHelpSpanish, kHelpEnglish, kHelpPortuguese);

        // ------------------------------------------------------------ Métodos
        case Msg::MethodsTitle:
            return pick("Repertorio de {1} métodos reversibles:",
                        "Repertoire of {1} reversible methods:",
                        "Repertório de {1} métodos reversíveis:");
        case Msg::MethodsHeaderId:
            return "ID";
        case Msg::MethodsHeaderMethod:
            return pick("MÉTODO", "METHOD", "MÉTODO");
        case Msg::MethodsHeaderType:
            return pick("TIPO", "TYPE", "TIPO");
        case Msg::MethodsHeaderDescription:
            return pick("DESCRIPCIÓN", "DESCRIPTION", "DESCRIÇÃO");
        case Msg::MethodsFooter:
            return pick("Cada ejecución elige entre 7 y 12 métodos distintos en un orden diferente.",
                        "Each run picks 7 to 12 distinct methods in a different order.",
                        "Cada execução escolhe entre 7 e 12 métodos distintos em uma ordem diferente.");
        case Msg::KindCipher:
            return pick("Cifrado", "Cipher", "Cifra");
        case Msg::KindEncoding:
            return pick("Codificación", "Encoding", "Codificação");

        // ------------------------------------------------------------ Descripciones
        case Msg::MethodChaCha20:
            return pick("Flujo ChaCha20 (RFC 8439) con clave y nonce generados",
                        "ChaCha20 stream cipher (RFC 8439) with a fresh key and nonce",
                        "Fluxo ChaCha20 (RFC 8439) com chave e nonce gerados");
        case Msg::MethodRc4:
            return pick("Flujo RC4 con clave generada (capa heredada de ruido)",
                        "RC4 stream cipher with a fresh key (legacy noise layer)",
                        "Fluxo RC4 com chave gerada (camada legada de ruído)");
        case Msg::MethodSha256Ctr:
            return pick("Flujo XOR derivado de SHA-256 en modo contador",
                        "XOR stream derived from SHA-256 in counter mode",
                        "Fluxo XOR derivado de SHA-256 em modo contador");
        case Msg::MethodCaesar:
            return pick("Desplazamiento aditivo de cada byte módulo 256",
                        "Additive shift of every byte modulo 256",
                        "Deslocamento aditivo de cada byte módulo 256");
        case Msg::MethodVigenere:
            return pick("Suma módulo 256 con clave repetida",
                        "Modulo-256 addition with a repeating key",
                        "Soma módulo 256 com chave repetida");
        case Msg::MethodAtbash:
            return pick("Complemento a 255 de cada byte",
                        "255's complement of every byte",
                        "Complemento de 255 de cada byte");
        case Msg::MethodRot13:
            return pick("Rotación de 13 posiciones en letras A-Z / a-z",
                        "13-position rotation on A-Z / a-z letters",
                        "Rotação de 13 posições em letras A-Z / a-z");
        case Msg::MethodRot47:
            return pick("Rotación de 47 posiciones en ASCII imprimible (33-126)",
                        "47-position rotation on printable ASCII (33-126)",
                        "Rotação de 47 posições em ASCII imprimível (33-126)");
        case Msg::MethodReverse:
            return pick("Inversión completa del orden de los bytes",
                        "Full reversal of byte order",
                        "Inversão completa da ordem dos bytes");
        case Msg::MethodBitRotate:
            return pick("Rotación de bits dentro de cada byte",
                        "Bit rotation inside every byte",
                        "Rotação de bits dentro de cada byte");
        case Msg::MethodNibbleSwap:
            return pick("Intercambio de los dos nibbles de cada byte",
                        "Swap of both nibbles in every byte",
                        "Troca dos dois nibbles de cada byte");
        case Msg::MethodRailFence:
            return pick("Transposición en zigzag con 2-6 rieles",
                        "Zigzag transposition with 2-6 rails",
                        "Transposição em zigue-zague com 2-6 trilhos");
        case Msg::MethodBase64:
            return pick("Base64 estándar con relleno (RFC 4648)",
                        "Standard Base64 with padding (RFC 4648)",
                        "Base64 padrão com preenchimento (RFC 4648)");
        case Msg::MethodBase64Url:
            return pick("Base64 variante URL sin relleno",
                        "URL-safe Base64 without padding",
                        "Base64 variante URL sem preenchimento");
        case Msg::MethodBase32:
            return pick("Base32 RFC 4648 sin relleno",
                        "RFC 4648 Base32 without padding",
                        "Base32 RFC 4648 sem preenchimento");
        case Msg::MethodBase16:
            return pick("Hexadecimal en minúsculas",
                        "Lowercase hexadecimal",
                        "Hexadecimal em minúsculas");
        case Msg::MethodBase58:
            return pick("Base58 estilo Bitcoin",
                        "Bitcoin-style Base58",
                        "Base58 estilo Bitcoin");
        case Msg::MethodBase85:
            return pick("Ascii85 delimitado con <~ ~>",
                        "Ascii85 delimited with <~ ~>",
                        "Ascii85 delimitado com <~ ~>");
        case Msg::MethodPercent:
            return pick("Codificación porcentual de URL (%XX)",
                        "URL percent-encoding (%XX)",
                        "Codificação percentual de URL (%XX)");
        case Msg::MethodQuotedPrintable:
            return pick("Quoted-printable sin saltos de línea",
                        "Quoted-printable without line breaks",
                        "Quoted-printable sem quebras de linha");
        case Msg::MethodBinary:
            return pick("Bits de cada byte separados por espacios",
                        "Bits of every byte separated by spaces",
                        "Bits de cada byte separados por espaços");
        case Msg::MethodOctal:
            return pick("Valores octal de 3 dígitos separados por espacios",
                        "3-digit octal values separated by spaces",
                        "Valores octais de 3 dígitos separados por espaços");
        case Msg::MethodDecimal:
            return pick("Valores decimales 0-255 separados por espacios",
                        "Decimal values 0-255 separated by spaces",
                        "Valores decimais 0-255 separados por espaços");
        case Msg::MethodUnicodeEscape:
            return pick("Secuencias \\u00XX por byte",
                        "\\u00XX sequences per byte",
                        "Sequências \\u00XX por byte");
        case Msg::MethodHtmlEntities:
            return pick("Entidades numéricas &#NNN; por byte",
                        "Numeric entities &#NNN; per byte",
                        "Entidades numéricas &#NNN; por byte");
        case Msg::MethodMorse:
            return pick("Código morse (marca * para minúsculas)",
                        "Morse code (* marks lowercase letters)",
                        "Código morse (* marca minúsculas)");

        // ------------------------------------------------------------ Cifrado
        case Msg::EncryptHeading:
            return pick("Mensaje cifrado con {1} capas en un orden diferente:",
                        "Message encrypted with {1} layers in a different order:",
                        "Mensagem cifrada com {1} camadas em uma ordem diferente:");
        case Msg::IdLabel:
            return pick("ID del plan (guárdalo junto al texto cifrado):",
                        "Plan ID (keep it with the ciphertext):",
                        "ID do plano (guarde-o junto ao texto cifrado):");
        case Msg::SeedFingerprintLabel:
            return pick("Huella de la semilla: {1}",
                        "Seed fingerprint: {1}",
                        "Impressão digital da semente: {1}");
        case Msg::CiphertextLabel:
            return pick("Texto cifrado:", "Ciphertext:", "Texto cifrado:");
        case Msg::SavedCiphertext:
            return pick("Texto cifrado guardado en: {1}",
                        "Ciphertext saved to: {1}",
                        "Texto cifrado salvo em: {1}");
        case Msg::PasswordProtectedId:
            return pick("ID protegido con contraseña (PBKDF2-HMAC-SHA256, 150000 iteraciones).",
                        "ID protected with a password (PBKDF2-HMAC-SHA256, 150000 iterations).",
                        "ID protegido com senha (PBKDF2-HMAC-SHA256, 150000 iterações).");
        case Msg::SaveIdWarning:
            return pick("Sin el ID no se puede recuperar el mensaje.",
                        "Without the ID the message cannot be recovered.",
                        "Sem o ID, a mensagem não pode ser recuperada.");

        // ------------------------------------------------------------ Descifrado
        case Msg::DecryptHeading:
            return pick("Mensaje descifrado y verificado ({1} capas revertidas).",
                        "Message decrypted and verified ({1} layers reversed).",
                        "Mensagem decifrada e verificada ({1} camadas revertidas).");
        case Msg::ReverseOrderHeading:
            return pick("Orden inverso aplicado:",
                        "Reverse order applied:",
                        "Ordem inversa aplicada:");
        case Msg::OriginalMessageLabel:
            return pick("Mensaje original:", "Original message:", "Mensagem original:");
        case Msg::SavedMessage:
            return pick("Mensaje guardado en: {1}",
                        "Message saved to: {1}",
                        "Mensagem salva em: {1}");
        case Msg::IntegrityFailed:
            return pick("Verificación de integridad fallida: el texto no coincide con el ID.",
                        "Integrity check failed: the text does not match the ID.",
                        "Falha na verificação de integridade: o texto não corresponde ao ID.");
        case Msg::CiphertextMismatch:
            return pick("El texto cifrado no corresponde a este ID o está dañado.",
                        "The ciphertext does not match this ID or is damaged.",
                        "O texto cifrado não corresponde a este ID ou está danificado.");
        case Msg::ProtectedIdNeedsPassword:
            return pick("Este ID está protegido; indica la contraseña con --passphrase.",
                        "This ID is protected; provide the password with --passphrase.",
                        "Este ID está protegido; informe a senha com --passphrase.");

        // ------------------------------------------------------------ Inspección
        case Msg::InspectHeading:
            return pick("ID del plan:", "Plan ID:", "ID do plano:");
        case Msg::FieldVersion:
            return pick("Versión", "Version", "Versão");
        case Msg::FieldProtection:
            return pick("Protección", "Protection", "Proteção");
        case Msg::ProtectionPassword:
            return pick("Contraseña (ChaCha20 + HMAC-SHA-256 + PBKDF2)",
                        "Password (ChaCha20 + HMAC-SHA-256 + PBKDF2)",
                        "Senha (ChaCha20 + HMAC-SHA-256 + PBKDF2)");
        case Msg::ProtectionNone:
            return pick("Ninguna (el ID contiene la semilla)",
                        "None (the ID contains the seed)",
                        "Nenhuma (o ID contém a semente)");
        case Msg::FieldLayers:
            return pick("Capas", "Layers", "Camadas");
        case Msg::FieldSeed:
            return pick("Semilla", "Seed", "Semente");
        case Msg::SeedValue:
            return pick("32 bytes, huella {1}",
                        "32 bytes, fingerprint {1}",
                        "32 bytes, impressão {1}");
        case Msg::ApplicationOrderHeading:
            return pick("Orden de aplicación (de la primera a la última capa):",
                        "Application order (from the first to the last layer):",
                        "Ordem de aplicação (da primeira à última camada):");
        case Msg::InspectProtectedHint:
            return pick("ID protegido: usa -p (o --passphrase) para ver el plan de capas.",
                        "Protected ID: use -p (or --passphrase) to see the layer plan.",
                        "ID protegido: use -p (ou --passphrase) para ver o plano de camadas.");
        case Msg::ProtectedShort:
            return pick("Protegido", "Protected", "Protegido");

        // ------------------------------------------------------------ Entradas
        case Msg::HintPlaintext:
            return pick("Escribe el mensaje de texto plano y pulsa Enter.",
                        "Enter the plaintext message and press Enter.",
                        "Digite a mensagem de texto simples e pressione Enter.");
        case Msg::HintCiphertext:
            return pick("Escribe el texto cifrado y pulsa Enter.",
                        "Enter the ciphertext and press Enter.",
                        "Digite o texto cifrado e pressione Enter.");
        case Msg::HintId:
            return pick("Introduce el ID que se generó al cifrar.",
                        "Enter the ID that was generated when encrypting.",
                        "Informe o ID gerado ao cifrar.");
        case Msg::PromptPassword:
            return pick("Contraseña> ", "Password> ", "Senha> ");
        case Msg::PromptRepeatPassword:
            return pick("Repite la contraseña> ", "Repeat the password> ", "Repita a senha> ");

        // ------------------------------------------------------------ Idioma
        case Msg::LanguageHeading:
            return pick("Selecciona un idioma:", "Select a language:", "Selecione um idioma:");
        case Msg::LanguagePrompt:
            return pick("Idioma [1-3] (Enter = mantener):",
                        "Language [1-3] (Enter = keep):",
                        "Idioma [1-3] (Enter = manter):");
        case Msg::LanguageSet:
            return pick("Idioma cambiado a {1}.", "Language changed to {1}.", "Idioma alterado para {1}.");
        case Msg::LanguageCurrent:
            return pick("Idioma actual: {1}.", "Current language: {1}.", "Idioma atual: {1}.");
        case Msg::LanguageAvailable:
            return pick("Disponibles: 1. English, 2. Español, 3. Português. También puedes usar --lang es|en|pt.",
                        "Available: 1. English, 2. Español, 3. Português. You can also use --lang es|en|pt.",
                        "Disponíveis: 1. English, 2. Español, 3. Português. Você também pode usar --lang es|en|pt.");
        case Msg::LanguageInvalid:
            return pick("Opción de idioma no válida.", "Invalid language option.", "Opção de idioma inválida.");
        case Msg::LanguageCancelled:
            return pick("Cambio de idioma cancelado.", "Language change cancelled.", "Alteração de idioma cancelada.");
        case Msg::ErrLanguageUnknown:
            return pick("Idioma no válido: {1}. Usa es, en o pt.",
                        "Invalid language: {1}. Use es, en or pt.",
                        "Idioma inválido: {1}. Use es, en ou pt.");

        // ------------------------------------------------------------ Shell
        case Msg::ShellWelcome:
            return pick("Modo interactivo. Escribe 'help' para ver los comandos, 'language' para cambiar el idioma y 'exit' para salir.",
                        "Interactive mode. Type 'help' for commands, 'language' to change the language, and 'exit' to quit.",
                        "Modo interativo. Digite 'help' para ver os comandos, 'language' para alterar o idioma e 'exit' para sair.");
        case Msg::ShellGoodbye:
            return pick("Hasta luego.", "Goodbye.", "Até logo.");

        // ------------------------------------------------------------ Errores CLI
        case Msg::ErrOptionUnknown:
            return pick("Opción desconocida: {1}.", "Unknown option: {1}.", "Opção desconhecida: {1}.");
        case Msg::ErrOptionNeedsValue:
            return pick("La opción {1} requiere un valor.",
                        "Option {1} requires a value.",
                        "A opção {1} requer um valor.");
        case Msg::ErrOptionDuplicate:
            return pick("La opción {1} se indicó varias veces.",
                        "Option {1} was given more than once.",
                        "A opção {1} foi informada várias vezes.");
        case Msg::ErrLayersInvalid:
            return pick("El número de capas debe ser un entero positivo.",
                        "The number of layers must be a positive integer.",
                        "O número de camadas deve ser um inteiro positivo.");
        case Msg::ErrLayersTooHigh:
            return pick("El número de capas es demasiado alto (máximo {1}).",
                        "The number of layers is too high (maximum {1}).",
                        "O número de camadas é muito alto (máximo {1}).");
        case Msg::ErrLayersZero:
            return pick("El número de capas debe ser mayor que cero.",
                        "The number of layers must be greater than zero.",
                        "O número de camadas deve ser maior que zero.");
        case Msg::ErrCommandNoMessage:
            return pick("El comando '{1}' no acepta un mensaje posicional.",
                        "Command '{1}' does not accept a positional message.",
                        "O comando '{1}' não aceita uma mensagem posicional.");
        case Msg::ErrUnexpectedArgument:
            return pick("Argumento inesperado: {1}.",
                        "Unexpected argument: {1}.",
                        "Argumento inesperado: {1}.");
        case Msg::ErrOptionUnused:
            return pick("La opción {1} no se usa con el comando '{2}'.",
                        "Option {1} is not used with the command '{2}'.",
                        "A opção {1} não é usada com o comando '{2}'.");
        case Msg::ErrCommandUnknown:
            return pick("Comando desconocido: {1}.", "Unknown command: {1}.", "Comando desconhecido: {1}.");
        case Msg::ErrEmptyMessage:
            return pick("El mensaje está vacío.", "The message is empty.", "A mensagem está vazia.");
        case Msg::ErrMessageTooLong:
            return pick("El mensaje es demasiado largo (máximo {1} bytes).",
                        "The message is too long (maximum {1} bytes).",
                        "A mensagem é longa demais (máximo {1} bytes).");
        case Msg::ErrEmptyCiphertext:
            return pick("El texto cifrado está vacío.",
                        "The ciphertext is empty.",
                        "O texto cifrado está vazio.");
        case Msg::ErrMissingId:
            return pick("Falta el ID del plan: usa -i KR1-...",
                        "Missing plan ID: use -i KR1-...",
                        "Falta o ID do plano: use -i KR1-...");
        case Msg::ErrEmptyId:
            return pick("El ID está vacío.", "The ID is empty.", "O ID está vazio.");
        case Msg::ErrPasswordEmpty:
            return pick("La contraseña no puede estar vacía.",
                        "The password cannot be empty.",
                        "A senha não pode estar vazia.");
        case Msg::ErrNoConsolePassword:
            return pick("No hay consola interactiva para pedir la contraseña; usa --passphrase.",
                        "There is no interactive console to ask for the password; use --passphrase.",
                        "Não há console interativo para pedir a senha; use --passphrase.");
        case Msg::ErrPasswordsDiffer:
            return pick("Las contraseñas no coinciden.",
                        "The passwords do not match.",
                        "As senhas não coincidem.");
        case Msg::ErrNoText:
            return pick("No se recibió ningún texto.", "No text was received.", "Nenhum texto foi recebido.");
        case Msg::ErrNoIdReceived:
            return pick("No se recibió ningún ID.", "No ID was received.", "Nenhum ID foi recebido.");
        case Msg::ErrUseMessageOrFile:
            return pick("Usa -m o -f, no ambos.", "Use -m or -f, not both.", "Use -m ou -f, não ambos.");
        case Msg::ErrCiphertextTooLarge:
            return pick("El texto cifrado es demasiado grande.",
                        "The ciphertext is too large.",
                        "O texto cifrado é grande demais.");
        case Msg::ErrLayerTooLarge:
            return pick("Resultado intermedio demasiado grande en la capa {1}.",
                        "Intermediate result too large at layer {1}.",
                        "Resultado intermediário grande demais na camada {1}.");
        case Msg::ErrNoLayer:
            return pick("No se pudo aplicar ninguna capa al mensaje.",
                        "No layer could be applied to the message.",
                        "Nenhuma camada pôde ser aplicada à mensagem.");
        case Msg::ErrPlanTooShort:
            return pick("Solo se pudieron aplicar {1} capas (se pidieron {2}).",
                        "Only {1} layers could be applied ({2} were requested).",
                        "Só foi possível aplicar {1} camadas ({2} foram solicitadas).");
        case Msg::ErrResultTooLarge:
            return pick("El resultado cifrado sería demasiado grande; usa un mensaje más corto.",
                        "The encrypted result would be too large; use a shorter message.",
                        "O resultado cifrado seria grande demais; use uma mensagem mais curta.");
        case Msg::ErrLayerRevert:
            return pick("No se pudo revertir la capa {1} ({2}): {3}.",
                        "Could not reverse layer {1} ({2}): {3}.",
                        "Não foi possível reverter a camada {1} ({2}): {3}.");
        case Msg::ErrInternalNotPrintable:
            return pick("Error interno: el texto cifrado no es ASCII imprimible.",
                        "Internal error: the ciphertext is not printable ASCII.",
                        "Erro interno: o texto cifrado não é ASCII imprimível.");
        case Msg::ErrPlanInvalidSeed:
            return pick("Plan inválido: la semilla debe tener 32 bytes.",
                        "Invalid plan: the seed must be 32 bytes.",
                        "Plano inválido: a semente deve ter 32 bytes.");
        case Msg::ErrPlanInvalidCount:
            return pick("Plan inválido: número de capas fuera de rango.",
                        "Invalid plan: layer count out of range.",
                        "Plano inválido: número de camadas fora do intervalo.");
        case Msg::ErrPlanUnknownMethod:
            return pick("Plan inválido: método desconocido ({1}).",
                        "Invalid plan: unknown method ({1}).",
                        "Plano inválido: método desconhecido ({1}).");
        case Msg::ErrHelpHint:
            return pick("Prueba 'kruptos --help' para ver el uso.",
                        "Try 'kruptos --help' for usage.",
                        "Tente 'kruptos --help' para ver o uso.");

        // ------------------------------------------------------------ IDs
        case Msg::ErrIdPrefix:
            return pick("ID inválido: debe comenzar por KR1-.",
                        "Invalid ID: it must start with KR1-.",
                        "ID inválido: deve começar com KR1-.");
        case Msg::ErrIdBody:
            return pick("ID inválido: falta el cuerpo.", "Invalid ID: the body is missing.", "ID inválido: falta o corpo.");
        case Msg::ErrIdShort:
            return pick("ID inválido: demasiado corto.", "Invalid ID: too short.", "ID inválido: curto demais.");
        case Msg::ErrIdVersion:
            return pick("Versión de ID no soportada ({1}).",
                        "Unsupported ID version ({1}).",
                        "Versão de ID não suportada ({1}).");
        case Msg::ErrIdFlags:
            return pick("ID inválido: opciones desconocidas.",
                        "Invalid ID: unknown options.",
                        "ID inválido: opções desconhecidas.");
        case Msg::ErrIdIncomplete:
            return pick("ID inválido: estructura incompleta.",
                        "Invalid ID: incomplete structure.",
                        "ID inválido: estrutura incompleta.");
        case Msg::ErrIdChecksum:
            return pick("ID inválido: la comprobación de integridad ha fallado (¿copia incompleta?).",
                        "Invalid ID: the integrity check failed (incomplete copy?).",
                        "ID inválido: a verificação de integridade falhou (cópia incompleta?).");
        case Msg::ErrIdPlanIncomplete:
            return pick("ID dañado: plan incompleto.", "Damaged ID: incomplete plan.", "ID danificado: plano incompleto.");
        case Msg::ErrIdLayerCount:
            return pick("ID dañado: número de capas no válido.",
                        "Damaged ID: invalid layer count.",
                        "ID danificado: número de camadas inválido.");
        case Msg::ErrIdPlanLength:
            return pick("ID dañado: longitud del plan incoherente.",
                        "Damaged ID: inconsistent plan length.",
                        "ID danificado: comprimento do plano incoerente.");
        case Msg::ErrIdUnknownMethod:
            return pick("ID no compatible: método desconocido ({1}).",
                        "Unsupported ID: unknown method ({1}).",
                        "ID não compatível: método desconhecido ({1}).");
        case Msg::ErrIdProtected:
            return pick("Este ID está protegido con contraseña: indica una con -p o --passphrase.",
                        "This ID is password-protected: provide one with -p or --passphrase.",
                        "Este ID está protegido com senha: informe uma com -p ou --passphrase.");
        case Msg::ErrIdProtectedDamaged:
            return pick("ID protegido incompleto o dañado.",
                        "Protected ID is incomplete or damaged.",
                        "ID protegido incompleto ou danificado.");
        case Msg::ErrWrongPassword:
            return pick("Contraseña incorrecta o ID alterado.",
                        "Wrong password or altered ID.",
                        "Senha incorreta ou ID alterado.");
        case Msg::ErrIdTooLong:
            return pick("ID demasiado largo.", "ID is too long.", "ID longo demais.");
        case Msg::ErrIdCreateLayers:
            return pick("No se puede crear el ID: número de capas fuera de rango.",
                        "Cannot create the ID: layer count out of range.",
                        "Não é possível criar o ID: número de camadas fora do intervalo.");
        case Msg::ErrIdCreateSeed:
            return pick("No se puede crear el ID: semilla inválida.",
                        "Cannot create the ID: invalid seed.",
                        "Não é possível criar o ID: semente inválida.");

        // ------------------------------------------------------------ Terminal
        case Msg::ErrPasswordRead:
            return pick("No se pudo leer la contraseña de la consola; usa --passphrase.",
                        "Could not read the password from the console; use --passphrase.",
                        "Não foi possível ler a senha do console; use --passphrase.");
        case Msg::ErrInputCancelled:
            return pick("Entrada cancelada.", "Input cancelled.", "Entrada cancelada.");
        case Msg::ErrUnclosedQuotes:
            return pick("Comillas sin cerrar.", "Unclosed quotes.", "Aspas não fechadas.");

        // ------------------------------------------------------------ Codificaciones
        case Msg::CodecInvalidLength:
            return pick("Longitud inválida", "Invalid length", "Comprimento inválido");
        case Msg::CodecInvalidCharacter:
            return pick("Carácter no válido", "Invalid character", "Caractere inválido");
        case Msg::CodecResidualBits:
            return pick("Bits residuales no válidos", "Invalid trailing bits", "Bits residuais inválidos");
        case Msg::CodecInvalidValue:
            return pick("Valor fuera de rango", "Value out of range", "Valor fora do intervalo");
        case Msg::CodecIncompleteSequence:
            return pick("Secuencia incompleta o mal formada",
                        "Incomplete or malformed sequence",
                        "Sequência incompleta ou malformada");
        case Msg::CodecInvalidDigit:
            return pick("Dígito no válido", "Invalid digit", "Dígito inválido");
        case Msg::CodecUnknownSymbol:
            return pick("Símbolo desconocido", "Unknown symbol", "Símbolo desconhecido");
        case Msg::CodecEmptySymbol:
            return pick("Símbolo vacío no válido", "Invalid empty symbol", "Símbolo vazio inválido");
        case Msg::CodecNotRepresentable:
            return pick("Carácter no representable", "Character not representable", "Caractere não representável");
        case Msg::CodecMorseCase:
            return pick("La marca de minúscula solo se aplica a letras",
                        "The lowercase marker only applies to letters",
                        "A marca de minúscula só se aplica a letras");

        // ------------------------------------------------------------ Archivos
        case Msg::ErrFileOpen:
            return pick("No se pudo abrir el archivo: {1}.",
                        "Could not open the file: {1}.",
                        "Não foi possível abrir o arquivo: {1}.");
        case Msg::ErrFileRead:
            return pick("Error al leer el archivo: {1}.",
                        "Error reading the file: {1}.",
                        "Erro ao ler o arquivo: {1}.");
        case Msg::ErrFileCreate:
            return pick("No se pudo crear el archivo: {1}.",
                        "Could not create the file: {1}.",
                        "Não foi possível criar o arquivo: {1}.");
        case Msg::ErrFileWrite:
            return pick("Error al escribir el archivo: {1}.",
                        "Error writing the file: {1}.",
                        "Erro ao escrever o arquivo: {1}.");

        // ------------------------------------------------------------ Pruebas
        case Msg::SelfTestTitle:
            return pick("Kruptos {1} - Pruebas internas",
                        "Kruptos {1} - Internal tests",
                        "Kruptos {1} - Testes internos");
        case Msg::SelfTestCrypto:
            return pick("Vectores criptográficos", "Cryptographic vectors", "Vetores criptográficos");
        case Msg::SelfTestEncodings:
            return pick("Codificaciones reversibles", "Reversible encodings", "Codificações reversíveis");
        case Msg::SelfTestCiphers:
            return pick("Cifrados reversibles", "Reversible ciphers", "Cifras reversíveis");
        case Msg::SelfTestTokens:
            return pick("Identificadores (ID)", "Identifiers (ID)", "Identificadores (ID)");
        case Msg::SelfTestEngine:
            return pick("Motor multicapa", "Multi-layer engine", "Motor multicamadas");
        case Msg::SelfTestRepertoire:
            return pick("Repertorio", "Repertoire", "Repertório");
        case Msg::SelfTestPassed:
            return pick("{1}/{2} pruebas superadas",
                        "{1}/{2} tests passed",
                        "{1}/{2} testes aprovados");
        case Msg::SelfTestFailed:
            return pick("{1} de {2} pruebas han fallado",
                        "{1} of {2} tests failed",
                        "{1} de {2} testes falharam");
        case Msg::SelfTestFail:
            return pick("FALLO", "FAIL", "FALHA");
    }
    return "";
}

std::string tr(Msg message, std::initializer_list<std::string> arguments) {
    const std::string pattern = tr(message);
    std::string result;
    result.reserve(pattern.size());

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '{' && i + 2 < pattern.size() && pattern[i + 2] == '}' &&
            pattern[i + 1] >= '1' && pattern[i + 1] <= '9') {
            const std::size_t index = static_cast<std::size_t>(pattern[i + 1] - '1');
            if (index < arguments.size()) {
                result += arguments.begin()[index];
            }
            i += 2;
        } else {
            result.push_back(pattern[i]);
        }
    }
    return result;
}

} // namespace kruptos
