#!/bin/bash
set -e

echo "=== CarOS Dependency Installer (Arch Linux) ==="
echo "Aktualisiere Paketdatenbank..."
sudo pacman -Sy

echo "Installiere Bibliotheken..."
# base-devel: Compiler & Tools (gcc, make, pkg-config via pkgconf)
# gtk4: GTK4 Library & Headers
# libgpiod: GPIO Library (Achtung: Arch liefert v2.x)
# gstreamer: Core Multimedia Framework
# gst-plugins-base: Basis-Plugins & Libs
# gst-plugins-good: Gute Plugins (MP3, etc.)
# curl: HTTP Client Library
sudo pacman -S --needed --noconfirm \
    base-devel \
    pkgconf \
    gtk4 \
    libgpiod \
    gstreamer \
    gst-plugins-base \
    gst-plugins-good \
    curl

echo "Prüfe pkg-config..."
if pkg-config --exists gtk4 libgpiodcxx gstreamer-1.0 libcurl; then
    echo "  [OK] Alle Bibliotheken gefunden!"
else
    echo "  [FEHLER] Einige Bibliotheken fehlen noch."
    exit 1
fi