// Kruptos - consola multiplataforma (UTF-8, colores, lectura de líneas y contraseñas).
#pragma once

#include <cstddef>
#include <string>

namespace kruptos::term {

void setupConsole();
bool stdoutIsTty();
bool stdinIsTty();
bool supportsColor();

// Devuelve false si se alcanzó EOF sin leer nada.
bool readLine(const std::string& prompt, std::string& line);
std::string readAllStdin();
std::string readPassword(const std::string& prompt);

void clearScreen();
std::size_t width();

} // namespace kruptos::term
