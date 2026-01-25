#include <gtk/gtk.h>
#include <iostream>
#include <thread>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
#include <curl/curl.h>
#include <algorithm>

#include "ui_manager.hpp"
#include "bluetooth_manager.hpp"
#include "radio_manager.hpp"
#include "gpio_handler.hpp"
#include "virtual_keyboard.hpp"
#include "gps_handler.hpp"

// Prototypen
void refresh_radio_list(GtkWidget *flowbox, RadioManager *radio_mgr);
void perform_seeding(GtkWidget *flowbox, RadioManager *radio_mgr);

// --- Datenstrukturen ---
struct RadioStation {
    std::string name;
    std::string url;
    std::string logo_path; // Lokaler Pfad zum Cache
};

struct AppWidgets {
    GtkWidget *stack;
    GtkWidget *volume_label;
    RadioManager *radio_mgr;
    GPSManager *gps_mgr;
    int current_volume = 50;
    GtkWidget *keyboard_revealer;
    VirtualKeyboard *keyboard;
};

struct SaveData {
    GtkEntry *name_entry;
    GtkEntry *url_entry;
    GtkWidget *flowbox;
    RadioManager *radio_mgr;
    GtkPopover *popover;
    AppWidgets* widgets;
};

// Callback für CURL
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// --- Hilfsfunktionen ---

// Lädt Stationen aus der CSV (Name;URL;LogoPath)
std::vector<RadioStation> load_stations() {
    std::vector<RadioStation> list;
    std::ifstream file("assets/stations.csv");
    std::string line;
    if (file.is_open()) {
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string name, url, logo;
            if (std::getline(ss, name, ';') && 
                std::getline(ss, url, ';') && 
                std::getline(ss, logo, ';')) {
                list.push_back({name, url, logo});
            }
        }
        file.close();
    }
    // Fallback falls Datei leer
    if (list.empty()) {
        list.push_back({"Rock Antenne", "https://stream.rockantenne.de/rockantenne/stream/mp3", "assets/logos/default.png"});
    }
    return list;
}

void save_station(const std::string& name, const std::string& url, const std::string& logo = "assets/logos/default.png") {
    std::ofstream file("assets/stations.csv", std::ios::app);
    if (file.is_open()) { 
        file << name << ";" << url << ";" << logo << "\n"; 
        file.close(); 
    }
}

void delete_station(const std::string& name_to_delete) {
    auto stations = load_stations();
    std::ofstream file("assets/stations.csv", std::ios::trunc);
    if (file.is_open()) {
        for (const auto& s : stations) {
            if (s.name != name_to_delete) {
                file << s.name << ";" << s.url << ";" << s.logo_path << "\n";
            }
        }
        file.close();
    }
}

static gboolean update_clock_label(gpointer user_data) {
    time_t now = time(nullptr);
    struct tm *lt = localtime(&now);
    char buf[10];
    strftime(buf, sizeof(buf), "%H:%M", lt);
    gtk_label_set_text(GTK_LABEL(user_data), buf);
    return G_SOURCE_CONTINUE;
}

GtkWidget* create_icon_button(const char* icon_name, const char* label_text) {
    GtkWidget *btn = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(box), gtk_image_new_from_icon_name(icon_name));
    gtk_box_append(GTK_BOX(box), gtk_label_new(label_text));
    gtk_button_set_child(GTK_BUTTON(btn), box);
    gtk_widget_add_css_class(btn, "glass-button");
    return btn;
}

GtkWidget* create_nav_button(const char* icon_path) {
    GtkWidget *btn = gtk_button_new();
    // Bild aus Datei laden (deine neuen monochromen Icons)
    GtkWidget *icon = gtk_image_new_from_file(icon_path);
    
    // Icon-Größe festlegen (z.B. 32px für eine flache Leiste)
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
    
    gtk_button_set_child(GTK_BUTTON(btn), icon);
    gtk_widget_add_css_class(btn, "nav-button");
    return btn;
}

// --- Hauptfunktionen ---

