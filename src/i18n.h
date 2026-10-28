// Kruptos - internacionalización (español, inglés y portugués).
#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace kruptos {

enum class Lang : std::uint8_t {
    Spanish    = 0,
    English    = 1,
    Portuguese = 2,
};

enum class Msg : std::uint16_t {
    // Generales
    AppTagline,

    // Versión
    VersionEngine,
    VersionPlatform,
    VersionMethods,
    VersionMethodsSummary,

    // Ayuda
    HelpText,

    // Tabla de métodos
    MethodsTitle,
    MethodsHeaderId,
    MethodsHeaderMethod,
    MethodsHeaderType,
    MethodsHeaderDescription,
    MethodsFooter,
    KindCipher,
    KindEncoding,

    // Descripciones de métodos
    MethodChaCha20,
    MethodRc4,
    MethodSha256Ctr,
    MethodCaesar,
    MethodVigenere,
    MethodAtbash,
    MethodRot13,
    MethodRot47,
    MethodReverse,
    MethodBitRotate,
    MethodNibbleSwap,
    MethodRailFence,
    MethodBase64,
    MethodBase64Url,
    MethodBase32,
    MethodBase16,
    MethodBase58,
    MethodBase85,
    MethodPercent,
    MethodQuotedPrintable,
    MethodBinary,
    MethodOctal,
    MethodDecimal,
    MethodUnicodeEscape,
    MethodHtmlEntities,
    MethodMorse,

    // Cifrado
    EncryptHeading,
    IdLabel,
    SeedFingerprintLabel,
    CiphertextLabel,
    SavedCiphertext,
    PasswordProtectedId,
    SaveIdWarning,

    // Descifrado
    DecryptHeading,
    ReverseOrderHeading,
    OriginalMessageLabel,
    SavedMessage,
    IntegrityFailed,
    CiphertextMismatch,
    ProtectedIdNeedsPassword,

    // Inspección
    InspectHeading,
    FieldVersion,
    FieldProtection,
    ProtectionPassword,
    ProtectionNone,
    FieldLayers,
    FieldSeed,
    SeedValue,
    ApplicationOrderHeading,
    InspectProtectedHint,
    ProtectedShort,

    // Entradas
    HintPlaintext,
    HintCiphertext,
    HintId,
    PromptPassword,
    PromptRepeatPassword,

    // Idioma
    LanguageHeading,
    LanguagePrompt,
    LanguageSet,
    LanguageCurrent,
    LanguageAvailable,
    LanguageInvalid,
    LanguageCancelled,
    ErrLanguageUnknown,

    // Shell
    ShellWelcome,
    ShellGoodbye,

    // Errores de la CLI
    ErrOptionUnknown,
    ErrOptionNeedsValue,
    ErrOptionDuplicate,
    ErrLayersInvalid,
    ErrLayersTooHigh,
    ErrLayersZero,
    ErrCommandNoMessage,
    ErrUnexpectedArgument,
    ErrOptionUnused,
    ErrCommandUnknown,
    ErrEmptyMessage,
    ErrMessageTooLong,
    ErrEmptyCiphertext,
    ErrMissingId,
    ErrEmptyId,
    ErrPasswordEmpty,
    ErrNoConsolePassword,
    ErrPasswordsDiffer,
    ErrNoText,
    ErrNoIdReceived,
    ErrUseMessageOrFile,
    ErrCiphertextTooLarge,
    ErrLayerTooLarge,
    ErrNoLayer,
    ErrPlanTooShort,
    ErrResultTooLarge,
    ErrLayerRevert,
    ErrInternalNotPrintable,
    ErrPlanInvalidSeed,
    ErrPlanInvalidCount,
    ErrPlanUnknownMethod,
    ErrHelpHint,

    // Identificadores
    ErrIdPrefix,
    ErrIdBody,
    ErrIdShort,
    ErrIdVersion,
    ErrIdFlags,
    ErrIdIncomplete,
    ErrIdChecksum,
    ErrIdPlanIncomplete,
    ErrIdLayerCount,
    ErrIdPlanLength,
    ErrIdUnknownMethod,
    ErrIdProtected,
    ErrIdProtectedDamaged,
    ErrWrongPassword,
    ErrIdTooLong,
    ErrIdCreateLayers,
    ErrIdCreateSeed,

    // Terminal
    ErrPasswordRead,
    ErrInputCancelled,
    ErrUnclosedQuotes,

    // Detalles técnicos de codificaciones
    CodecInvalidLength,
    CodecInvalidCharacter,
    CodecResidualBits,
    CodecInvalidValue,
    CodecIncompleteSequence,
    CodecInvalidDigit,
    CodecUnknownSymbol,
    CodecEmptySymbol,
    CodecNotRepresentable,
    CodecMorseCase,

    // Archivos
    ErrFileOpen,
    ErrFileRead,
    ErrFileCreate,
    ErrFileWrite,

    // Pruebas internas
    SelfTestTitle,
    SelfTestCrypto,
    SelfTestEncodings,
    SelfTestCiphers,
    SelfTestTokens,
    SelfTestEngine,
    SelfTestRepertoire,
    SelfTestPassed,
    SelfTestFailed,
    SelfTestFail,
};

// Idioma actual del proceso.
Lang currentLanguage();
void setCurrentLanguage(Lang language);

// Detección: KRUPTOS_LANG, luego el idioma del sistema; inglés si no se reconoce.
Lang detectLanguage();
std::optional<Lang> parseLanguage(std::string_view text);
const char* languageCode(Lang language);
const char* languageName(Lang language);

// Mensajes. Las variantes con argumentos sustituyen {1}, {2}, {3}...
const char* tr(Msg message);
std::string tr(Msg message, std::initializer_list<std::string> arguments);

} // namespace kruptos
