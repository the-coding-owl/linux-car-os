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
    // Anstatt der Makro-Zeile nutzen wir eine Lambda-Funktion für maximale Flexibilität:
    g_signal_connect(btn_radio, "clicked", G_CALLBACK(+[](GtkButton* btn, gpointer data) {
        gtk_stack_set_visible_child_name(GTK_STACK(data), "radio");
    }), stack);
    
    gtk_box_append(GTK_BOX(nav_bar), btn_radio);
    gtk_box_append(GTK_BOX(box), stack);
    gtk_box_append(GTK_BOX(box), nav_bar);

    return box;
}