void refresh_radio_list(GtkWidget *flowbox, RadioManager *radio_mgr) {
    // 1. Bestehende Widgets entfernen
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(flowbox)) != nullptr) {
        gtk_flow_box_remove(GTK_FLOW_BOX(flowbox), child);
    }

    // 2. Liste neu aufbauen
    for (const auto& s : load_stations()) {
        // ORIENTATION_VERTICAL: Packt Logo, Name und Button untereinander
        GtkWidget *item_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_add_css_class(item_box, "radio-item-card");
        gtk_widget_set_valign(item_box, GTK_ALIGN_START);

        // --- Logo (Zentral & Groß) ---
        GtkWidget *logo_img = gtk_image_new_from_file(s.logo_path.c_str());
        // Erhöhte Größe für bessere Sichtbarkeit (z.B. 120x120)
        gtk_widget_set_size_request(logo_img, 120, 120);
        gtk_widget_set_halign(logo_img, GTK_ALIGN_CENTER);
        gtk_widget_add_css_class(logo_img, "station-logo");

        // --- Play Button (Text unter dem Bild) ---
        GtkWidget *play_btn = gtk_button_new_with_label(s.name.c_str());
        GtkWidget *btn_label = gtk_button_get_child(GTK_BUTTON(play_btn));
        if (GTK_IS_LABEL(btn_label)) {
            gtk_label_set_ellipsize(GTK_LABEL(btn_label), PANGO_ELLIPSIZE_END);
            // Zentriert den Text unter dem Logo
            gtk_label_set_justify(GTK_LABEL(btn_label), GTK_JUSTIFY_CENTER);
            gtk_widget_set_halign(btn_label, GTK_ALIGN_CENTER);
        }
        gtk_widget_add_css_class(play_btn, "radio-play-btn");
        
        // Signal für Playback (bleibt gleich)
        g_object_set_data_full(G_OBJECT(play_btn), "station_url", g_strdup(s.url.c_str()), g_free);
        g_signal_connect(play_btn, "clicked", G_CALLBACK(+[](GtkWidget* w, gpointer data) {
            static_cast<RadioManager*>(data)->set_source((const char*)g_object_get_data(G_OBJECT(w), "station_url"));
        }), radio_mgr);

        // --- Delete Button ---
        // Wir platzieren ihn dezent am unteren Rand oder als Icon
        GtkWidget *del_btn = gtk_button_new_from_icon_name("edit-delete-symbolic");
        gtk_widget_add_css_class(del_btn, "radio-delete-btn");
        gtk_widget_set_halign(del_btn, GTK_ALIGN_CENTER); // Mittig unter dem Namen
        
        struct DelData { std::string name; GtkWidget* fb; RadioManager* rm; };
        DelData* dd = new DelData{s.name, flowbox, radio_mgr};
        g_signal_connect(del_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer data) {
            auto* d = static_cast<DelData*>(data);
            delete_station(d->name);
            refresh_radio_list(d->fb, d->rm);
            delete d;
        }), dd);

        // --- Aufbau der Kachel (Vertikal) ---
        gtk_box_append(GTK_BOX(item_box), logo_img);
        gtk_box_append(GTK_BOX(item_box), play_btn);
        gtk_box_append(GTK_BOX(item_box), del_btn);
        
        // In die Flowbox einfügen
        gtk_flow_box_insert(GTK_FLOW_BOX(flowbox), item_box, -1);
    }
}

bool download_image(const std::string& url, const std::string& destination) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    FILE* fp = fopen(destination.c_str(), "wb");
    if (!fp) { curl_easy_cleanup(curl); return false; }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CarOS-RadioApp/1.0");

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}

void perform_seeding(GtkWidget *flowbox, RadioManager *radio_mgr) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    std::string readBuffer;
    const char* api_url = "https://all.api.radio-browser.info/json/stations/search?country=Germany&limit=100&language=german&order=clicktrend&reverse=true";

    curl_easy_setopt(curl, CURLOPT_URL, api_url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "CarOS-RadioApp/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); 
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    std::cout << "Seeding: Starte API Download..." << std::endl;
    CURLcode res = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK && response_code == 200 && !readBuffer.empty()) {
        std::ofstream file("assets/stations.csv", std::ios::trunc);
        system("mkdir -p assets/logos");

        size_t pos = 0;
        int count = 0;
        while ((pos = readBuffer.find("\"name\":", pos)) != std::string::npos) {
            auto extract = [&](const std::string& key, size_t start_from) {
                size_t k_pos = readBuffer.find("\"" + key + "\":", start_from);
                if (k_pos == std::string::npos) return std::string("");
                size_t v_start = readBuffer.find("\"", k_pos + key.length() + 2) + 1;
                size_t v_end = readBuffer.find("\"", v_start);
                return readBuffer.substr(v_start, v_end - v_start);
            };

            std::string name = extract("name", pos);
            std::string url = extract("url_resolved", pos);
            std::string logoUrl = extract("favicon", pos);

            if (!name.empty() && !url.empty() && url.find("http") == 0) {
                size_t s_pos;
                while((s_pos = name.find(";")) != std::string::npos) name.replace(s_pos, 1, " ");
                
                std::string localLogoPath = "assets/logos/default.png";
                if (!logoUrl.empty() && logoUrl.find("http") == 0) {
                    std::string safeName = name;
                    safeName.erase(std::remove_if(safeName.begin(), safeName.end(), [](char c){ return !std::isalnum(c); }), safeName.end());
                    std::string target = "assets/logos/" + safeName + ".png";
                    if (download_image(logoUrl, target)) localLogoPath = target;
                }
                
                file << name << ";" << url << ";" << localLogoPath << "\n";
                count++;
            }
            pos = readBuffer.find("}", pos + 1);
            if (pos == std::string::npos) break;
        }
        file.close();
        std::cout << "Seeding abgeschlossen: " << count << " Sender." << std::endl;
        refresh_radio_list(flowbox, radio_mgr);
    }
}

