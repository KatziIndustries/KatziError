#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define W 560
#define H 320

// Button geometry (client coordinates)
#define QUIT_X 115
#define QUIT_Y 235
#define OK_X   315
#define OK_Y   235
#define BTN_W  130
#define BTN_H  48

// Fixed-size window: no resize, no maximize
#define WINDOW_STYLE (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX)

typedef struct {
    HWND hwnd;
    HINSTANCE instance;

    int running;
    int result;

    int mouse_x;
    int mouse_y;

    const char *message;
} WIN32_WINDOW;

static WIN32_WINDOW *g_window = NULL;

// ----------------------------------------
// Helpers
// ----------------------------------------

static int point_in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && px <= x + w &&
           py >= y && py <= y + h;
}

static int over_quit(int px, int py)
{
    return point_in_rect(px, py, QUIT_X, QUIT_Y, BTN_W, BTN_H);
}

static int over_ok(int px, int py)
{
    return point_in_rect(px, py, OK_X, OK_Y, BTN_W, BTN_H);
}

// ----------------------------------------
// Drawing
// ----------------------------------------

static void pixel(HDC dc, int x, int y, uint32_t color)
{
    SetPixelV(dc, x, y, RGB(
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF
    ));
}

static void rect(
    HDC dc,
    int x, int y,
    int width, int height,
    uint32_t color)
{
    HBRUSH brush = CreateSolidBrush(RGB(
        (color >> 16) & 0xFF,
        (color >> 8) & 0xFF,
        color & 0xFF
    ));

    RECT r = { x, y, x + width, y + height };

    FillRect(dc, &r, brush);
    DeleteObject(brush);
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
    HDC dc,
    int x, int y,
    char c,
    uint32_t color,
    int scale)
{
    c = (char)toupper((unsigned char)c);

    if (c == ' ')
        return;

    const uint8_t *g = NULL;

    if (c >= 'A' && c <= 'Z')
        g = font[c - 'A'];

    if (c >= '0' && c <= '9')
        g = font[26 + c - '0'];

    if (g) {
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                if (g[col] & (1 << row))
                    rect(dc,
                        x + col * scale,
                        y + row * scale,
                        scale, scale, color);
            }
        }

        return;
    }

    if (c == '.') {
        rect(dc, x + 2*scale, y + 6*scale,
             scale, scale, color);
    }

    if (c == '!') {
        rect(dc, x + 2*scale, y,
             scale, 5*scale, color);
        rect(dc, x + 2*scale, y + 6*scale,
             scale, scale, color);
    }

    if (c == '-') {
        rect(dc, x, y + 3*scale,
             5*scale, scale, color);
    }

    if (c == ':') {
        rect(dc, x + 2*scale, y + 2*scale,
             scale, scale, color);
        rect(dc, x + 2*scale, y + 5*scale,
             scale, scale, color);
    }

    if (c == '[' || c == ']') {
        int xx = c == '[' ? x : x + 3*scale;

        rect(dc, xx, y, scale, 7*scale, color);
        rect(dc, xx, y, 2*scale, scale, color);
        rect(dc, xx, y + 6*scale,
             2*scale, scale, color);
    }

    if (c == '?') {
        rect(dc, x + scale, y, 3*scale, scale, color);
        rect(dc, x + 3*scale, y + scale,
             2*scale, 2*scale, color);
        rect(dc, x + 2*scale, y + 3*scale,
             scale, 2*scale, color);
        rect(dc, x + 2*scale, y + 6*scale,
             scale, scale, color);
    }
}

static void draw_text(
    HDC dc,
    int x, int y,
    const char *text,
    uint32_t color,
    int scale)
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

        glyph(dc, x, y, *text, color, scale);

        x += 6 * scale;
        text++;
    }
}

// ----------------------------------------
// Render dialog
// ----------------------------------------

