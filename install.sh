#!/usr/bin/env sh
# Instalador portable de Kruptos para Linux.
#
# Copia el binario a ~/.local/bin/kruptos. No requiere root ni dependencias.
#
#   ./install.sh
#   ./install.sh --prefix /usr/local/bin     (normalmente con sudo)
#   ./install.sh --uninstall
set -eu
cd "$(dirname "$0")"

prefix="${HOME}/.local/bin"
uninstall=0

while [ $# -gt 0 ]; do
    case "$1" in
        --uninstall)
            uninstall=1
            ;;
        --prefix)
            shift
            if [ $# -eq 0 ]; then
                echo "Falta el directorio para --prefix"
                exit 2
            fi
            prefix="$1"
            ;;
        -h|--help)
            echo "Uso: $0 [--prefix DIR] [--uninstall]"
            exit 0
            ;;
        *)
            echo "Opcion desconocida: $1"
            exit 2
            ;;
    esac
    shift
done

target="$prefix/kruptos"

if [ "$uninstall" -eq 1 ]; then
    rm -f "$target"
    echo "Kruptos desinstalado de $prefix"
    exit 0
fi

if [ -f ./kruptos ]; then
    src=./kruptos
elif [ -f build-static/kruptos ]; then
    src=build-static/kruptos
elif [ -f build/kruptos ]; then
    src=build/kruptos
else
    echo "No se encontro el binario. Compila con ./build.sh --static"
    echo "o usa ./publish-linux.sh."
    exit 1
fi

mkdir -p "$prefix"
cp "$src" "$target"
chmod +x "$target"

echo "Instalado: $target"

case ":${PATH}:" in
    *":${prefix}:"*)
        echo "Comando disponible: kruptos --help"
        ;;
    *)
        echo "Anade $prefix al PATH. Por ejemplo:"
        echo "  echo 'export PATH=\"\$HOME/.local/bin:\$PATH\"' >> ~/.profile"
        echo "Anade esa linea a tu ~/.profile, ~/.bashrc o ~/.zshrc."
        echo "Abre una terminal nueva y escribe: kruptos --help"
        ;;
esac