// --- UI Erstellung ---
GtkWidget* create_navigation_page(GPSManager *gps_mgr) {
    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 30);
    gtk_widget_set_valign(nav_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(nav_box, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(nav_box, "navigation-container");

    // Icon für Navigation
    GtkWidget *nav_icon = gtk_image_new_from_file("assets/icons/navigation.png");
    gtk_image_set_pixel_size(GTK_IMAGE(nav_icon), 80);
    gtk_box_append(GTK_BOX(nav_box), nav_icon);

    GtkWidget *status_label = gtk_label_new("Warte auf GPS Fix...");
    gtk_widget_add_css_class(status_label, "radio-metadata"); // Nutzen wir vorhandenen Style
    gtk_box_append(GTK_BOX(nav_box), status_label);

    GtkWidget *detail_label = gtk_label_new("Verbinde mit NEO-6M...");
    gtk_box_append(GTK_BOX(nav_box), detail_label);

    // Timer zum UI-Update der GPS-Daten (1Hz)
    g_timeout_add_seconds(1, [](gpointer data) -> gboolean {
        auto* labels = static_cast<GtkWidget**>(data);
        GtkLabel* l_status = GTK_LABEL(labels[0]);
        GtkLabel* l_detail = GTK_LABEL(labels[1]);
        GPSManager* mgr = static_cast<GPSManager*>(g_object_get_data(G_OBJECT(l_status), "mgr"));

        GPSData d = mgr->get_latest_data();
        if (d.fix) {
            char buf_status[64];
            snprintf(buf_status, sizeof(buf_status), "%.1f km/h", d.speed);
            gtk_label_set_text(l_status, buf_status);

            char buf_detail[128];
            snprintf(buf_detail, sizeof(buf_detail), 
                     "Lat: %.5f | Lon: %.5f\nSats: %d", 
                     d.latitude, d.longitude, d.satellites);
            gtk_label_set_text(l_detail, buf_detail);
        } else {
            gtk_label_set_text(l_status, "Kein GPS Fix");
            gtk_label_set_text(l_detail, "Suche Satelliten...");
        }
        return G_SOURCE_CONTINUE;
    }, new GtkWidget*[2]{status_label, detail_label});

    // Manager an Widget binden für Zugriff im Timer
    g_object_set_data(G_OBJECT(status_label), "mgr", gps_mgr);

    return nav_box;
}

GtkWidget* create_radio_page(RadioManager **mgr_out, AppWidgets* widgets) {
    GtkWidget *radio_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(radio_scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(radio_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *radio_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_add_css_class(radio_box, "radio-container");

    GtkWidget *meta_label = gtk_label_new("Wähle einen Sender...");
    gtk_widget_add_css_class(meta_label, "radio-metadata");
    *mgr_out = new RadioManager(meta_label);

    GtkWidget *action_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(action_row, GTK_ALIGN_CENTER);

    GtkWidget *add_btn = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_add_css_class(add_btn, "glass-button");
    
    GtkWidget *seed_btn = gtk_button_new_from_icon_name("folder-download-symbolic");
    gtk_widget_add_css_class(seed_btn, "glass-button");
    gtk_widget_add_css_class(seed_btn, "seed-button");

    gtk_box_append(GTK_BOX(action_row), add_btn);
    gtk_box_append(GTK_BOX(action_row), seed_btn);

    GtkWidget *popover = gtk_popover_new();
    GtkWidget *form = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *e_name = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_name), "Sendername");
    GtkWidget *e_url = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_url), "Stream URL");
    GtkWidget *s_btn = gtk_button_new_with_label("Speichern");

    // Helper: Wenn Entry Fokus erhält, Tastatur-Ziel setzen
    auto connect_kb = [&](GtkWidget* entry) {
        GtkEventController *c = gtk_event_controller_focus_new();
        g_signal_connect(c, "enter", G_CALLBACK(+[](GtkEventControllerFocus* ctrl, gpointer data){
            AppWidgets* w = static_cast<AppWidgets*>(data);
            w->keyboard->set_target(GTK_EDITABLE(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(ctrl))));
            gtk_revealer_set_reveal_child(GTK_REVEALER(w->keyboard_revealer), TRUE);
        }), widgets);
        gtk_widget_add_controller(entry, c);
    };
    connect_kb(e_name);
    connect_kb(e_url);
    
    gtk_box_append(GTK_BOX(form), e_name); gtk_box_append(GTK_BOX(form), e_url); gtk_box_append(GTK_BOX(form), s_btn);
    
    gtk_popover_set_child(GTK_POPOVER(popover), form);
    gtk_widget_set_parent(popover, add_btn);

    GtkWidget *flowbox = gtk_flow_box_new();
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flowbox), 3);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flowbox), 3);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flowbox), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flowbox), 20); // Abstand zwischen Spalten
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flowbox), 20);    // Abstand zwischen Zeilen
    gtk_widget_set_valign(flowbox, GTK_ALIGN_START);

    SaveData *sd = new SaveData{GTK_ENTRY(e_name), GTK_ENTRY(e_url), flowbox, *mgr_out, GTK_POPOVER(popover), widgets};
    g_signal_connect(s_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer data) {
        auto* d = static_cast<SaveData*>(data);
        save_station(gtk_editable_get_text(GTK_EDITABLE(d->name_entry)), gtk_editable_get_text(GTK_EDITABLE(d->url_entry)));
        refresh_radio_list(d->flowbox, d->radio_mgr);
        gtk_revealer_set_reveal_child(GTK_REVEALER(d->widgets->keyboard_revealer), FALSE);
        gtk_popover_popdown(d->popover);
    }), sd);

    g_signal_connect(add_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer p) { gtk_popover_popup(GTK_POPOVER(p)); }), popover);

    struct SeedData { GtkWidget* fb; RadioManager* rm; };
    SeedData* sc = new SeedData{flowbox, *mgr_out};
    g_signal_connect(seed_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer data) {
        auto* d = static_cast<SeedData*>(data);
        perform_seeding(d->fb, d->rm); 
    }), sc);

    refresh_radio_list(flowbox, *mgr_out);

    gtk_box_append(GTK_BOX(radio_box), meta_label);
    gtk_box_append(GTK_BOX(radio_box), action_row);
    gtk_box_append(GTK_BOX(radio_box), flowbox);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(radio_scroll), radio_box);
    
    return radio_scroll;
}

