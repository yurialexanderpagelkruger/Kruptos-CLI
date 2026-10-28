#include "terminal.h"

#include "common.h"
#include "i18n.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <conio.h>
#  include <io.h>
#  include <cwchar>
#else
#  include <csignal>
#  include <sys/ioctl.h>
#  include <termios.h>
#  include <unistd.h>
#endif

namespace kruptos::term {
namespace {

bool g_color = false;

#if defined(_WIN32)
// Restaura la página de códigos de la consola al salir: no es agradable dejar
// la terminal del usuario cambiada por una ejecución de la herramienta.
struct ConsoleCodePageGuard {
    UINT outputCodePage = 0;
    UINT inputCodePage  = 0;
    bool active         = false;

    ~ConsoleCodePageGuard() {
        if (!active) return;
        std::cout.flush();
        std::cerr.flush();
        if (outputCodePage != 0) ::SetConsoleOutputCP(outputCodePage);
        if (inputCodePage != 0) ::SetConsoleCP(inputCodePage);
    }
};

ConsoleCodePageGuard g_codePageGuard;
#endif

void dropLastUtf8Character(std::string& text) {
    while (!text.empty() &&
           (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {
        text.pop_back();
    }
    if (!text.empty()) text.pop_back();
}

#if defined(_WIN32)
std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                           static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (size <= 0) throw Error("Could not convert the password to UTF-8");
    std::string out(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                          out.data(), size, nullptr, nullptr);
    return out;
}
#else
termios g_passwordSettings{};
volatile std::sig_atomic_t g_passwordActive = 0;

// Si el usuario pulsa Ctrl+C mientras escribe la contraseña, devuelve el eco
// a la terminal antes de terminar (si no, quedaría desactivado).
void restoreTerminalOnSignal(int signalNumber) {
    if (g_passwordActive != 0) {
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_passwordSettings);
        g_passwordActive = 0;
    }
    std::signal(signalNumber, SIG_DFL);
    std::raise(signalNumber);
}
#endif

} // namespace

void setupConsole() {
#if defined(_WIN32)
    g_codePageGuard.outputCodePage = ::GetConsoleOutputCP();
    g_codePageGuard.inputCodePage  = ::GetConsoleCP();
    g_codePageGuard.active = (g_codePageGuard.outputCodePage != 0 ||
                              g_codePageGuard.inputCodePage != 0);
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);

    auto enableVirtualTerminal = [](DWORD handleId) {
        const HANDLE handle = ::GetStdHandle(handleId);
        if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return false;
        DWORD mode = 0;
        if (!::GetConsoleMode(handle, &mode)) return false;
        return ::SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                                            ENABLE_PROCESSED_OUTPUT) != 0;
    };

    const bool color = enableVirtualTerminal(STD_OUTPUT_HANDLE);
    enableVirtualTerminal(STD_ERROR_HANDLE);
    g_color = color;
#else
    g_color = true;
#endif

    if (std::getenv("NO_COLOR") != nullptr) g_color = false;
    if (const char* term = std::getenv("TERM"); term != nullptr && std::strcmp(term, "dumb") == 0) {
        g_color = false;
    }
    if (!stdoutIsTty()) g_color = false;
}

bool stdoutIsTty() {
#if defined(_WIN32)
    return ::_isatty(::_fileno(stdout)) != 0;
#else
    return ::isatty(STDOUT_FILENO) != 0;
#endif
}

bool stdinIsTty() {
#if defined(_WIN32)
    return ::_isatty(::_fileno(stdin)) != 0;
#else
    return ::isatty(STDIN_FILENO) != 0;
#endif
}

bool supportsColor() {
    return g_color;
}

bool readLine(const std::string& prompt, std::string& line) {
    std::cout << prompt;
    std::cout.flush();
    if (!std::getline(std::cin, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return true;
}

std::string readAllStdin() {
    std::string data;
    char buffer[4096];
    while (std::cin.read(buffer, sizeof(buffer)) || std::cin.gcount() > 0) {
        data.append(buffer, static_cast<std::size_t>(std::cin.gcount()));
        if (std::cin.eof()) break;
    }
    return data;
}

std::string readPassword(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

    std::string password;

#if defined(_WIN32)
    for (;;) {
        const wint_t input = ::_getwch();
        if (input == WEOF) {
            std::cout << "\n";
            throw Error(tr(Msg::ErrPasswordRead));
        }
        if (input == 0 || input == 0xE0) { // tecla especial: se consume su código
            (void)::_getwch();
            continue;
        }
        const wchar_t wide = static_cast<wchar_t>(input);
        if (wide == L'\r' || wide == L'\n') break;
        if (wide == 8) { // retroceso
            dropLastUtf8Character(password);
            continue;
        }
        if (wide == 3) { // Ctrl+C
            std::cout << "\n";
            throw Error(tr(Msg::ErrInputCancelled));
        }
        const std::wstring one(1, wide);
        password += wideToUtf8(one);
    }
#else
    termios oldSettings{};
    bool restore = false;
    if (::tcgetattr(STDIN_FILENO, &oldSettings) == 0) {
        g_passwordSettings = oldSettings;
        std::signal(SIGINT, restoreTerminalOnSignal);
        std::signal(SIGTERM, restoreTerminalOnSignal);

        termios newSettings = oldSettings;
        newSettings.c_lflag &= static_cast<tcflag_t>(~ECHO);
        if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &newSettings) == 0) {
            restore = true;
            g_passwordActive = 1;
        }
    }

    char c = 0;
    for (;;) {
        const ssize_t got = ::read(STDIN_FILENO, &c, 1);
        if (got <= 0) {
            if (restore) {
                ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldSettings);
                g_passwordActive = 0;
            }
            std::cout << "\n";
            throw Error(tr(Msg::ErrInputCancelled));
        }
        if (c == '\n' || c == '\r') break;
        if (c == 0x7F || c == 0x08) {
            dropLastUtf8Character(password);
            continue;
        }
        password.push_back(c);
    }
    if (restore) {
        ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &oldSettings);
        g_passwordActive = 0;
        std::signal(SIGINT, SIG_DFL);
        std::signal(SIGTERM, SIG_DFL);
    }
#endif

    std::cout << "\n";
    return password;
}

void clearScreen() {
    std::cout << "\x1b[2J\x1b[H";
    std::cout.flush();
}

std::size_t width() {
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (::GetConsoleScreenBufferInfo(::GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        const int columns = info.srWindow.Right - info.srWindow.Left + 1;
        if (columns > 20) return static_cast<std::size_t>(columns);
    }
#else
    winsize size{};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 20) {
        return static_cast<std::size_t>(size.ws_col);
    }
#endif
    return 80;
}

} // namespace kruptos::term
