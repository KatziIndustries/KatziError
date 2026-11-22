#include <stdbool.h>

int kerror(char *text);

void redraw();

int linux_wayland_init(char *text);
int linux_x11_init(char *text);
int windows_native_init(char *text);
void kerror_init();

int linux_wayland_pull();
int linux_x11_pull();
int windows_native_pull();
int kerror_pull();

void linux_wayland_close();
void linux_x11_close();
void windows_native_close();
void kerror_close();

void linux_wayland_redraw();

void kerror_set_title(char *t);
void kerror_set_text(char *t);
void kerror_set_buttoncount(int n);
void kerror_set_button1(char *t);
void kerror_set_button2(char *t);
void kerror_set_progressactive(bool b);
void kerror_set_progress(float f);

typedef struct {
    const char *title;
    const char *text;
    int buttoncount;
    const char *button1;
    const char *button2;
    bool progressactive;
    float progress;
} Message;

extern Message msg;