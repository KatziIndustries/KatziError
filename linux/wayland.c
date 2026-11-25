
#define _GNU_SOURCE

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#include "kerror.h"

#include <linux/input-event-codes.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <errno.h>
#include <poll.h>

#define W 560
#define H 320
#define STRIDE (W * 4)
#define SIZE (STRIDE * H)

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_pointer *pointer;

    struct wl_surface *surface;
    struct wl_buffer *buffer;

    struct xdg_wm_base *wm;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;

    uint32_t *pixels;

    int configured;
    int running;
    int result;

    int mouse_x;
    int mouse_y;
} Wayland;

static Wayland w = {0};
static Wayland *wp = &w;

// ----------------------------------------
// Drawing
// ----------------------------------------

static void pixel(Wayland *w, int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= W || y >= H)
        return;

    w->pixels[y * W + x] = color;
}

static void rect(
    Wayland *w,
    int x, int y,
    int width, int height,
    uint32_t color)
{
    for (int py = y; py < y + height; py++)
        for (int px = x; px < x + width; px++)
            pixel(w, px, py, color);
}

// ----------------------------------------
// Tiny 5x7 bitmap font
// Each character is 5 columns, 7 bits.
// ----------------------------------------

static const uint8_t font[36][5] = {
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x3F,0x40,0x38,0x40,0x3F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z

    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}  // 9
};

static void glyph(Wayland *w, int x, int y, char c,
                  uint32_t color, int scale)
{
    c = (char)toupper((unsigned char)c);

    if (c == ' ')
        return;

    if (c >= 'A' && c <= 'Z') {
        const uint8_t *g = font[c - 'A'];

        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if (g[col] & (1 << row))
                    rect(w, x + col * scale, y + row * scale,
                         scale, scale, color);

        return;
    }

    if (c >= '0' && c <= '9') {
        const uint8_t *g = font[26 + c - '0'];

        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if (g[col] & (1 << row))
                    rect(w, x + col * scale, y + row * scale,
                         scale, scale, color);

        return;
    }

    // Simple punctuation
    if (c == '.') rect(w, x + 2*scale, y + 6*scale,
                       scale, scale, color);
    if (c == '!') {
        rect(w, x + 2*scale, y, scale, 5*scale, color);
        rect(w, x + 2*scale, y + 6*scale, scale, scale, color);
    }
    if (c == '-') rect(w, x, y + 3*scale, 5*scale, scale, color);
    if (c == ':') {
        rect(w, x + 2*scale, y + 2*scale, scale, scale, color);
        rect(w, x + 2*scale, y + 5*scale, scale, scale, color);
    }
    if (c == '[' || c == ']') {
        int xx = c == '[' ? x : x + 3*scale;
        rect(w, xx, y, scale, 7*scale, color);
        rect(w, xx, y, 2*scale, scale, color);
        rect(w, xx, y + 6*scale, 2*scale, scale, color);
    }
    if (c == '?') {
        rect(w, x + scale, y, 3*scale, scale, color);
        rect(w, x + 3*scale, y + scale, 2*scale, 2*scale, color);
        rect(w, x + 2*scale, y + 3*scale, scale, 2*scale, color);
        rect(w, x + 2*scale, y + 6*scale, scale, scale, color);
    }
}

static void draw_text(Wayland *w, int x, int y,
                      const char *text, uint32_t color, int scale)
{
    int start_x = x;

    while (*text) {
        if (*text == '\n' || x + 12 * scale > W - 20) {
            x = start_x;
            y += 11 * scale;

            if (*text == '\n') {
                text++;
                continue;
            }
        }

        glyph(w, x, y, *text, color, scale);
        x += 6 * scale;
        text++;
    }
}

static void draw_button_text(
    Wayland *w,
    int x, int y,
    int width, int height,
    const char *text,
    uint32_t color,
    int scale)
{
    int text_width = (int)strlen(text) * 6 * scale - scale;
    int text_height = 7 * scale;

    int text_x = x + (width - text_width) / 2;
    int text_y = y + (height - text_height) / 2;

    draw_text(w, text_x, text_y, text, color, scale);
}

static void centerd_rect(
    Wayland *w,
    int cx,
    int y,
    size_t dx,
    int height,
    uint32_t color
){
    int len = 65 > 6*(int)dx ? 65 : 6*(int)dx;
    rect(
        w,
        cx - len,
        y,
        2 * len,
        height,
        color
    );
}

