#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kerror.h"

typedef enum {
    SESSION_UNKNOWN,
    SESSION_X11,
    SESSION_WAYLAND
} SessionType;

SessionType get_session_type(void) {
    const char *type = getenv("XDG_SESSION_TYPE");

    if (type) {
        if (strcmp(type, "wayland") == 0)
            return SESSION_WAYLAND;

        if (strcmp(type, "x11") == 0)
            return SESSION_X11;
    }

    if (getenv("WAYLAND_DISPLAY"))
        return SESSION_WAYLAND;

    if (getenv("DISPLAY"))
        return SESSION_X11;

    return SESSION_UNKNOWN;
}

int kerror(char *text)
{
    printf("Unsupport please update your implementation\n");
    return 0;
}

void kerror_init()
{
    switch (get_session_type()) {
        case SESSION_X11:
            linux_x11_init("");
            return;
        case SESSION_WAYLAND:
            linux_wayland_init("");
            return;
        default:
            printf("Unknown Display\n");
            break;
    }
}

void redraw()
{
    if(get_session_type() == SESSION_WAYLAND)
        linux_wayland_redraw();
}

int kerror_pull()
{
    switch (get_session_type()) {
        case SESSION_X11:
            return linux_x11_pull();
        case SESSION_WAYLAND:
            return linux_wayland_pull();
        default:
            printf("Unknown Display\n");
            break;
    }

    return -1;
}

void kerror_close()
{
    switch (get_session_type()) {
        case SESSION_X11:
            linux_x11_close();
            return;
        case SESSION_WAYLAND:
            linux_wayland_close();
            return;
        default:
            printf("Unknown Display\n");
            break;
    }
}