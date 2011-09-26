
#include <Windows.h>
#include <list>
#include "NetLight.h"

#pragma comment(lib, "netlight.lib")

HINSTANCE ghInst;
ATOM aWClass;
HWND hwMain;
NetLight *nl;
HBRUSH brWhite;
HFONT fStatus;
bool dirty;

double startStep;
double prevStep;


double readTime() {
    unsigned long long li = 0, pf = 0;
    ::QueryPerformanceCounter((LARGE_INTEGER *)&li);
    //  this should be done once...
    ::QueryPerformanceFrequency((LARGE_INTEGER *)&pf);
    double d = (double)li;
    d = d / (double)pf;
    return d;
}


void MakeDirty() {
    if (!dirty) {
        dirty = true;
        ::InvalidateRect(hwMain, 0, TRUE);
    }
}

void StartGame() {
    startStep = readTime();
    prevStep = startStep;
}

void StepGame() {
    double t = readTime();
    //  30 fps
    while (t - prevStep > 0.033) {
        if (t - prevStep > 1.0) {
            prevStep = t;
        }
        else {
            prevStep = prevStep + 0.033;
        }
        //  update physics
    }
}

LRESULT CALLBACK MyWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
                ::SetTimer(hwnd, 1, 10, 0);
            }
            break;
        case WM_PAINT: {
                dirty = false;
                PAINTSTRUCT ps;
                HDC dc = ::BeginPaint(hwnd, &ps);
                ::SelectObject(dc, fStatus);
                ::SelectObject(dc, brWhite);
                ::SetTextColor(dc, RGB(255, 255, 255));
                ::SetBkColor(dc, RGB(0, 0, 0));
                if (!nl) {
                    wchar_t const *str = L"Error connecting to server.";
                    ::TextOutW(dc, 10, 10, str, wcslen(str));
                }
                else {
                }
                ::EndPaint(hwnd, &ps);
            }
            return 0;
        case WM_CLOSE: {
                ::DestroyWindow(hwnd);
            }
            return 0;
        case WM_DESTROY: {
                ::PostQuitMessage(0);
            }
            break;
        case WM_TIMER: {
                if (nl != 0) {
                    StepGame();
                }
            }
            return 0;
    }
    return ::DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void CreateClass() {
    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(wcex));
    wcex.cbSize = sizeof(wcex);
    wcex.hbrBackground = ::CreateSolidBrush(RGB(0, 0, 0));
    wcex.hIcon = ::LoadIconW(ghInst, L"Icon");
    wcex.hIconSm = ::LoadIconW(ghInst, L"IconSm");
    wcex.hInstance = ghInst;
    wcex.lpfnWndProc = &MyWndProc;
    wcex.lpszClassName = L"Pong Server Class";
    wcex.style = CS_OWNDC;
    aWClass = ::RegisterClassExW(&wcex);
    brWhite = ::CreateSolidBrush(RGB(255, 255, 255));
    fStatus = ::CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_SWISS, L"Verdana");
}

void OpenWindow() {
    hwMain = ::CreateWindowExW(WS_EX_APPWINDOW, (wchar_t const *)aWClass, L"Pong Server", WS_OVERLAPPEDWINDOW, 100, 100, 600, 400, 0, 0, ghInst, 0);
    ::ShowWindow(hwMain, SW_NORMAL);
}

void Run() {
    //  netlight port for pong is 5718, let's say!
    nl = ::OpenNetLightClient("localhost", 5718);
    MSG msg;
    StartGame();
    while (::GetMessageW(&msg, 0, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}

int WINAPI WinMain(HINSTANCE me, HINSTANCE x, LPSTR cmdline, int cmdShow) {
    ghInst = me;
    CreateClass();
    OpenWindow();
    Run();
    if (nl != 0) {
        nl->shutdown();
    }
    return 0;
}
