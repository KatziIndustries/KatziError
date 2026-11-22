#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include "kerror.h"   // provides: msg.title, msg.text, msg.button1, msg.button2,
                      //           msg.progressactive, msg.progress

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define W 560
#define H 320

// Button geometry (client coordinates)
#define BTN1_X 115
#define BTN1_Y 235
#define BTN2_X 315
#define BTN2_Y 235
#define BTN_W  130
#define BTN_H  48

// Fixed-size window: no resize, no maximize
#define WINDOW_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)

typedef struct {
    HWND hwnd;
    HINSTANCE instance;

    HDC memdc;
    HBITMAP bitmap;
    HGDIOBJ old_bitmap;
    uint32_t *pixels;       // top-down 0x00RRGGBB, W*H

    int running;
    int result;

    int mouse_x;
    int mouse_y;
} Win32;

static Win32 w = {0};
static Win32 *wp = &w;

static const char *WINDOW_CLASS = "NativeMessageWindow";

// ----------------------------------------
// Helpers
// ----------------------------------------

static int point_in_rect(int px, int py, int x, int y, int rw, int rh)
{
    return px >= x && px < x + rw &&
           py >= y && py < y + rh;
}

static int over_button1(int px, int py)
{
    // Button 1 is not drawn while the progress bar is active.
    return !msg.progressactive &&
           point_in_rect(px, py, BTN1_X, BTN1_Y, BTN_W, BTN_H);
}

static int over_button2(int px, int py)
{
    return point_in_rect(px, py, BTN2_X, BTN2_Y, BTN_W, BTN_H);
}

// ----------------------------------------
// Drawing (identical to the Wayland version)
// ----------------------------------------

static void pixel(Win32 *w, int x, int y, uint32_t color)
{
    if (x < 0 || y < 0 || x >= W || y >= H)
        return;

    w->pixels[y * W + x] = color;
}