// (Bluetooth-Seite bleibt gleich)
GtkWidget* create_bluetooth_page() {
    GtkWidget *bt_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    GtkWidget *bt_list = gtk_list_box_new();
    gtk_widget_add_css_class(bt_list, "bt-list");
    static BluetoothManager *bt_mgr = new BluetoothManager(GTK_LIST_BOX(bt_list));
    GtkWidget *scan_btn = gtk_button_new_with_label("Nach Geräten suchen");
    g_signal_connect(scan_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer d) { static_cast<BluetoothManager*>(d)->start_discovery(); }), bt_mgr);
    gtk_box_append(GTK_BOX(bt_box), bt_list);
    gtk_box_append(GTK_BOX(bt_box), scan_btn);
    return bt_box;
}

// GPIO Callback Wrapper
void on_encoder_event(bool clockwise, gpointer data) {
    // @TODO: Implement real volume setting
    (void) clockwise;
    AppWidgets *w = static_cast<AppWidgets*>(data);
    
    // UI Updates müssen im Main-Thread passieren
    g_idle_add((GSourceFunc)+[](gpointer user_data) -> gboolean {
        AppWidgets *widgets = static_cast<AppWidgets*>(user_data);
        // Logik: Lautstärke ändern (simuliert: immer lauter/leiser toggeln oder einfach inkrementieren)
        // Hier vereinfacht: Jeder Klick erhöht Lautstärke, bei 100 Reset auf 0 (Demo)
        widgets->current_volume = (widgets->current_volume + 5) % 105;
        
        if (widgets->radio_mgr) {
            widgets->radio_mgr->set_volume(widgets->current_volume / 100.0);
        }
        return FALSE;
    }, w);
}