static void render(HDC dc, const char *message)
{
    rect(dc, 0, 0, W, H, 0x001B1B24);

    rect(dc, 0, 0, W, 42, 0x00272735);
    draw_text(dc, 18, 15, "MESSAGE", 0x00FFFFFF, 2);

    draw_text(dc, 24, 75, message, 0x00FFFFFF, 2);

    rect(dc, QUIT_X, QUIT_Y, BTN_W, BTN_H, 0x00444455);
    rect(dc, OK_X,   OK_Y,   BTN_W, BTN_H, 0x003B82F6);

    draw_text(dc, 132, 253, "  QUIT", 0x00FFFFFF, 2);
    draw_text(dc, 350, 253, "  OK", 0x00FFFFFF, 2);
}

// ----------------------------------------
// Window procedure
// ----------------------------------------

static LRESULT CALLBACK window_proc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    WIN32_WINDOW *w = (WIN32_WINDOW *)GetWindowLongPtrA(
        hwnd, GWLP_USERDATA
    );

    switch (msg) {

    case WM_CREATE:
        return 0;

    // We paint everything ourselves, so skip the default
    // background erase (avoids white flashes).
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);

        if (w)
            render(dc, w->message ? w->message : "");

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SETCURSOR:
        // Only override the cursor inside the client area.
        if (LOWORD(lParam) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);

            if (over_quit(pt.x, pt.y) || over_ok(pt.x, pt.y)) {
                SetCursor(LoadCursorA(NULL, IDC_HAND));
                return TRUE;
            }

            SetCursor(LoadCursorA(NULL, IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_MOUSEMOVE:
        if (w) {
            w->mouse_x = GET_X_LPARAM(lParam);
            w->mouse_y = GET_Y_LPARAM(lParam);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (!w)
            return 0;

        w->mouse_x = GET_X_LPARAM(lParam);
        w->mouse_y = GET_Y_LPARAM(lParam);

        // QUIT button
        if (over_quit(w->mouse_x, w->mouse_y)) {
            w->result = 0;
            w->running = 0;

            DestroyWindow(hwnd);
        }

        // OK button
        else if (over_ok(w->mouse_x, w->mouse_y)) {
            w->result = 1;
            w->running = 0;

            DestroyWindow(hwnd);
        }

        return 0;

    case WM_CLOSE:
        if (w) {
            w->result = 0;
            w->running = 0;
        }

        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (w) {
            w->running = 0;
        }

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ----------------------------------------
// PUBLIC API
// ----------------------------------------

int windows_native(char *text)
{
    WIN32_WINDOW w = {0};

    w.running = 1;
    w.result = -1;
    w.instance = GetModuleHandleA(NULL);
    w.message = text ? text : "";

    const char *class_name = "NativeMessageWindow";

    WNDCLASSA wc = {0};

    wc.lpfnWndProc = window_proc;
    wc.hInstance = w.instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;

    if (!RegisterClassA(&wc) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return -1;
    }

    // Compute the outer size using the FINAL (non-resizable) style,
    // so the client area is exactly W x H with no unpainted border.
    RECT r = {0, 0, W, H};

    AdjustWindowRect(
        &r,
        WINDOW_STYLE,
        FALSE
    );

    HWND hwnd = CreateWindowExA(
        0,
        class_name,
        "Message",
        WINDOW_STYLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        r.right - r.left,
        r.bottom - r.top,
        NULL,
        NULL,
        w.instance,
        NULL
    );

    if (!hwnd)
        return -1;

    w.hwnd = hwnd;

    SetWindowLongPtrA(
        hwnd,
        GWLP_USERDATA,
        (LONG_PTR)&w
    );

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Native Windows message loop.
    MSG msg;

    while (w.running) {
        BOOL status = GetMessageA(&msg, NULL, 0, 0);

        if (status <= 0)
            break;

        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return w.result;
}

// Optional alias matching the original API style.
int windows_win32(char *text)
{
    return windows_native(text);
}