static void rect(
    Win32 *w,
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

static void glyph(Win32 *w, int x, int y, char c,
                  uint32_t color, int scale)
{
    c = (char)toupper((unsigned char)c);

    if (c == ' ')
        return;

    const uint8_t *g = NULL;

    if (c >= 'A' && c <= 'Z')
        g = font[c - 'A'];
    else if (c >= '0' && c <= '9')
        g = font[26 + c - '0'];

    if (g) {
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

static void draw_text(Win32 *w, int x, int y,
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
    Win32 *w,
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
// ----------------------------------------

static void render(void)
{
    if (!wp->pixels)
        return;

    rect(wp, 0, 0, W, H, 0x001B1B24);

    // Header
    rect(wp, 0, 0, W, 42, 0x00272735);
    draw_text(wp, 18, 15, msg.title, 0x00FFFFFF, 2);

    // Message
    draw_text(wp, 24, 75, msg.text, 0x00FFFFFF, 2);

    if (msg.progressactive == false) {
        rect(wp, BTN1_X, BTN1_Y, BTN_W, BTN_H, 0x00444455);
        rect(wp, BTN2_X, BTN2_Y, BTN_W, BTN_H, 0x003B82F6);

        draw_button_text(wp, BTN1_X, BTN1_Y, BTN_W, BTN_H,
                         msg.button1, 0x00FFFFFF, 2);

        draw_button_text(wp, BTN2_X, BTN2_Y, BTN_W, BTN_H,
                         msg.button2, 0x00FFFFFF, 2);
    } else {
        // msg.progress (float) 0.0 -> 1.0
        rect(wp, BTN2_X, BTN2_Y, BTN_W, BTN_H, 0x00444455);

        float progress = msg.progress;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        rect(wp, BTN2_X, BTN2_Y, (int)(BTN_W * progress), BTN_H, 0x003B82F6);

        draw_button_text(wp, BTN2_X, BTN2_Y, BTN_W, BTN_H,
                         msg.button2, 0x00FFFFFF, 2);
    }
}

void redraw(void)
{
    if (!wp->hwnd || !wp->pixels)
        return;

    render();

    InvalidateRect(wp->hwnd, NULL, FALSE);
    UpdateWindow(wp->hwnd);
}

// ----------------------------------------
// Window procedure
// ----------------------------------------

static LRESULT CALLBACK window_proc(
    HWND hwnd,
    UINT umsg,          // not "msg": that's the global dialog state
    WPARAM wParam,
    LPARAM lParam)
{
    switch (umsg) {

    // We paint everything ourselves; skip the background erase.
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);

        if (wp->memdc)
            BitBlt(dc, 0, 0, W, H, wp->memdc, 0, 0, SRCCOPY);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);

            if (over_button1(pt.x, pt.y) || over_button2(pt.x, pt.y))
                SetCursor(LoadCursorA(NULL, IDC_HAND));
            else
                SetCursor(LoadCursorA(NULL, IDC_ARROW));

            return TRUE;
        }
        break;

    case WM_MOUSEMOVE:
        wp->mouse_x = GET_X_LPARAM(lParam);
        wp->mouse_y = GET_Y_LPARAM(lParam);
        return 0;

    case WM_LBUTTONDOWN:
        wp->mouse_x = GET_X_LPARAM(lParam);
        wp->mouse_y = GET_Y_LPARAM(lParam);

        // Button 1 ("QUIT")
        if (over_button1(wp->mouse_x, wp->mouse_y)) {
            wp->result = 1;
            return 0;
        }

        // Button 2 ("OK")
        if (over_button2(wp->mouse_x, wp->mouse_y)) {
            wp->result = 2;
            return 0;
        }

        return 0;

    case WM_CLOSE:
        // Don't destroy here; the owner calls windows_native_close().
        wp->result = -1;
        wp->running = 0;
        return 0;

    case WM_DESTROY:
        wp->hwnd = NULL;
        wp->running = 0;
        return 0;
    }

    return DefWindowProcA(hwnd, umsg, wParam, lParam);
}

// ----------------------------------------
// Backbuffer (DIB section)
// ----------------------------------------

static int create_buffer(Win32 *w)
{
    BITMAPINFO bi = {0};

    bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth       = W;
    bi.bmiHeader.biHeight      = -H;          // negative = top-down
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    w->memdc = CreateCompatibleDC(NULL);
    if (!w->memdc)
        return -1;

    void *bits = NULL;

    w->bitmap = CreateDIBSection(
        w->memdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);

    if (!w->bitmap || !bits) {
        DeleteDC(w->memdc);
        w->memdc = NULL;
        w->bitmap = NULL;
        return -1;
    }

    w->pixels = (uint32_t *)bits;
    w->old_bitmap = SelectObject(w->memdc, w->bitmap);

    return 0;
}

// ----------------------------------------
// Cleanup
// ----------------------------------------

void windows_native_close(void)
{
    if (!w.instance)
        return;

    if (w.hwnd)
        DestroyWindow(w.hwnd);

    if (w.memdc) {
        if (w.old_bitmap)
            SelectObject(w.memdc, w.old_bitmap);
        DeleteDC(w.memdc);
    }

    if (w.bitmap)
        DeleteObject(w.bitmap);

    w = (Win32){0};
}

// ----------------------------------------
// PUBLIC API
// ----------------------------------------

int windows_native_init(char *text)
{
    (void)text;     // text comes from msg.text, same as the Wayland version

    w = (Win32){0};

    w.running = 1;
    w.result = 0;
    w.instance = GetModuleHandleA(NULL);

    WNDCLASSA wc = {0};

    wc.lpfnWndProc   = window_proc;
    wc.hInstance     = w.instance;
    wc.lpszClassName = WINDOW_CLASS;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;

    if (!RegisterClassA(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        goto error;

    if (create_buffer(&w) < 0)
        goto error;

    render();

    // Outer size computed from the final style so the client area is exactly W x H.
    RECT r = {0, 0, W, H};
    AdjustWindowRect(&r, WINDOW_STYLE, FALSE);

    w.hwnd = CreateWindowExA(
        0,
        WINDOW_CLASS,
        "Message",
        WINDOW_STYLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        r.right - r.left,
        r.bottom - r.top,
        NULL, NULL,
        w.instance,
        NULL);

    if (!w.hwnd)
        goto error;

    ShowWindow(w.hwnd, SW_SHOW);
    SetForegroundWindow(w.hwnd);
    UpdateWindow(w.hwnd);

    return 0;

error:
    windows_native_close();
    return -1;
}

// Non-blocking. Returns:
//   0  nothing happened
//   1  button 1 clicked
//   2  button 2 clicked
//  -1  window closed / error
int windows_native_pull(void)
{
    if (!w.hwnd)
        return -1;

    if (!w.running)
        return w.result;

    MSG m;

    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) {
        if (m.message == WM_QUIT) {
            w.running = 0;
            w.result = -1;
            return -1;
        }

        TranslateMessage(&m);
        DispatchMessageA(&m);

        if (!w.running)
            return w.result;
    }

    int output = w.result;
    w.result = 0;

    return output;
}