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
    switch (get_session_type()) {
        case SESSION_X11:
            return linux_x11(text);
            break;
        case SESSION_WAYLAND:
            return linux_wayland(text);
        default:
            printf("Unknown");
            break;
    }

    return 0;
}