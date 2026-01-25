#ifndef GPS_HANDLER_HPP
#define GPS_HANDLER_HPP

#include <gps.h>
#include <gtk/gtk.h>
#include <iostream>
#include <thread>
#include <atomic>

struct GPSData {
    double latitude;
    double longitude;
    double speed;
    int satellites;
    bool fix;
};

class GPSManager {
public:
    GPSManager() : running(false) {}
    
    void start() {
        running = true;
        worker_thread = std::thread(&GPSManager::update_loop, this);
    }

    GPSData get_latest_data() {
        return last_data.load();
    }

private:
    std::atomic<bool> running;
    std::thread worker_thread;
    std::atomic<GPSData> last_data;

    void update_loop() {
        struct gps_data_t gps_data;
        
        // Verbindung zum lokalen gpsd-Daemon
        if (gps_open("localhost", DEFAULT_GPSD_PORT, &gps_data) != 0) {
            std::cerr << "GPS Fehler: gpsd nicht erreichbar." << std::endl;
            return;
        }

        gps_stream(&gps_data, WATCH_ENABLE | WATCH_JSON, NULL);

        while (running) {
            if (gps_waiting(&gps_data, 1000000)) { // 1 Sekunde Timeout
                if (gps_read(&gps_data, NULL, 0) != -1) {
                    GPSData current;
                    current.fix = (gps_data.fix.mode >= MODE_2D);
                    if (current.fix) {
                        current.latitude = gps_data.fix.latitude;
                        current.longitude = gps_data.fix.longitude;
                        current.speed = gps_data.fix.speed * 3.6; // m/s in km/h
                        current.satellites = gps_data.satellites_used;
                    }
                    last_data.store(current);
                }
            }
        }
        gps_stream(&gps_data, WATCH_DISABLE, NULL);
        gps_close(&gps_data);
    }
};

#endif