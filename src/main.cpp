#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <print>

using namespace std;

int g_image_width = 0;
int g_image_height = 0;

vector<uint32_t> g_pixel_buffer;

void load_and_decode_uaf(const char* filepath) {
    ifstream file(filepath, ios::binary);
    if (!file.is_open()) {
        cerr << "Failed to open file: " << filepath << "\n";
        return;
    }

    string token;
    
    // Expect ::{ global namespace entry
    if (!(file >> token) || token != "::{") {
        cerr << "Could not find UAF/Graveyard global namespace entry. Aborting.\n";
        return;
    }

    // Expect !!uafheader operator
    if(!(file >> token) or token != "!!uafheader") {
        cerr << "File does not have a uafheader node. Aborting.\n";
        return;
    }

    // Expect {<uafheader key value pairs>}
    vector<string> uaf_header;
    while (file >> token) {
        if (!token.empty() && token.back() == ';') {
            token.pop_back();
            if (!token.empty()) {
                uaf_header.push_back(token);
            }
            break;
        }
        uaf_header.push_back(token);
    }
    
    string uaf_version;
    for (int i = 0; i < uaf_header.size(); i++) {
        if (uaf_header[i] == "version") {
            uaf_version = uaf_header[i + 1];
        }
    }
    
    if (uaf_version != "0.1") {
        cerr << "UAF file uses unknown version: " << uaf_version << ". Aborting.\n";
        return;
    }

    // Expect !!uafimage operator
    if(!(file >> token) or token != "!!uafimage") {
        cerr << "File does not have a uafimage node. Aborting.\n";
        return;
    }

    // Expect {<uaf image key value pairs>}
    vector<string> uaf_image_header;
    while (file >> token) {
        if (!token.empty() && token.back() == '}') {
            token.pop_back();
            if (!token.empty()) {
                uaf_image_header.push_back(token);
            }
            break;
        }
        uaf_image_header.push_back(token);
    }
    
    for (int i = 0; i < uaf_image_header.size(); i++) {
        if (uaf_image_header[i] == "width") {
            g_image_width = stoi(uaf_image_header[i + 1]);
        }
        if (uaf_image_header[i] == "height") {
            g_image_height = stoi(uaf_image_header[i + 1]);
        }
    }
    g_pixel_buffer.resize(g_image_width * g_image_height);

    // Expect rgb triplets
    vector<string> uaf_image_data;
    while (file >> token) {
        if (token == ";") {
            break;
        }
        uaf_image_data.push_back(token);
    }
    
    for (int y = 0; y < g_image_height; ++y) {
        for (int x = 0; x < g_image_width; ++x) {
            int pixel_index = y * g_image_width + x;
            int raw_index = pixel_index * 3;

            uint8_t r = stoi(uaf_image_data[raw_index]);
            uint8_t g = stoi(uaf_image_data[raw_index + 1]);
            uint8_t b = stoi(uaf_image_data[raw_index + 2]);

            g_pixel_buffer[y * g_image_width + x] = (r << 16) | (g << 8) | b;
        }
    }

    file.close();
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
    const char* filename = (lpCmdLine && lpCmdLine[0] != '\0') ? lpCmdLine : "render.uaf";
    load_and_decode_uaf(filename);

    const wchar_t CLASS_NAME[] = L"UafViewerWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    RECT wr = { 0, 0, g_image_width, g_image_height };
    AdjustWindowRect(&wr, window_style, FALSE);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"UAF Image Viewer",
        window_style,
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

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
