// Kruptos - punto de entrada. En Windows se leen los argumentos como UTF-16 y se
// convierten a UTF-8 para que acentos y símbolos funcionen en PowerShell.
#include "cli.h"
#include "terminal.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shellapi.h>
#  include <cwchar>
#  if defined(_MSC_VER)
#    pragma comment(lib, "shell32.lib")
#  endif
#endif

namespace {

#if defined(_WIN32)
std::string wideToUtf8(const wchar_t* text) {
    if (text == nullptr) return {};
    const int length = static_cast<int>(::wcslen(text));
    if (length == 0) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
    if (size <= 0) throw std::runtime_error("no se pudo convertir la linea de comandos a UTF-8");
    std::string out(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text, length, out.data(), size, nullptr, nullptr);
    return out;
}

std::vector<std::string> systemArguments(int argc, char** argv) {
    std::vector<std::string> arguments;
    int count = 0;
    LPWSTR* wide = ::CommandLineToArgvW(::GetCommandLineW(), &count);
    if (wide != nullptr) {
        arguments.reserve(count > 0 ? static_cast<std::size_t>(count - 1) : 0);
        for (int i = 1; i < count; ++i) arguments.push_back(wideToUtf8(wide[i]));
        ::LocalFree(wide);
        return arguments;
    }
    for (int i = 1; i < argc; ++i) arguments.emplace_back(argv[i]);
    return arguments;
}
#else
std::vector<std::string> systemArguments(int argc, char** argv) {
    std::vector<std::string> arguments;
    arguments.reserve(argc > 0 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i) arguments.emplace_back(argv[i]);
    return arguments;
}
#endif

} // namespace

int main(int argc, char** argv) {
    kruptos::term::setupConsole();
    try {
        return kruptos::cli::run(systemArguments(argc, argv));
    } catch (const std::exception& error) {
        std::cerr << "kruptos: error fatal: " << error.what() << "\n";
        return 1;
    }
}
