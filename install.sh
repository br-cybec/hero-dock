#!/bin/bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

echo "╔══════════════════════════════════════════╗"
echo "║   QDock v5 — Dock moderno para Linux     ║"
echo "╚══════════════════════════════════════════╝"
echo ""

echo "[1/4] Verificando dependencias..."

# Qt6
if ! pkg-config --exists Qt6Core 2>/dev/null; then
    echo "  ERROR: Qt6 no encontrado."
    echo ""
    echo "  Instalar con:"
    echo "    Ubuntu/Debian: sudo apt install qt6-base-dev qt6-base-private-dev \\"
    echo "                       libx11-dev cmake build-essential"
    echo "    Fedora:        sudo dnf install qt6-qtbase-devel qt6-qtbase-private-devel \\"
    echo "                       libX11-devel cmake gcc-c++"
    echo "    Arch:          sudo pacman -S qt6-base libx11 cmake base-devel"
    exit 1
fi
echo "  ✓ Qt6 encontrado"

# udisks2 (para detección de dispositivos)
if gdbus introspect --system --dest org.freedesktop.UDisks2 \
   --object-path /org/freedesktop/UDisks2 &>/dev/null; then
    echo "  ✓ UDisks2 encontrado — detección completa de dispositivos"
else
    echo "  ⚠ UDisks2 no disponible. Instalar para soporte completo de USB/disco:"
    echo "    Ubuntu/Debian: sudo apt install udisks2"
    echo "    El dock usará modo básico (/proc/mounts)"
fi

# nmcli (para panel de redes)
if command -v nmcli &>/dev/null; then
    echo "  ✓ nmcli encontrado — panel de redes completo"
else
    echo "  ⚠ nmcli no encontrado. Instalar para el panel de redes:"
    echo "    Ubuntu/Debian: sudo apt install network-manager"
fi

echo ""
echo "[2/4] Configurando compilación..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release

echo ""
echo "[3/4] Compilando..."
make -j$(nproc)

echo ""
echo "[4/4] Instalando..."
if [ "$EUID" -ne 0 ]; then
    sudo make install
else
    make install
fi

# Autostart
mkdir -p "$HOME/.config/autostart"
cp "$SCRIPT_DIR/qdock.desktop" "$HOME/.config/autostart/"

echo ""
echo "╔══════════════════════════════════════════╗"
echo "║   ✓ Instalado correctamente              ║"
echo "║                                          ║"
echo "║   Ejecutar:  qdock                       ║"
echo "║   Autostart: ~/.config/autostart/        ║"
echo "╚══════════════════════════════════════════╝"
