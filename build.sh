#!/usr/bin/env sh
# Compila Kruptos con CMake.
#
#   ./build.sh            Binario normal (dinamico)
#   ./build.sh --static   Binario estatico: no necesita librerias en el equipo destino
set -eu
cd "$(dirname "$0")"

build_dir="build"
cmake_args="-DCMAKE_BUILD_TYPE=Release"

case "${1:-}" in
    --static)
        build_dir="build-static"
        cmake_args="$cmake_args -DKRUPTOS_STATIC=ON"
        echo "Compilando binario estatico (si falla, faltan las librerias estaticas"
        echo "del compilador; solo hacen falta para compilar, no para ejecutar)."
        ;;
    "")
        ;;
    *)
        echo "Uso: $0 [--static]"
        exit 2
        ;;
esac

cmake -S . -B "$build_dir" $cmake_args
cmake --build "$build_dir" --parallel

echo
echo "Listo: $(pwd)/$build_dir/kruptos"
echo "Dependencias:"
ldd "$build_dir/kruptos" 2>&1 || true
