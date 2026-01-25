## 1. System-Architektur & UI Design

Da das System direkt nach dem Booten die Anwendung startet, fungiert deine C++ App quasi als "Kiosk-Shell".

* **UI-Framework:** GTK4 (bietet bessere Performance durch GPU-Beschleunigung als GTK3).
* **Layout:** Ein statisches Top-Bar (Uhrzeit, Status, Lautstärke) und eine Bottom-Bar (Home, Back, Schnellzugriff) mit einem variablen Content-Bereich in der Mitte.
* **Touch-Optimierung:** Nutzung von `GtkListView` für Medienlisten und große `GtkButton`-Elemente mit CSS-Styling für großzügige Paddings.

---

## 2. Funktions-Module & Integration

### Medien & Radio

* **DAB+ Radio:** Da das Modul per USB kommt, ist die Bibliothek **welle.io** (bzw. die zugrunde liegende `libremote-tcp`) oder direkt `rtl-sdr` der Standard. Du kannst einen C++ Wrapper bauen, der die Signalstärke und RDS-Daten ausliest.
* **Mediaplayer:** Integration von **GStreamer**. Es ist das Rückgrat für Audio/Video unter Linux und lässt sich exzellent in GTK-Applikationen einbinden.

### Konnektivität

* **Bluetooth:** Verwendung von **BlueZ** über D-Bus. Du musst Profile für A2DP (Audio-Streaming) und HFP (Telefonie) implementieren.
* **Smartphone-Integration:** Das ist der schwierigste Teil ohne Emulator.
* **Apple CarPlay/Android Auto:** Hierfür gibt es keine legalen Open-Source "Native Apps". Die Lösung ist meist **OpenAuto Pro** (kommerziell) oder die Nutzung von Projekten wie **AASDK**. Eine Alternative ist das Spiegeln via **Scrcpy** (für Android), was aber manuelles Setup erfordert.



### Hardware-Steuerung (GPIO & Drehknöpfe)

* **Bibliothek:** `libgpiod` (der moderne Linux-Standard) oder `WiringPi`.
* **Event-Loop:** Die GPIO-Abfrage sollte in einem eigenen Thread laufen und Signale an den GTK-Hauptthread senden (`g_idle_add`), um die UI zu aktualisieren.

---

## 3. Entwicklungsphasen

### Phase 1: Boot-Konfiguration & Base System

* Installation von Raspbian Lite (ohne Desktop).
* Konfiguration von `autologin` in der Konsole.
* Einrichtung von **X11** oder **Wayland** (Sway/Wayfire empfohlen für bessere Performance auf Pi 3).
* Skripting des Autostarts, sodass deine GTK-App direkt die Hardware-Ressourcen übernimmt.

### Phase 2: Core-UI & GPIO Integration

* Erstellung des Main-Windows in C++/GTK.
* Implementierung des CSS-Designs (Dark Mode, kontrastreich).
* Mapping der physischen Buttons (GPIO) auf GTK-Signale (z.B. Drehknopf-Event löst "Volume Up" in der App aus).

### Phase 3: Multimedia & Radio

* Anbindung des DAB+ USB-Moduls.
* Entwicklung des Media-Browsers (Einlesen von USB-Sticks via `GIO`).
* GStreamer-Backend für die Audiowiedergabe und Lautstärkeregelung (ALSA/PipeWire).

### Phase 4: Konnektivität & Feinschliff

* D-Bus Schnittstelle für Bluetooth-Kopplung und Titelanzeige vom Handy.
* Integration einer Offline-Navigation (z.B. **Navit** oder **7Ways**), die als Sub-Prozess in das GTK-Fenster eingebettet wird (XReparenting).
* Stresstests: Hitzeentwicklung des Pi 3B im Gehäuse prüfen.

---

## 4. Hardware-Mapping (Beispiel)

| Komponente | Anschluss | Linux-Schnittstelle |
| --- | --- | --- |
| DAB+ Modul | USB | `libusb` / `rtl-sdr` |
| Drehknopf (Encoder) | GPIO (A/B Phase) | `libgpiod` (Interrup-gesteuert) |
| Display | HDMI / DSI | DRM/KMS Treiber |
| Audio-Out | Klinke / I2S DAC | ALSA / PipeWire |

---

## Nächste Schritte

Damit der Pi 3B nicht überhitzt oder ruckelt, sollten wir das UI so schlank wie möglich halten.

---
---