// ----------------------------------------
// Render dialog
// ----------------------------------------

// static void render(Wayland *w, const char *message)
void render()
{
    rect(wp, 0, 0, W, H, 0x001B1B24);

    // Header
    rect(wp, 0, 0, W, 42, 0x00272735);
    draw_text(wp, 18, 15, msg.title, 0x00FFFFFF, 2);

    // Message
    draw_text(wp, 24, 75, msg.text, 0x00FFFFFF, 2);

    if (msg.progressactive == false) {

        size_t len = strlen(msg.button1) > strlen(msg.button2)
                     ? strlen(msg.button1)
                     : strlen(msg.button2);

        //rect(wp, 115, 235, 130, 48, 0x00444455);
        centerd_rect(wp, 180, 235, (int)len, 48, 0x00444455);

        draw_button_text(wp, 115, 235, 130, 48,
                         msg.button1, 0x00FFFFFF, 2);

        if (msg.buttoncount > 1) {
            // rect(wp, 315, 235, 130, 48, 0x003B82F6);
            centerd_rect(wp, 380, 235, (int)len, 48, 0x003B82F6);
            draw_button_text(wp, 315, 235, 130, 48,
                             msg.button2, 0x00FFFFFF, 2);
        }
    } else {
        // msg.progress (float) 0.0 -> 1.0

        float progress = msg.progress;

        rect(wp, 20, 185, (W-40), 48, 0x00444455);
        rect(wp, 20, 185, (int)((W - 40) * progress), 48, 0x003B82F6);

    }
}

void linux_wayland_redraw(void)
{
    if (!wp->configured || !wp->surface ||
        !wp->display || !wp->buffer)
        return;

    render();

    wl_surface_attach(wp->surface, wp->buffer, 0, 0);
    wl_surface_damage(wp->surface, 0, 0, W, H);
    wl_surface_commit(wp->surface);

    wl_display_flush(wp->display);
}

// ----------------------------------------
// XDG shell
// ----------------------------------------

static void wm_ping(void *data, struct xdg_wm_base *wm,
                    uint32_t serial)
{
    xdg_wm_base_pong(wm, serial);
}

static const struct xdg_wm_base_listener wm_listener = {
    .ping = wm_ping
};

static void surface_configure(
    void *data,
    struct xdg_surface *surface,
    uint32_t serial)
{
    Wayland *w = data;

    xdg_surface_ack_configure(surface, serial);

    render();

    wl_surface_attach(w->surface, w->buffer, 0, 0);
    wl_surface_damage(w->surface, 0, 0, W, H);
    wl_surface_commit(w->surface);

    w->configured = 1;
}

static const struct xdg_surface_listener surface_listener = {
    .configure = surface_configure
};

static void toplevel_configure(void *data,
                               struct xdg_toplevel *top,
                               int32_t width,
                               int32_t height,
                               struct wl_array *states)
{
}

static void toplevel_close(void *data,
                           struct xdg_toplevel *top)
{
    Wayland *w = data;
    w->result = 0;
    w->running = 0;
}

static const struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configure,
    .close = toplevel_close
};

static void buffer_release(void *data, struct wl_buffer *buffer)
{
    // Buffer is available for reuse.
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release
};

// ----------------------------------------
// Mouse input
// ----------------------------------------

static void pointer_enter(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface,
                          wl_fixed_t x, wl_fixed_t y)
{
    Wayland *w = data;
    w->mouse_x = wl_fixed_to_int(x);
    w->mouse_y = wl_fixed_to_int(y);
}

static void pointer_leave(
    void *data,
    struct wl_pointer *pointer,
    uint32_t serial,
    struct wl_surface *surface)
{
    Wayland *w = data;

    w->mouse_x = -1;
    w->mouse_y = -1;
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
                           uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
    Wayland *w = data;
    w->mouse_x = wl_fixed_to_int(x);
    w->mouse_y = wl_fixed_to_int(y);
}


