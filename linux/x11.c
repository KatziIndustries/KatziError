
#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

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
// ----------------------------------------

static const uint8_t font[36][5] = {
    {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43},

    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}
};

static void glyph(
    X11 *w, int x, int y, char c,
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

    if (c == '.')
        rect(w, x + 2*scale, y + 6*scale,
             scale, scale, color);

    if (c == '!') {
        rect(w, x + 2*scale, y, scale, 5*scale, color);
        rect(w, x + 2*scale, y + 6*scale,
             scale, scale, color);
    }

    if (c == '-')
        rect(w, x, y + 3*scale, 5*scale, scale, color);

    if (c == ':') {
        rect(w, x + 2*scale, y + 2*scale,
             scale, scale, color);
        rect(w, x + 2*scale, y + 5*scale,
             scale, scale, color);
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

static void draw_text(
    X11 *w, int x, int y,
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

// ----------------------------------------
// Render dialog
// ----------------------------------------

static void render(X11 *w, const char *message)
{
    rect(w, 0, 0, W, H, 0x001B1B24);

    rect(w, 0, 0, W, 42, 0x00272735);
    draw_text(w, 18, 15, "MESSAGE", 0x00FFFFFF, 2);

    draw_text(w, 24, 75, message, 0x00FFFFFF, 2);

    rect(w, 115, 235, 130, 48, 0x00444455);
    rect(w, 315, 235, 130, 48, 0x003B82F6);

    draw_text(w, 132, 253, "  QUIT", 0x00FFFFFF, 2);
    draw_text(w, 350, 253, "  OK", 0x00FFFFFF, 2);
}

// ----------------------------------------
// X11 framebuffer
// ----------------------------------------

static int create_image(X11 *w)
{
    w->pixels = calloc(1, SIZE);

    if (!w->pixels)
        return -1;

    Visual *visual = DefaultVisual(w->display, DefaultScreen(w->display));
    int depth = DefaultDepth(w->display, DefaultScreen(w->display));

    w->image = XCreateImage(
        w->display,
        visual,
        depth,
        ZPixmap,
        0,
        (char *)w->pixels,
        W,
        H,
        32,
        STRIDE
    );

    if (!w->image)
        return -1;

    return 0;
}

static void present(X11 *w)
{
    XPutImage(
        w->display,
        w->window,
        w->gc,
        w->image,
        0, 0,
        0, 0,
        W, H
    );

    XFlush(w->display);
}

// ----------------------------------------
// Cleanup
// ----------------------------------------

static void cleanup(X11 *w)
{
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
// PUBLIC API
// ----------------------------------------

int linux_x11(char *text)
{
    X11 w = {0};

    w.running = 1;
    w.result = -1;

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
        BlackPixel(w.display, screen)
    );

    if (!w.window)
        goto error;

    XStoreName(w.display, w.window, "Message");

    XSizeHints hints = {0};
    hints.flags = PMinSize | PMaxSize;
    hints.min_width = W;
    hints.min_height = H;
    hints.max_width = W;
    hints.max_height = H;

    XSetWMNormalHints(w.display, w.window, &hints);

    w.wm_delete = XInternAtom(
        w.display,
        "WM_DELETE_WINDOW",
        False
    );

    XSetWMProtocols(
        w.display,
        w.window,
        &w.wm_delete,
        1
    );

    XSelectInput(
        w.display,
        w.window,
        ExposureMask |
        ButtonPressMask |
        PointerMotionMask |
        StructureNotifyMask
    );

    w.gc = XCreateGC(w.display, w.window, 0, NULL);

    if (!w.gc)
        goto error;

    if (create_image(&w) < 0)
        goto error;

    render(&w, text ? text : "");

    XMapWindow(w.display, w.window);
    XFlush(w.display);

    // Main X11 event loop.
    while (w.running) {
        XEvent event;

        XNextEvent(w.display, &event);

        switch (event.type) {

        case Expose:
            if (event.xexpose.count == 0)
                present(&w);
            break;

        case MotionNotify:
            w.mouse_x = event.xmotion.x;
            w.mouse_y = event.xmotion.y;
            break;

        case ButtonPress:
            if (event.xbutton.button != Button1)
                break;

            w.mouse_x = event.xbutton.x;
            w.mouse_y = event.xbutton.y;

            // QUIT button
            if (w.mouse_x >= 115 && w.mouse_x <= 245 &&
                w.mouse_y >= 235 && w.mouse_y <= 283) {
                w.result = 0;
                w.running = 0;
            }

            // OK button
            if (w.mouse_x >= 315 && w.mouse_x <= 445 &&
                w.mouse_y >= 235 && w.mouse_y <= 283) {
                w.result = 1;
                w.running = 0;
            }
            break;

        case ClientMessage:
            if ((Atom)event.xclient.data.l[0] == w.wm_delete) {
                w.result = 0;
                w.running = 0;
            }
            break;

        case DestroyNotify:
            w.result = 0;
            w.running = 0;
            break;
        }
    }

    int result = w.result;

    cleanup(&w);
    return result;

error:
    cleanup(&w);
    return -1;
}
