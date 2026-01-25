# --- Projektkonfiguration ---
TARGET = bin/CarOS
SRC_DIR = src
BIN_DIR = bin
ASSETS_DIR = assets

# Compiler Einstellungen
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra `pkg-config --cflags gtk4 libgpiodcxx gstreamer-1.0`
LIBS = `pkg-config --libs gtk4 libgpiodcxx gstreamer-1.0` -lcurl -lgps -pthread -latomic

# --- Abhängigkeiten prüfen ---
# Diese Liste entspricht den pkg-config Namen
REQUIRED_PKGS = gtk4 libgpiodcxx gstreamer-1.0 libcurl

.PHONY: all clean check_deps directories

all: check_deps directories $(TARGET)

# Prüft, ob alle pkg-config Pakete installiert sind
check_deps:
	@for pkg in $(REQUIRED_PKGS); do \
		pkg-config --exists $$pkg || (echo "Fehler: Paket $$pkg nicht gefunden! Nutze 'yay -S' zum Installieren." && exit 1); \
	done
	@ld -lgps --verbose >/dev/null 2>&1 || (echo "Fehler: libgps nicht gefunden! Installiere 'gpsd'." && exit 1)

# Verzeichnisse erstellen
directories:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(ASSETS_DIR)/logos
	@mkdir -p $(ASSETS_DIR)/icons

# Kompilierung
$(TARGET): $(SRC_DIR)/main.cpp
	@echo "🔨 Kompiliere $(TARGET)..."
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)
	@echo "✅ Fertig! Starte die App mit: ./$(TARGET)"

# Aufräumen
clean:
	@echo "🧹 Räume auf..."
	rm -rf $(BIN_DIR)/*