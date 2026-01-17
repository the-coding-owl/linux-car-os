#ifndef RADIO_MANAGER_HPP
#define RADIO_MANAGER_HPP

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <string>
#include <iostream>
#include <curl/curl.h>

struct MetadataTask {
    GtkWidget* label;
    char* text;
};

class RadioManager {
public:
    GstElement *pipeline;
    GtkWidget *title_label;

    RadioManager(GtkWidget *label) : title_label(label) {
        gst_init(NULL, NULL);
        pipeline = gst_element_factory_make("playbin", "radio-player");

        if (!pipeline) {
            std::cerr << "[RadioManager] FEHLER: playbin konnte nicht erstellt werden!" << std::endl;
        }

        GstBus *bus = gst_element_get_bus(pipeline);
        gst_bus_add_watch(bus, (GstBusFunc)on_bus_message, this);
        gst_object_unref(bus);
    }

    std::string resolve_m3u(const std::string& url) {
        if (url.find(".m3u") == std::string::npos && url.find(".pls") == std::string::npos) {
            return url; // Keine Playlist, URL direkt zurückgeben
        }

        std::cout << "[RadioManager] Playlist erkannt, extrahiere Stream-URL..." << std::endl;
        
        // Einfacher Weg ohne libcurl (via Shell für schnellen Test):
        // In einer produktiven App solltest du eine HTTP-Library nutzen.
        std::string cmd = "curl -L -s " + url + " | grep -v '^#' | head -n 1";
        char buffer[128];
        std::string result = "";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe) {
            if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
                result = buffer;
                result.erase(result.find_last_not_of(" \n\r\t") + 1); // Trim
            }
            pclose(pipe);
        }
        
        if (!result.empty()) {
            std::cout << "[RadioManager] Echte URL gefunden: " << result << std::endl;
            return result;
        }
        return url; 
    }

    void set_source(const std::string& uri) {
        std::string final_uri = resolve_m3u(uri); // URL auflösen
        std::cout << "[RadioManager] Lade URI: " << final_uri << std::endl;
        
        gst_element_set_state(pipeline, GST_STATE_NULL); // Reset auf NULL für sauberen Wechsel
        g_object_set(pipeline, "uri", final_uri.c_str(), NULL);
        
        // Puffer-Einstellungen
        g_object_set(pipeline, "buffer-duration", 5000 * GST_MSECOND, NULL);
        g_object_set(pipeline, "buffer-size", 1024 * 1024, NULL); 

        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "[RadioManager] FEHLER: Pipeline konnte nicht in PLAYING-Zustand versetzt werden!" << std::endl;
        }
    }

    static gboolean on_bus_message(GstBus *bus, GstMessage *msg, gpointer data) {
        RadioManager *self = static_cast<RadioManager*>(data);

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *err;
                gchar *debug_info;
                gst_message_parse_error(msg, &err, &debug_info);
                std::cerr << "[RadioManager] GST-FEHLER: " << err->message << std::endl;
                std::cerr << "[RadioManager] Debug Info: " << (debug_info ? debug_info : "keine") << std::endl;
                g_clear_error(&err);
                g_free(debug_info);
                break;
            }
            case GST_MESSAGE_STATE_CHANGED: {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(self->pipeline)) {
                    std::cout << "[RadioManager] Status: " << gst_element_state_get_name(old_state) 
                              << " -> " << gst_element_state_get_name(new_state) << std::endl;
                }
                break;
            }
            case GST_MESSAGE_BUFFERING: {
                gint percent = 0;
                gst_message_parse_buffering(msg, &percent);
                std::cout << "[RadioManager] Buffering: " << percent << "%" << std::endl;
                break;
            }
            case GST_MESSAGE_TAG: {
                GstTagList *tags = NULL;
                gst_message_parse_tag(msg, &tags);
                gchar *title = NULL;
                gchar *artist = NULL;

                if (gst_tag_list_get_string(tags, GST_TAG_TITLE, &title) ||
                    gst_tag_list_get_string(tags, GST_TAG_ARTIST, &artist)) {
                    
                    std::string display = (artist ? std::string(artist) + " - " : "") + (title ? title : "Stream läuft...");
                    self->update_ui_label(display);
                    
                    g_free(title);
                    g_free(artist);
                }
                if (tags) gst_tag_list_free(tags);
                break;
            }
            default:
                break;
        }
        return TRUE;
    }

private:
    void update_ui_label(const std::string& text) {
        MetadataTask *task = new MetadataTask();
        task->label = this->title_label;
        task->text = g_strdup(text.c_str());

        g_idle_add((GSourceFunc)+[](gpointer d) -> gboolean {
            MetadataTask *t = static_cast<MetadataTask*>(d);
            if (GTK_IS_LABEL(t->label)) {
                gtk_label_set_text(GTK_LABEL(t->label), t->text);
            }
            g_free(t->text);
            delete t;
            return FALSE;
        }, task);
    }
};

#endif