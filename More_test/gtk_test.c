// #include <gtk/gtk.h>
// int main() { 
//     g_print("GTK %d.%d.%d\n", gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version());
//     return 0; 
// }

#include <gtk/gtk.h>
#include <glib.h>
#include <pthread.h>

// Global UI widgets
GtkWidget *label;
GtkWidget *button;

// Thread data
typedef struct {
    gboolean running;
    guint counter;
} ThreadData;

// Callback to update UI (runs in main thread)
static gboolean update_label(gpointer user_data) {
    ThreadData *data = (ThreadData *)user_data;
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Count: %u", data->counter);
    gtk_label_set_text(GTK_LABEL(label), buffer);
    return G_SOURCE_REMOVE; // Run once (not repeatedly)
}

// Worker thread function
static void *counter_thread(void *user_data) {
    ThreadData *data = (ThreadData *)user_data;
    data->running = TRUE;
    data->counter = 0;

    while (data->running) {
        g_usleep(500000); // 0.5 sec delay
        data->counter++;
        
        // Schedule UI update from main thread
        g_idle_add(update_label, data);
    }
    return NULL;
}

// Button click callback
static void on_button_clicked(GtkButton *btn, gpointer user_data) {
    ThreadData *data = (ThreadData *)user_data;
    data->running = !data->running; // Toggle thread
    
    if (data->running) {
        pthread_t thread;
        pthread_create(&thread, NULL, counter_thread, data);
        pthread_detach(thread);
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    
    label = gtk_label_new("Press Start");
    button = gtk_button_new_with_label("Start/Stop");
    
    ThreadData *data = g_new0(ThreadData, 1);
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), data);
    
    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), button);
    gtk_window_set_child(GTK_WINDOW(window), box);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.counter", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}