Das ist ein hervorragender Startpunkt. Da wir uns auf C++, GTK und den Raspberry Pi 3B festgelegt haben, müssen wir besonders auf die Performance achten. Der Pi 3B hat begrenzte Ressourcen, daher ist eine effiziente Event-Verarbeitung und sauberes Rendering entscheidend.

Hier ist die detaillierte Ausarbeitung für **Phase 1** und der Einstieg in **Phase 2**.

---

## Phase 1: Das Fundament (OS & Display)

Bevor wir Code schreiben, muss das System so konfiguriert werden, dass es wie eine "Embedded Appliance" wirkt.

1. **OS-Wahl:** Nutze **Raspberry Pi OS Lite (64-bit)**. Wir brauchen keinen Desktop-Ballast (LXDE/Pixel).
2. **Display Server:** Da du GTK nutzt, ist **Wayland** (via `cage` oder `sway`) die modernste Wahl für den Pi. `cage` ist ein "Kiosk-Compositor", der genau eine Anwendung im Vollbild ausführt.
3. **Boot-Optimierung:** * Deaktiviere den Boot-Splash zugunsten eines eigenen Logos (Plymouth).
* Editiere `/boot/config.txt`, um die GPU-Beschleunigung zu erzwingen (`dtoverlay=vc4-kms-v3d`).



---

## Phase 2: Die Software-Architektur (C++ & GTK)

Um die GPIO-Buttons und das UI flüssig zu verbinden, nutzen wir ein **Model-View-Controller (MVC)** Muster.

### 1. Das Hardware-Interface (GPIO)

Wir verwenden `libgpiod`. Da Drehknöpfe (Rotary Encoder) sehr schnell feuern, sollten diese in einem separaten Thread überwacht werden, um das UI nicht zu blockieren.

### 2. Der C++ Code-Grundbau

Hier ist ein struktureller Vorschlag, wie du die Hardware-Events in die GTK-Hauptschleife bringst:

```cpp
#include <gtk/gtk.h>
#include <iostream>
#include <thread>

// Struktur für unseren App-Status
class CarApp {
public:
    GtkWidget *window;
    GtkWidget *label_volume;
    int volume = 20;

    void update_ui_volume() {
        std::string s = "Volume: " + std::to_string(volume);
        gtk_label_set_text(GTK_LABEL(label_volume), s.c_str());
    }
};

// Diese Funktion wird vom GPIO-Thread aufgerufen
static gboolean on_gpio_scroll(gpointer data) {
    CarApp *app = (CarApp*)data;
    app->volume++;
    app->update_ui_volume();
    return FALSE; // Nur einmal ausführen pro Signal
}

void gpio_thread_func(CarApp *app) {
    // Hier würde die libgpiod Logik stehen
    // Wenn Drehknopf gedreht wird:
    g_idle_add(on_gpio_scroll, app); 
}

```

### 3. Das UI-Konzept (Theming)

GTK sieht standardmäßig nach "Desktop" aus. Mit **CSS** biegen wir das auf "Car-Interface" um.

* **Große Touch-Zonen:** Nutze `min-width` und `min-height` in CSS für Buttons (mind. 80x80 Pixel).
* **Keine Cursor:** In den Einstellungen des Display Managers deaktivieren wir den Mauszeiger.
* **Struktur:**
* **Header:** Uhrzeit, Außentemperatur (via I2C Sensor?), BT-Status.
* **Main:** `GtkStack` – das ist das wichtigste Widget. Es erlaubt das Umschalten zwischen Radio, Media und Navi ohne Fensterwechsel.
* **Footer:** Home, Lautstärke-Indikator, Schnellwahltasten.



---

## Strategie für die "Sorgenkinder"

### Navigation

Da wir nativ bleiben: **Navit** ist das Urgestein der Open-Source-Navigation. Es ist extrem ressourcensparend. Du kannst Navit über ein `GtkSocket` / `GtkPlug` (unter X11) oder als Overlay-Fenster (unter Wayland) einbinden. Es nutzt OpenStreetMap-Daten offline.

### Smartphone-Anbindung (Native-Ansatz)

Da Emulatoren rausfallen:

1. **Android:** Nutze `scrcpy` über USB. Es ist kein Emulator, sondern ein Streamer. Du kannst den Output in ein GTK-Window leiten.
2. **Audio:** Das Handy wird primär als **Bluetooth-Quelle** behandelt. Mit `BlueZ` und `PipeWire` kannst du die Metadaten (Interpret/Titel) abgreifen und in deinem GTK-UI anzeigen.

---
---

Sehr gut. Damit legen wir das Fundament für ein performantes System. Wir nutzen **CMake** als Build-System, da es der Standard für C++ Projekte ist und die Einbindung von GTK sowie anderen Bibliotheken (wie `libgpiod`) sehr einfach macht.

