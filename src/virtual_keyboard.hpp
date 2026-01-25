#ifndef VIRTUAL_KEYBOARD_HPP
#define VIRTUAL_KEYBOARD_HPP

#include <gtk/gtk.h>
#include <vector>
#include <string>
#include <functional>

class VirtualKeyboard {
private:
    GtkWidget *container;
    GtkEditable *target_entry;
    std::function<void()> hide_callback;

public:
    VirtualKeyboard() {
        container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_widget_add_css_class(container, "virtual-keyboard");
        target_entry = nullptr;
        build_layout();
    }

    GtkWidget* get_widget() { return container; }

    void set_target(GtkEditable *entry) {
        target_entry = entry;
    }

    void set_hide_callback(std::function<void()> cb) {
        hide_callback = cb;
    }

private:
    void build_layout() {
        // Layout: QWERTZ (vereinfacht)
        const std::vector<std::vector<std::string>> rows = {
            {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "ß"},
            {"Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "Ü"},
            {"A", "S", "D", "F", "G", "H", "J", "K", "L", "Ö", "Ä"},
            {"Y", "X", "C", "V", "B", "N", "M", ".", "-", "_"}
        };

        for (const auto& row_keys : rows) {
            GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
            gtk_widget_set_halign(row_box, GTK_ALIGN_CENTER);
            
            for (const auto& key : row_keys) {
                create_key(key, row_box);
            }
            gtk_box_append(GTK_BOX(container), row_box);
        }

        // Sonderzeile: Space & Backspace
        GtkWidget *bottom_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_halign(bottom_row, GTK_ALIGN_CENTER);

        GtkWidget *space = gtk_button_new_with_label("SPACE");
        gtk_widget_set_size_request(space, 200, -1);
        gtk_widget_set_focusable(space, FALSE);
        g_signal_connect(space, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer d) {
            VirtualKeyboard* self = (VirtualKeyboard*)d;
            if (self->target_entry) {
                int pos = gtk_editable_get_position(self->target_entry);
                gtk_editable_insert_text(self->target_entry, " ", 1, &pos);
                gtk_editable_set_position(self->target_entry, pos);
            }
        }), this);
        gtk_box_append(GTK_BOX(bottom_row), space);

        GtkWidget *backspace = gtk_button_new_from_icon_name("edit-clear-symbolic");
        gtk_widget_set_focusable(backspace, FALSE);
        gtk_widget_add_css_class(backspace, "key-backspace");
        g_signal_connect(backspace, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer d) {
            VirtualKeyboard* self = (VirtualKeyboard*)d;
            if (self->target_entry) {
                int pos = gtk_editable_get_position(self->target_entry);
                if (pos > 0) {
                    gtk_editable_delete_text(self->target_entry, pos - 1, pos);
                }
            }
        }), this);
        gtk_box_append(GTK_BOX(bottom_row), backspace);

        // "Verstecken" Button
        GtkWidget *hide_btn = gtk_button_new_with_label("Verstecken");
        gtk_widget_set_focusable(hide_btn, FALSE);
        g_signal_connect(hide_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer d) {
            VirtualKeyboard* self = (VirtualKeyboard*)d;
            if (self->hide_callback) {
                self->hide_callback();
            }
        }), this);
        gtk_box_append(GTK_BOX(bottom_row), hide_btn);

        gtk_box_append(GTK_BOX(container), bottom_row);
    }

    void create_key(const std::string& label, GtkWidget* parent) {
        GtkWidget *btn = gtk_button_new_with_label(label.c_str());
        gtk_widget_set_focusable(btn, FALSE); // WICHTIG: Fokus bleibt im Entry
        gtk_widget_add_css_class(btn, "keyboard-key");
        
        // Label kopieren für Callback
        g_object_set_data_full(G_OBJECT(btn), "key_val", g_strdup(label.c_str()), g_free);
        g_signal_connect(btn, "clicked", G_CALLBACK(+[](GtkWidget* w, gpointer d) {
            VirtualKeyboard* self = (VirtualKeyboard*)d;
            const char* val = (const char*)g_object_get_data(G_OBJECT(w), "key_val");
            if (self->target_entry && val) {
                int pos = gtk_editable_get_position(self->target_entry);
                gtk_editable_insert_text(self->target_entry, val, -1, &pos);
                gtk_editable_set_position(self->target_entry, pos);
            }
        }), this);
        gtk_box_append(GTK_BOX(parent), btn);
    }
};

#endif