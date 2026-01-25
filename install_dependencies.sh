#!/bin/bash
set -e

echo "=== CarOS Dependency Installer ==="
echo "Aktualisiere Paketquellen..."
sudo apt-get update

echo "Installiere Bibliotheken..."
# libgtk-4-dev: GTK4 Header & Libs
# libgpiod-dev: GPIO Zugriff (inkl. C++ Bindings auf Debian/Raspbian)
# libgstreamer...: Multimedia Framework
# libcurl4-openssl-dev: HTTP Client
sudo apt-get install -y \
    build-essential \
    pkg-config \
    libgtk-4-dev \
    libgpiod-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-good \
    libcurl4-openssl-dev

echo "Prüfe pkg-config..."
if pkg-config --exists gtk4 libgpiodcxx gstreamer-1.0 libcurl; then
    echo "  [OK] Alle Bibliotheken gefunden!"
else
    echo "  [FEHLER] Einige Bibliotheken fehlen noch."
    exit 1
fi