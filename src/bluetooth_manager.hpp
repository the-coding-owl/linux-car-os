#ifndef BLUETOOTH_MANAGER_HPP
#define BLUETOOTH_MANAGER_HPP

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <vector>
#include <string>

// Struktur für ein gefundenes Gerät
struct BluetoothDevice {
    std::string address;
    std::string name;
    bool paired;
    bool connected;
};

class BluetoothManager {
private:
    GDBusConnection *connection;
    GtkListBox *ui_list; // Referenz auf die Liste im UI

public:
    BluetoothManager(GtkListBox *listbox) : ui_list(listbox) {
        GError *error = nullptr;
        connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
        
        if (error) {
            g_printerr("Fehler beim Verbinden mit D-Bus: %s\n", error->message);
            g_error_free(error);
        } else {
            setup_signals();
        }
    }

    void start_discovery() {
        if (!connection) return;

        g_print("Bluetooth: Sende StartDiscovery Signal...\n");

        g_dbus_connection_call(
            connection,
            "org.bluez",
            "/org/bluez/hci0",
            "org.bluez.Adapter1",
            "StartDiscovery",
            nullptr, // Hier war der Absturzgrund: nullptr ist oft okay, aber GVariant ist strikt
            nullptr, // Erwarteter Rückgabetyp (GVariantType*)
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            nullptr,
            [](GObject* source, GAsyncResult* res, gpointer user_data) {
                GError *local_error = nullptr;
                // WICHTIG: Ergebnis abholen, sonst bleibt der Call im Speicher hängen
                GVariant *result = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &local_error);
                
                if (local_error) {
                    g_printerr("Discovery Fehler: %s\n", local_error->message);
                    g_error_free(local_error);
                } else {
                    g_print("Bluetooth: Scan läuft.\n");
                    g_variant_unref(result);
                }
            },
            nullptr
        );
    }

    // Meldet sich für Signale an, wenn neue Geräte auftauchen
    void setup_signals() {
        g_dbus_connection_signal_subscribe(
            connection,
            "org.bluez",
            "org.freedesktop.DBus.ObjectManager",
            "InterfacesAdded",
            nullptr, // Objekt-Pfad egal
            nullptr,
            G_DBUS_SIGNAL_FLAGS_NONE,
            on_interface_added,
            this, // Wir übergeben die Instanz
            nullptr
        );
    }

    void pair_device(const std::string& address) {
        if (!connection) return;

        // MAC-Adresse für D-Bus Pfad formatieren (Doppelpunkte zu Unterstrichen)
        std::string path_addr = address;
        for (auto &c : path_addr) if (c == ':') c = '_';
        std::string device_path = "/org/bluez/hci0/dev_" + path_addr;

        g_print("D-Bus Call: Pair @ %s\n", device_path.c_str());

        g_dbus_connection_call(
            connection, "org.bluez", device_path.c_str(), "org.bluez.Device1",
            "Pair", nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
            [](GObject* source, GAsyncResult* res, gpointer user_data) {
                GError *err = nullptr;
                g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), res, &err);
                if (err) {
                    g_printerr("Pairing fehlgeschlagen: %s\n", err->message);
                    g_error_free(err);
                } else {
                    g_print("Pairing erfolgreich! Verbinde...\n");
                    // Nach dem Pairing folgt meist automatisch 'Connect'
                }
            }, nullptr);
    }

private:
    void add_device_to_ui(const std::string& name, const std::string& address) {
        GtkWidget *row = gtk_list_box_row_new();
        
        // Wir speichern die MAC-Adresse als String am Row-Objekt
        g_object_set_data_full(G_OBJECT(row), "dev_addr", g_strdup(address.c_str()), g_free);

        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
        GtkWidget *label_name = gtk_label_new(name.c_str());
        GtkWidget *label_addr = gtk_label_new(address.c_str());

        gtk_widget_set_hexpand(label_name, TRUE);
        gtk_widget_set_halign(label_name, GTK_ALIGN_START);
        gtk_widget_add_css_class(label_addr, "bt-address-dimmed");

        gtk_box_append(GTK_BOX(box), label_name);
        gtk_box_append(GTK_BOX(box), label_addr);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

        gtk_list_box_append(ui_list, row);
    }

    static void on_interface_added(GDBusConnection*, const gchar*, const gchar*, 
                               const gchar*, const gchar*, GVariant *parameters, 
                               gpointer user_data) {
        BluetoothManager *self = static_cast<BluetoothManager*>(user_data);
        
        const gchar *object_path;
        GVariantIter *interfaces;

        // Entpacken des Tupels: (ObjectPath, Dict von Interfaces)
        g_variant_get(parameters, "(&oa{sa{sv}})", &object_path, &interfaces);

        const gchar *interface_name;
        GVariantIter *properties_iter;

        // Wir suchen in den Interfaces nach "org.bluez.Device1"
        while (g_variant_iter_next(interfaces, "{&sa{sv}}", &interface_name, &properties_iter)) {
            if (g_strcmp0(interface_name, "org.bluez.Device1") == 0) {
                const gchar *name = "Unbekanntes Gerät";
                const gchar *address = "??:??:??:??:??:??";
                
                const gchar *key;
                GVariant *value;

                // Wir iterieren durch die Eigenschaften des Geräts
                while (g_variant_iter_next(properties_iter, "{&sv}", &key, &value)) {
                    if (g_strcmp0(key, "Alias") == 0 || (g_strcmp0(key, "Name") == 0 && g_strcmp0(name, "Unbekanntes Gerät") == 0)) {
                        name = g_variant_get_string(value, nullptr);
                    } else if (g_strcmp0(key, "Address") == 0) {
                        address = g_variant_get_string(value, nullptr);
                    }
                    g_variant_unref(value);
                }

                g_print("Bluetooth: Gerät erkannt -> %s [%s]\n", name, address);

                // UI-Update sicher im Main-Thread ausführen
                struct DevData { BluetoothManager* mgr; std::string n; std::string a; };
                DevData* d = new DevData{self, name, address};

                g_idle_add((GSourceFunc)+[](gpointer data) -> gboolean {
                    DevData* d = static_cast<DevData*>(data);
                    d->mgr->add_device_to_ui(d->n, d->a);
                    delete d;
                    return FALSE;
                }, d);
            }
            g_variant_iter_free(properties_iter); // Wichtig: Den inneren Iterator freigeben
        }
        g_variant_iter_free(interfaces);
    }
};

#endif