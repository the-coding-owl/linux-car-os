# Anleitung: libgpiod v2 auf Raspberry Pi OS installieren

Das Standard-Raspberry Pi OS (Bookworm/Bullseye) liefert aktuell noch `libgpiod` in der Version 1.x aus. Da **CarOS** die moderne C++ API der Version 2.x verwendet, muss diese Bibliothek manuell kompiliert werden.

Folge diesen Schritten, um `libgpiod v2` zu installieren.

## 1. Abhängigkeiten installieren

Zuerst müssen die Build-Tools und notwendigen Header installiert werden:

```bash
sudo apt-get update
sudo apt-get install -y \
    git \
    autoconf \
    autoconf-archive \
    libtool \
    pkg-config \
    build-essential \
    python3-dev
```

## 2. Quellcode herunterladen

Wir klonen das offizielle Repository und wechseln auf die Version 2.1 (oder neuer):

```bash
cd ~
git clone https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git
cd libgpiod
# Wir nutzen hier v2.1, da dies eine stabile v2-Version ist
git checkout v2.1
```

## 3. Kompilieren und Installieren

Wichtig ist hier das Flag `--enable-bindings-cxx`, damit die C++ Header (`gpiod.hpp`) erstellt werden.

```bash
./autogen.sh --enable-bindings-cxx --prefix=/usr/local
make -j$(nproc)
sudo make install
sudo ldconfig
```

## 4. Installation prüfen

Überprüfe, ob `pkg-config` die neue Version findet:

```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
pkg-config --modversion libgpiodcxx
```

Die Ausgabe sollte `2.1.0` (oder ähnlich) sein. Wenn das klappt, kannst du **CarOS** ganz normal mit `make` kompilieren.