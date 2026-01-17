
# CarOS - Automotive Infotainment System

**CarOS** ist ein leichtgewichtiges, GTK4-basiertes Infotainment-System für Embedded-Geräte (z. B. Raspberry Pi mit Touchscreen). Es bietet eine moderne Radio-Schnittstelle mit automatischem Sender-Import, lokaler Bild-Caching-Logik und GStreamer-Integration.

## Features

* **Smart Radio:** Automatischer Import der Top 100 deutschen Radiosender via Radio-Browser API.
* **Logo Caching:** Alle Senderlogos werden lokal unter `assets/logos/` gespeichert, um Datenvolumen zu sparen und die Performance zu steigern.
* **3-Spalten-Layout:** Optimierte Benutzeroberfläche für Touch-Bedienung mit großen Kacheln.
* **GStreamer Backend:** Hochwertige Audiowiedergabe mit Metadaten-Extraktion.
* **Bluetooth Management:** (In Entwicklung) Schnittstelle zur Verwaltung gekoppelter Mobilgeräte.

## Architektur

Die App folgt einem modularen Aufbau, um Hardware-Abstraktion (GPIOs) und UI-Logik zu trennen:

* `main.cpp`: Einstiegspunkt und UI-Layout.
* `radio_manager.hpp/cpp`: Steuerung der GStreamer-Pipeline.
* `bluetooth_manager.hpp/cpp`: Integration von BlueZ für die Geräteverwaltung.
* `assets/stations.csv`: Lokale Datenbank für deine Senderliste.

## Dateistruktur

* **/assets**: Enthält `style.css`, die Senderliste `stations.csv` und den Logo-Cache.
* **/src**: Quellcodedateien (`main.cpp`, Manager-Klassen).
* **/bin**: Ausführbare Binärdateien.

## Konfiguration

Die Senderliste wird automatisch beim ersten Klick auf den **Seeding-Button** (Download-Icon) erstellt. Die Daten werden von `all.api.radio-browser.info` bezogen.

## Lizenz

Dieses Projekt ist unter der MIT-Lizenz lizenziert.