static void activate(GtkApplication *app, gpointer) {
    AppWidgets *widgets = new AppWidgets();

    widgets->gps_mgr = new GPSManager();
    widgets->gps_mgr->start();
    
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(window), 1024, 600);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "assets/style.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(main_box, "main-layout");
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(top_bar, "top-bar");
    GtkWidget *top_clock = gtk_label_new("00:00");
    gtk_widget_add_css_class(top_clock, "top-clock");
    gtk_widget_set_hexpand(top_clock, TRUE);
    gtk_widget_set_halign(top_clock, GTK_ALIGN_END);
    g_timeout_add_seconds(1, update_clock_label, top_clock);
    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(close_btn, "top-close-btn");
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(g_application_quit), app);
    gtk_box_append(GTK_BOX(top_bar), top_clock);
    gtk_box_append(GTK_BOX(top_bar), close_btn);
    gtk_box_append(GTK_BOX(main_box), top_bar);

    // --- Overlay für die Tastatur ---
    GtkWidget *overlay = gtk_overlay_new();
    gtk_widget_set_vexpand(overlay, TRUE);

    // Tastatur und Revealer erstellen
    widgets->keyboard = new VirtualKeyboard();
    widgets->keyboard_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(widgets->keyboard_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_transition_duration(GTK_REVEALER(widgets->keyboard_revealer), 300); // Schnelle Animation
    gtk_revealer_set_child(GTK_REVEALER(widgets->keyboard_revealer), widgets->keyboard->get_widget());
    
    // Callback zum Verstecken der Tastatur setzen
    widgets->keyboard->set_hide_callback([w = widgets]() {
        gtk_revealer_set_reveal_child(GTK_REVEALER(w->keyboard_revealer), FALSE);
    });

    widgets->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(widgets->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    
    // Stack als Haupt-Kind des Overlays, Tastatur als Overlay-Element
    gtk_overlay_set_child(GTK_OVERLAY(overlay), widgets->stack);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), widgets->keyboard_revealer);
    gtk_widget_set_valign(widgets->keyboard_revealer, GTK_ALIGN_END); // Unten ausrichten

    RadioManager *radio_mgr = nullptr;
    gtk_stack_add_titled(GTK_STACK(widgets->stack), create_radio_page(&radio_mgr, widgets), "radio", "Radio");
    widgets->radio_mgr = radio_mgr; // Manager im Struct speichern für Zugriff via GPIO
    gtk_stack_add_titled(GTK_STACK(widgets->stack), create_bluetooth_page(), "bt", "Bluetooth");

    GtkWidget *nav_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(nav_bar, "bottom-bar");

    const char* icon_paths[] = {
        "assets/icons/radio.png", 
        "assets/icons/media.png", 
        "assets/icons/navigation.png", 
        "assets/icons/bluetooth.png"
    };
    const char* ids[] = {"radio", "media", "navi", "bt"};

    for (int i = 0; i < 4; i++) {
        GtkWidget *btn = create_nav_button(icon_paths[i]);
        gtk_widget_set_hexpand(btn, TRUE);
        
        g_object_set_data_full(G_OBJECT(btn), "sid", g_strdup(ids[i]), g_free);
        g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkWidget* b, gpointer s) {
            gtk_stack_set_visible_child_name(GTK_STACK(s), (const char*)g_object_get_data(G_OBJECT(b), "sid"));
        }), widgets->stack);
        
        gtk_box_append(GTK_BOX(nav_bar), btn);
    }

    gtk_box_append(GTK_BOX(main_box), overlay);
    gtk_box_append(GTK_BOX(main_box), nav_bar);
    gtk_window_present(GTK_WINDOW(window));

    // GPIO Thread starten (Pins 17 und 27 als Beispiel für Encoder A/B)
    std::thread gpio_thread(monitor_encoder, 17, 27, on_encoder_event, widgets);
    gpio_thread.detach();
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.car.os", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    return g_application_run(G_APPLICATION(app), argc, argv);
}
