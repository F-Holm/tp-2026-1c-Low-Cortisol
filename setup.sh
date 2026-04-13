#!/bin/bash
set -e

echo "Instalando dependencias del sistema..."
sudo apt update
sudo apt install -y \
    git \
    build-essential \
    libreadline-dev \
    clang-format \
    gdb \
    valgrind

# CSpec
echo "Instalando CSpec..."
git clone https://github.com/mumuki/cspec.git /tmp/cspec
sudo make -C /tmp/cspec install
sudo rm -rf /tmp/cspec

# Commons Library For C
echo "Instalando so-commons-library..."
git clone https://github.com/sisoputnfrba/so-commons-library.git /tmp/so-commons-library
sudo make -C /tmp/so-commons-library install
sudo rm -rf /tmp/so-commons-library

# Update cache
echo "Actualizando caché de librerías (ldconfig)..."
sudo ldconfig

echo "¡Configuración completada con éxito!"