static void pointer_button(
    void *data,
    struct wl_pointer *pointer,
    uint32_t serial,
    uint32_t time,
    uint32_t button,
    uint32_t state)
{
    Wayland *w = data;

    if (state != WL_POINTER_BUTTON_STATE_PRESSED)
        return;

    if (button != BTN_LEFT)
        return;

    int x = w->mouse_x;
    int y = w->mouse_y;

    // QUIT
    if (x >= 115 && x < 245 &&
        y >= 235 && y < 283) {

        w->result = 1;
        return;
    }

    // OK
    if (x >= 315 && x < 445 &&
        y >= 235 && y < 283) {

        w->result = 2;
        return;
    }
}


static void pointer_axis(void *data, struct wl_pointer *pointer,
                         uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static void pointer_frame(void *data, struct wl_pointer *pointer)
{
}

static void pointer_axis_source(
    void *data,
    struct wl_pointer *pointer,
    uint32_t axis_source)
{
}

static void pointer_axis_stop(
    void *data,
    struct wl_pointer *pointer,
    uint32_t time,
    uint32_t axis)
{
}

static void pointer_axis_discrete(
    void *data,
    struct wl_pointer *pointer,
    uint32_t axis,
    int32_t discrete)
{
}

static void pointer_axis_value120(
    void *data,
    struct wl_pointer *pointer,
    uint32_t axis,
    int32_t value120)
{
}

static void pointer_axis_relative_direction(
    void *data,
    struct wl_pointer *pointer,
    uint32_t axis,
    uint32_t direction)
{
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
    .axis_value120 = pointer_axis_value120,
    .axis_relative_direction = pointer_axis_relative_direction
};

_Static_assert(
    sizeof(struct wl_pointer_listener) >=
    sizeof(void *) * 10,
    "Unexpected wl_pointer_listener size"
);

static void seat_capabilities(void *data, struct wl_seat *seat,
                              uint32_t capabilities)
{
    Wayland *w = data;

    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        if (!w->pointer) {
            w->pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(
                w->pointer, &pointer_listener, w);
        }
    } else if (w->pointer) {
        wl_pointer_destroy(w->pointer);
        w->pointer = NULL;
    }
}

static void seat_name(void *data, struct wl_seat *seat,
                      const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name
};

// ----------------------------------------
// Registry
// ----------------------------------------

static void registry_global(void *data,
                            struct wl_registry *registry,
                            uint32_t name,
                            const char *interface,
                            uint32_t version)
{
    Wayland *w = data;

    if (!strcmp(interface, "wl_compositor")) {
        w->compositor = wl_registry_bind(
            registry, name, &wl_compositor_interface,
            version < 4 ? version : 4);
    }

    if (!strcmp(interface, "wl_shm")) {
        w->shm = wl_registry_bind(
            registry, name, &wl_shm_interface, 1);
    }

    if (!strcmp(interface, "wl_seat")) {
        w->seat = wl_registry_bind(
            registry, name, &wl_seat_interface,
            version < 5 ? version : 5);

        wl_seat_add_listener(w->seat, &seat_listener, w);
    }

    if (!strcmp(interface, "xdg_wm_base")) {
        w->wm = wl_registry_bind(
            registry, name, &xdg_wm_base_interface, 1);

        xdg_wm_base_add_listener(w->wm, &wm_listener, w);
    }
}

static void registry_remove(void *data,
                            struct wl_registry *registry,
                            uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_remove
};

// ----------------------------------------
// SHM framebuffer
// ----------------------------------------

static int create_buffer(Wayland *w)
{
    int fd = syscall(SYS_memfd_create, "wayland-pixels", 0);

    if (fd < 0)
        return -1;

    if (ftruncate(fd, SIZE) < 0) {
        close(fd);
        return -1;
    }

    w->pixels = mmap(NULL, SIZE,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED, fd, 0);

    if (w->pixels == MAP_FAILED) {
        w->pixels = NULL;
        close(fd);
        return -1;
    }

    struct wl_shm_pool *pool =
        wl_shm_create_pool(w->shm, fd, SIZE);

    w->buffer = wl_shm_pool_create_buffer(
        pool, 0, W, H, STRIDE,
        WL_SHM_FORMAT_XRGB8888
    );

    wl_buffer_add_listener(w->buffer, &buffer_listener, w);

    wl_shm_pool_destroy(pool);
    close(fd);

    return w->buffer ? 0 : -1;
}