Hier ist das Grundgerüst für dein **Linux Car OS**.

### 1. Die Projektstruktur

Erstelle einen Ordner für dein Projekt mit folgender Struktur:

```text
CarOS/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── ui_manager.cpp
│   └── gpio_handler.cpp
├── include/
│   ├── ui_manager.hpp
│   └── gpio_handler.hpp
└── assets/
    └── style.css

```

---

### 2. Das CMake-Setup (`CMakeLists.txt`)

Dieses Skript sucht nach den GTK4-Bibliotheken und verlinkt sie mit deinem Code.

```cmake
cmake_minimum_required(VERSION 3.10)
project(CarOS)

set(CMAKE_CXX_STANDARD 17)

# GTK4 finden
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)

# Header und Quellen
include_directories(include ${GTK4_INCLUDE_DIRS})
link_directories(${GTK4_LIBRARY_DIRS})

add_executable(CarOS 
    src/main.cpp 
    src/ui_manager.cpp 
    src/gpio_handler.cpp
)

target_link_libraries(CarOS ${GTK4_LIBRARIES} pthread gpiod)

```

---

### 3. Das UI-Grundgerüst (C++)

Wir nutzen einen `GtkStack`, um flüssig zwischen den Modulen (Radio, Media, Navi) hin- und herzuwechseln. Das ist performanter als ständig neue Fenster zu öffnen.

**`src/ui_manager.cpp` (Auszug):**

```cpp
#include "ui_manager.hpp"

GtkWidget* create_main_ui() {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

    // 1. Radio Modul (Beispiel)
    GtkWidget *radio_label = gtk_label_new("DAB+ Radio");
    gtk_stack_add_titled(GTK_STACK(stack), radio_label, "radio", "Radio");

    // 2. Media Modul
    GtkWidget *media_label = gtk_label_new("Media Player");
    gtk_stack_add_titled(GTK_STACK(stack), media_label, "media", "Media");

    // Bottom Navigation Bar
    GtkWidget *nav_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(nav_bar, GTK_ALIGN_CENTER);
    
    // Buttons für den Stack-Wechsel
    GtkWidget *btn_radio = gtk_button_new_with_label("Radio");
    g_signal_connect_swapped(btn_radio, "clicked", G_CALLBACK(gtk_stack_set_visible_child_name), stack, (gpointer)"radio");
    
    gtk_box_append(GTK_BOX(nav_bar), btn_radio);
    gtk_box_append(GTK_BOX(box), stack);
    gtk_box_append(GTK_BOX(box), nav_bar);

    return box;
}

```

---

### 4. Das "Car-Look" Styling (`assets/style.css`)

Damit die Oberfläche auf einem 7-Zoll Touchscreen bedienbar ist, erzwingen wir große Abstände und dunkle Farben (Nachtmodus-Standard).

```css
window {
    background-color: #121212;
    color: #ffffff;
}

button {
    min-width: 100px;
    min-height: 80px;
    margin: 10px;
    background-image: none;
    background-color: #2c2c2c;
    border-radius: 12px;
    font-size: 18px;
    font-weight: bold;
}

button:active {
    background-color: #007bff;
}

label {
    font-size: 24px;
}

```

---

### 5. Integration der Hardware-Buttons

Für die Drehknöpfe (Encoder) am Pi 3B empfehle ich, einen Listener-Thread zu starten, der `libgpiod` nutzt. Da Drehimpulse sehr schnell kommen, darf dies niemals im UI-Thread passieren.

**Vorgehensweise für die Buttons:**

1. **Polling/Interrupt:** Der GPIO-Thread wartet auf Flankenwechsel.
2. **Signal-Dispatch:** Bei einem "Lauter"-Event sendet der Thread via `g_idle_add()` einen Befehl an die UI, den Lautstärkebalken anzuzeigen.

---

### Phase 2: Nächste Schritte

Um dieses Grundgerüst auf deinem Pi 3B zum Laufen zu bringen, musst du:

1. Die Abhängigkeiten installieren: `sudo apt install libgtk-4-dev libgpiod-dev cmake g++`.
2. Die CSS-Datei in dein C++ Programm laden (`GtkCssProvider`).
3. Den Autostart über eine minimale `.xinitrc` oder einen Wayland-Compositor wie `cage` einrichten.

**Soll ich dir als nächstes zeigen, wie du die `style.css` im C++ Code korrekt lädst und wie wir die GPIO-Events der Drehknöpfe konkret verarbeiten?**