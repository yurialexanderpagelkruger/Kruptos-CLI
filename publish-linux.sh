#!/usr/bin/env sh
# Crea una distribucion portable de Kruptos para Linux en dist/linux-<arch>/.
# El binario es estatico: no necesita librerias adicionales en el equipo destino.
set -eu
cd "$(dirname "$0")"

./build.sh --static

arch="$(uname -m)"
dist="dist/linux-$arch"

mkdir -p "$dist"
cp build-static/kruptos "$dist/kruptos"
cp install.sh "$dist/install.sh"
chmod +x "$dist/kruptos" "$dist/install.sh"

cat > "$dist/LEEME.txt" <<'EOF'
Kruptos - cifrado multicapa adaptativo
======================================

1. Ejecuta: ./install.sh
2. Abre una terminal nueva
3. Escribe: kruptos --help

El binario es estatico: no necesita librerias adicionales ni permisos de
administrador. Si ~/.local/bin no esta en PATH, el instalador muestra la linea
que debes anadir a tu perfil.

Ejemplos:
  kruptos encrypt -m "Hola mundo"
  kruptos decrypt -i "KR1-..." -m "..."
  kruptos --lang pt --help

Desinstalar:
  ./install.sh --uninstall
EOF

package="Kruptos-linux-$arch.tar.gz"
tar -czf "$package" -C "$dist" .

echo
echo "Distribucion: $dist"
echo "Paquete: $package"
echo "Dependencias del binario:"
ldd "$dist/kruptos" 2>&1 || true
