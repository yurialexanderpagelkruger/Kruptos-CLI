cmake_minimum_required(VERSION 3.20)
project(Kruptos VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Binario autocontenido: sin VCRUNTIME/MSVCP140/UCRT.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(KRUPTOS_STATIC "Link a fully static executable (Linux)" OFF)

add_executable(kruptos
    src/common.cpp
    src/crypto.cpp
    src/rng.cpp
    src/encoding.cpp
    src/i18n.cpp
    src/methods.cpp
    src/pipeline.cpp
    src/token.cpp
    src/terminal.cpp
    src/selftest.cpp
    src/cli.cpp
    src/main.cpp
)

target_include_directories(kruptos PRIVATE src)

if(MSVC)
    target_compile_options(kruptos PRIVATE /W3 /utf-8 /permissive-)
    target_compile_definitions(kruptos PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(kruptos PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_link_libraries(kruptos PRIVATE bcrypt shell32)
endif()

if(KRUPTOS_STATIC AND UNIX AND NOT APPLE)
    target_link_options(kruptos PRIVATE -static)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME kruptos-selftest COMMAND kruptos selftest)
endif()

install(TARGETS kruptos RUNTIME DESTINATION bin)