// ----------------------------------------
// Cleanup
// ----------------------------------------

static void cleanup(Wayland *w)
{
    if (w->pointer)
        wl_pointer_destroy(w->pointer);

    if (w->buffer)
        wl_buffer_destroy(w->buffer);

    if (w->pixels)
        munmap(w->pixels, SIZE);

    if (w->toplevel)
        xdg_toplevel_destroy(w->toplevel);

    if (w->xdg_surface)
        xdg_surface_destroy(w->xdg_surface);

    if (w->surface)
        wl_surface_destroy(w->surface);

    if (w->wm)
        xdg_wm_base_destroy(w->wm);

    if (w->seat)
        wl_seat_destroy(w->seat);

    if (w->shm)
        wl_shm_destroy(w->shm);

    if (w->compositor)
        wl_compositor_destroy(w->compositor);

    if (w->registry)
        wl_registry_destroy(w->registry);

    if (w->display)
        wl_display_disconnect(w->display);
}

void linux_wayland_close(void)
{
    if (!w.display)
        return;

    cleanup(&w);

    w = (Wayland){0};
}

int linux_wayland_init(char *text)
{
    w = (Wayland){0};

    w.running = 1;
    w.result = 0;

    w.display = wl_display_connect(NULL);

    if (!w.display)
        return -1;

    w.registry = wl_display_get_registry(w.display);

    wl_registry_add_listener(
        w.registry, &registry_listener, &w);

    if (wl_display_roundtrip(w.display) < 0)
        goto error;

    if (!w.compositor || !w.shm || !w.wm || !w.seat)
        goto error;

    if (create_buffer(&w) < 0)
        goto error;

//    render(&w, text ? text : "");
    render();

    w.surface = wl_compositor_create_surface(w.compositor);

    w.xdg_surface = xdg_wm_base_get_xdg_surface(
        w.wm, w.surface);

    xdg_surface_add_listener(
        w.xdg_surface, &surface_listener, &w);

    w.toplevel = xdg_surface_get_toplevel(w.xdg_surface);

    xdg_toplevel_add_listener(
        w.toplevel, &toplevel_listener, &w);

    xdg_toplevel_set_title(w.toplevel, "Message");
    xdg_toplevel_set_app_id(w.toplevel, "linux-dialog");

    xdg_toplevel_set_min_size(w.toplevel, W, H);
    xdg_toplevel_set_max_size(w.toplevel, W, H);

    wl_surface_commit(w.surface);

    if (wl_display_roundtrip(w.display) < 0)
        goto error;

    return 0;

error:
    linux_wayland_close();
    return -1;
}



int linux_wayland_pull(void)
{
    if (!w.display)
        return -1;

    if (!w.running)
        return w.result;

    // Dispatch already received events.
    if (wl_display_dispatch_pending(w.display) < 0)
        goto error;

    if (!w.running)
        return w.result;

    // Prepare to read.
    while (wl_display_prepare_read(w.display) != 0) {
        if (wl_display_dispatch_pending(w.display) < 0)
            goto error;

        if (!w.running)
            return w.result;
    }

    // Flush outgoing Wayland requests.
    int want_write = 0;

    if (wl_display_flush(w.display) < 0) {
        if (errno == EAGAIN) {
            want_write = 1;
        } else {
            wl_display_cancel_read(w.display);
            goto error;
        }
    }

    // Nonblocking socket poll.
    struct pollfd pfd = {
        .fd = wl_display_get_fd(w.display),
        .events = POLLIN | (want_write ? POLLOUT : 0),
        .revents = 0
    };

    int ret;

    do {
        ret = poll(&pfd, 1, 0);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        wl_display_cancel_read(w.display);
        goto error;
    }

    // No events available.
    if (ret == 0) {
        wl_display_cancel_read(w.display);
        return 0;
    }

    // Read incoming events.
    if (pfd.revents & POLLIN) {
        if (wl_display_read_events(w.display) < 0)
            goto error;
    } else {
        wl_display_cancel_read(w.display);
    }

    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
        goto error;

    // Dispatch mouse clicks, motion, close events, etc.
    if (wl_display_dispatch_pending(w.display) < 0)
        goto error;

    int output = w.result;

    w.result = 0;

    return output;

error:
    w.running = 0;
    w.result = -1;
    return -1;
}
