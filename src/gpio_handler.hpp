#ifndef GPIO_HANDLER_HPP
#define GPIO_HANDLER_HPP

#include <gpiod.hpp>
#include <gtk/gtk.h>
#include <chrono>
#include <iostream>
#include <functional>

using EncoderCallback = std::function<void(bool, gpointer)>;

inline void monitor_encoder(int pinA, int pinB, EncoderCallback callback, gpointer user_data) {
    try {
        // 1. Einstellungen für die Leitungen festlegen
        gpiod::line_settings settings_a;
        settings_a.set_direction(gpiod::line::direction::INPUT);
        settings_a.set_edge_detection(gpiod::line::edge::RISING);
        // Optional: Debounce hinzufügen für mechanische Encoder
        settings_a.set_debounce_period(std::chrono::milliseconds(5));

        gpiod::line_settings settings_b;
        settings_b.set_direction(gpiod::line::direction::INPUT);

        // 2. Line Config erstellen
        gpiod::line_config line_cfg;
        line_cfg.add_line_settings(static_cast<unsigned int>(pinA), settings_a);
        line_cfg.add_line_settings(static_cast<unsigned int>(pinB), settings_b);

        // 3. Request Config erstellen
        gpiod::request_config req_cfg;
        req_cfg.set_consumer("CarOS_Encoder");

        // 4. Chip öffnen und Request direkt absetzen
        // Bei libgpiod v2.x ist dies der sauberste Weg:
        auto request = gpiod::chip("/dev/gpiochip0").prepare_request()
                        .set_request_config(req_cfg)
                        .set_line_config(line_cfg)
                        .do_request();

        // Buffer für Events vorab allozieren (Performance)
        gpiod::edge_event_buffer buffer(16);

        while (true) {
            // Warten auf Events
            if (request.wait_edge_events(std::chrono::milliseconds(100))) {
                // Events in den Buffer lesen
                request.read_edge_events(buffer);
                
                for (const auto& event : buffer) {
                    if (event.line_offset() == static_cast<unsigned int>(pinA)) {
                        // Richtung prüfen über Pin B
                        auto val_b = request.get_value(static_cast<unsigned int>(pinB));
                        
                        bool clockwise = (val_b == gpiod::line::value::ACTIVE);
                        
                        // Callback in den GTK Main-Loop schieben (Thread-Safety!)
                        struct CallbackWrapper {
                            EncoderCallback cb;
                            bool cw;
                            gpointer data;
                        };
                        auto* wrapper = new CallbackWrapper{callback, clockwise, user_data};
                        
                        g_idle_add([](gpointer d) -> gboolean {
                            auto* w = static_cast<CallbackWrapper*>(d);
                            w->cb(w->cw, w->data);
                            delete w;
                            return FALSE;
                        }, wrapper);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "GPIO Fehler: " << e.what() << std::endl;
    }
}

#endif