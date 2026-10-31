#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <vector>

// -----------------------------------------------------------------------------
// YOUR CANVAS / IMAGE BUFFER
// -----------------------------------------------------------------------------
int g_image_width = 800;
int g_image_height = 600;

// Packed 32-bit pixels (BGRA layout for Windows GDI: 0x00RRGGBB in little-endian)
std::vector<uint32_t> g_pixel_buffer;

void load_and_decode_uaf(const char* filepath) {

    g_pixel_buffer.resize(g_image_width * g_image_height);
    for (int y = 0; y < g_image_height; ++y) {
        for (int x = 0; x < g_image_width; ++x) {
            uint8_t r = 128;
            uint8_t g = 128;
            uint8_t b = 128;

            // In 32-bit little-endian memory: (B) | (G << 8) | (R << 16)
            g_pixel_buffer[y * g_image_width + x] = (r << 16) | (g << 8) | b;
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            if (!g_pixel_buffer.empty()) {
                // Describe the pixel format to GDI
                BITMAPINFO bmi = {};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = g_image_width;
                // Negative height specifies top-down orientation (row 0 is top)
                bmi.bmiHeader.biHeight = -g_image_height;
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                // Draw pixels directly to the window HDC
                StretchDIBits(
                    hdc,
                    0, 0, g_image_width, g_image_height,  // Destination (x, y, w, h)
                    0, 0, g_image_width, g_image_height,  // Source (x, y, w, h)
                    g_pixel_buffer.data(),
                    &bmi,
                    DIB_RGB_COLORS,
                    SRCCOPY
                );
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }

        case WM_KEYDOWN: {
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
                return 0;
            }
            break;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    // Optional: Pass filename via command line, e.g. "viewer.exe my_image.uaf"
    const char* filename = (lpCmdLine && lpCmdLine[0] != '\0') ? lpCmdLine : "test.uaf";
    load_and_decode_uaf(filename);

    const wchar_t CLASS_NAME[] = L"UafViewerWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    // Calculate window rectangle so client area fits the image exactly
    RECT wr = { 0, 0, g_image_width, g_image_height };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"UAF Image Viewer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left,
        wr.bottom - wr.top,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);

    // Standard Win32 Message Loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
