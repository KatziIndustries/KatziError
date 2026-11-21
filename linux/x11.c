#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "kerror.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define W 560
#define H 320
#define STRIDE (W * 4)
#define SIZE (STRIDE * H)

typedef struct {
    Display *display;
    Window window;
    GC gc;
    XImage *image;

    uint32_t *pixels;

    Atom wm_delete;

    int running;
    int result;

    int mouse_x;
    int mouse_y;
} X11;

static X11 w = {0};
static X11 *wp = &w;

// ----------------------------------------
// Drawing
// ----------------------------------------

static void pixel(X11 *w, int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= W || y >= H)
        return;

    w->pixels[y * W + x] = color;
}

static void rect(
    X11 *w,
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

static void glyph(X11 *w, int x, int y, char c,
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

static void draw_text(X11 *w, int x, int y,
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
    X11 *w,
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

// ----------------------------------------
// Render dialog
// (static + x11_ prefix so it can't collide with the Wayland
//  backend's non-static render()/redraw() if both are linked)
// ----------------------------------------

static void x11_render(void)
{
    rect(wp, 0, 0, W, H, 0x001B1B24);

    // Header
    rect(wp, 0, 0, W, 42, 0x00272735);
    draw_text(wp, 18, 15, msg.title, 0x00FFFFFF, 2);

    // Message
    draw_text(wp, 24, 75, msg.text, 0x00FFFFFF, 2);

    if (msg.progressactive == false) {
        rect(wp, 115, 235, 130, 48, 0x00444455);
        rect(wp, 315, 235, 130, 48, 0x003B82F6);

        draw_button_text(wp, 115, 235, 130, 48,
                         msg.button1, 0x00FFFFFF, 2);

        draw_button_text(wp, 315, 235, 130, 48,
                         msg.button2, 0x00FFFFFF, 2);
    } else {
        // msg.progress (float) 0.0 -> 1.0
        rect(wp, 315, 235, 130, 48, 0x00444455);

        float progress = msg.progress;

        rect(wp, 315, 235, (int)(130 * progress), 48, 0x003B82F6);

        draw_button_text(wp, 315, 235, 130, 48,
                         msg.button2, 0x00FFFFFF, 2);
    }
}

// ----------------------------------------
// X11 framebuffer
// ----------------------------------------

static int create_image(X11 *w)
{
    w->pixels = calloc(1, SIZE);

    if (!w->pixels)
        return -1;

    int screen = DefaultScreen(w->display);
    Visual *visual = DefaultVisual(w->display, screen);
    int depth = DefaultDepth(w->display, screen);

    w->image = XCreateImage(
        w->display, visual, depth,
        ZPixmap, 0,
        (char *)w->pixels,
        W, H,
        32, STRIDE);

    if (!w->image) {
        free(w->pixels);
        w->pixels = NULL;
        return -1;
    }

    return 0;
}

static void present(X11 *w)
{
    XPutImage(w->display, w->window, w->gc, w->image,
              0, 0, 0, 0, W, H);
    XFlush(w->display);
}

// Re-render from `msg` and push to the window (e.g. after progress changes).
void linux_x11_redraw(void)
{
    if (!wp->display || !wp->window || !wp->image)
        return;

    x11_render();
    present(wp);
}

// ----------------------------------------
// Cleanup
// ----------------------------------------

static void cleanup(X11 *w)
{
    // XDestroyImage also frees w->pixels (it owns the data pointer)
    if (w->image)
        XDestroyImage(w->image);
    else
        free(w->pixels);

    if (w->gc)
        XFreeGC(w->display, w->gc);

    if (w->window)
        XDestroyWindow(w->display, w->window);

    if (w->display)
        XCloseDisplay(w->display);
}

// ----------------------------------------
// Event handling
// ----------------------------------------

static void handle_event(X11 *w, XEvent *event)
{
    switch (event->type) {

    case Expose:
        if (event->xexpose.count == 0) {
            x11_render();
            present(w);
        }
        break;

    case EnterNotify:
        w->mouse_x = event->xcrossing.x;
        w->mouse_y = event->xcrossing.y;
        break;

    case LeaveNotify:
        w->mouse_x = -1;
        w->mouse_y = -1;
        break;

    case MotionNotify:
        w->mouse_x = event->xmotion.x;
        w->mouse_y = event->xmotion.y;
        break;

    case ButtonPress: {
        if (event->xbutton.button != Button1)
            break;

        int x = w->mouse_x = event->xbutton.x;
        int y = w->mouse_y = event->xbutton.y;

        // QUIT
        if (x >= 115 && x < 245 &&
            y >= 235 && y < 283) {
            w->result = 1;
            break;
        }

        // OK
        if (x >= 315 && x < 445 &&
            y >= 235 && y < 283) {
            w->result = 2;
            break;
        }
        break;
    }

    case ClientMessage:
        if ((Atom)event->xclient.data.l[0] == w->wm_delete) {
            w->result = -1;
            w->running = 0;
        }
        break;

    case DestroyNotify:
        w->result = -1;
        w->running = 0;
        break;
    }
}

// ----------------------------------------
// PUBLIC API
// ----------------------------------------

void linux_x11_close(void)
{
    if (!w.display)
        return;

    cleanup(&w);

    w = (X11){0};
}

int linux_x11_init(char *text)
{
    (void)text;

    w = (X11){0};

    w.running = 1;
    w.result = 0;
    w.mouse_x = -1;
    w.mouse_y = -1;

    w.display = XOpenDisplay(NULL);

    if (!w.display)
        return -1;

    int screen = DefaultScreen(w.display);

    w.window = XCreateSimpleWindow(
        w.display,
        RootWindow(w.display, screen),
        100, 100,
        W, H,
        1,
        BlackPixel(w.display, screen),
        BlackPixel(w.display, screen));

    if (!w.window)
        goto error;

    XStoreName(w.display, w.window, "Message");

    // Fixed-size window
    XSizeHints hints = {0};
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = W;
    hints.min_height = H;
    hints.max_width = W;
    hints.max_height = H;
    XSetWMNormalHints(w.display, w.window, &hints);

    // Class hint (equivalent of the Wayland app_id)
    XClassHint class_hint;
    class_hint.res_name = "linux-dialog";
    class_hint.res_class = "linux-dialog";
    XSetClassHint(w.display, w.window, &class_hint);

    w.wm_delete = XInternAtom(w.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(w.display, w.window, &w.wm_delete, 1);

    XSelectInput(
        w.display, w.window,
        ExposureMask |
        ButtonPressMask |
        PointerMotionMask |
        EnterWindowMask |
        LeaveWindowMask |
        StructureNotifyMask);

    w.gc = XCreateGC(w.display, w.window, 0, NULL);

    if (!w.gc)
        goto error;

    if (create_image(&w) < 0)
        goto error;

    x11_render();

    XMapWindow(w.display, w.window);
    XFlush(w.display);

    return 0;

error:
    linux_x11_close();
    return -1;
}

// Non-blocking. Same semantics as linux_wayland_pull:
//   0  nothing happened
//   1  left button button (QUIT)
//   2  right button (OK)
//  -1  window closed / error
int linux_x11_pull(void)
{
    if (!w.display)
        return -1;

    if (!w.running)
        return w.result;

    while (w.running && XPending(w.display) > 0) {
        XEvent event;
        XNextEvent(w.display, &event);
        handle_event(&w, &event);
    }

    int output = w.result;

    // Close/error stays latched (running == 0); clicks are one-shot.
    if (w.running)
        w.result = 0;

    return output;
}