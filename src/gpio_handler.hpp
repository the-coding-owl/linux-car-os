#ifndef GPIO_HANDLER_HPP
#define GPIO_HANDLER_HPP

#include <gpiod.hpp>
#include <gtk/gtk.h>
#include <chrono>
#include <thread>
#include <vector>

inline void monitor_encoder(int pinA, int pinB, gpointer user_data) {
    try {
        // Chip öffnen
        gpiod::chip chip("/dev/gpiochip0");

        // 1. Line Einstellungen (v2)
        gpiod::line_settings settings;
        settings.set_direction(gpiod::line::direction::INPUT);
        settings.set_edge_detection(gpiod::line::edge::BOTH);

        // 2. Line Konfiguration (Offsets explizit casten)
        gpiod::line_config line_cfg;
        std::vector<gpiod::line::offset> offsets = {
            static_cast<gpiod::line::offset>(pinA), 
            static_cast<gpiod::line::offset>(pinB)
        };
        line_cfg.add_line_settings(offsets, settings);

        // 3. Request Konfiguration
        gpiod::request_config req_cfg;
        req_cfg.set_consumer("CarOS_Encoder");

        // 4. Der Request-Aufruf (Builder-Style für Arch Linux / libgpiod v2)
        // Wir nutzen den Chip, um die konfigurierten Lines zu binden
        auto request = chip.prepare_config()
                           .set_request_config(req_cfg)
                           .set_line_config(line_cfg)
                           .request_lines();

        while (true) {
            // Prüfung auf Events
            if (request.wait_edge_events(std::chrono::milliseconds(100))) {
                auto events = request.read_edge_events();
                // Hier Logik für Drehknopf implementieren
            }
        }
    } catch (const std::exception& e) {
        g_printerr("GPIO Fehler: %s\n", e.what());
    }
}

#endif