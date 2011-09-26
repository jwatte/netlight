
#include <Windows.h>
#include <list>
#include "NetLight.h"

#pragma comment(lib, "netlight.lib")
#pragma warning(disable: 4996)

HINSTANCE ghInst;
ATOM aWClass;
HWND hwMain;
NetLight *nl;
HBRUSH brWhite;
HFONT fStatus;
HCURSOR cArrow;
bool dirty;

double startStep;
double prevStep;
int stepno = 0, remoteStepno = 0;
bool gamerunning = false;
Connection *c;


void ConnectToServer(char const *name) {
    //  netlight port for pong is 5718, let's say!
    nl = ::OpenNetLightClient(name, 5718);
    if (nl != NULL) {
        c = nl->getNewConnection();
        if (!c) {
            //  refused to connect?
            nl->shutdown();
            nl = NULL;
        }
    }
}

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
    gamerunning = true;
}

void Dispatch(char const *data) {
    if (!strcmp(data, "start")) {
        StartGame();
    }
    else if (!strcmp(data, "drop")) {
        //  other game dropped
        nl->destroyConnection(c);
        c = NULL;
        return;
    }
    else if (!strncmp(data, "step ", 5)) {
        //  whatever
        remoteStepno = atoi(&data[5]);
    }
    else {
        //  unknown packet
    }
}

void StepGame() {
    Packet *p = 0;
    while (c != 0 && (p = nl->receivePacketFromConnection(c)) != 0) {
        //  deal with packet -- it's guaranteed to have a 0 at the end (!)
        Dispatch((char const *)nl->packetData(p));
        nl->destroyPacket(p);
    }
    if (c && !nl->connectionIsAlive(c)) {
        nl->destroyConnection(c);
        c = NULL;
        nl->shutdown();
        nl = NULL;
    }
    if (!c) {
        return;
    }
    if (!gamerunning) {
        return;
    }
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
        ++stepno;
        if (!(stepno & 31)) {
            char buf[30];
            //  I'm using text packets here. However, I could just as well use binary packets (a struct, say)
            sprintf(buf, "step %d", stepno);
            nl->sendPacketToAllConnections(buf, strlen(buf));
        }
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
                else if (!c) {
                    wchar_t const *str = L"Remote client disconnected.";
                    ::TextOutW(dc, 10, 10, str, wcslen(str));
                }
                else {
                    if (!gamerunning) {
                        wchar_t const *str = L"Waiting for game start.";
                        ::TextOutW(dc, 10, 10, str, wcslen(str));
                    }
                    else {
                        char buf[100];
                        sprintf(buf, "step %d remote %d", stepno, remoteStepno);
                        ::TextOutA(dc, 10, 10, buf, strlen(buf));
                    }
                }
                ::EndPaint(hwnd, &ps);
            }
            return 0;
        case WM_MOUSEMOVE: {
                ::SetCursor(cArrow);
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
                    MakeDirty();
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
    wcex.lpszClassName = L"Pong Client Class";
    wcex.style = CS_OWNDC;
    aWClass = ::RegisterClassExW(&wcex);
    brWhite = ::CreateSolidBrush(RGB(255, 255, 255));
    fStatus = ::CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_SWISS, L"Verdana");
    cArrow = ::LoadCursor(NULL, IDC_ARROW);
}

void OpenWindow() {
    hwMain = ::CreateWindowExW(WS_EX_APPWINDOW, (wchar_t const *)aWClass, L"Pong Client", WS_OVERLAPPEDWINDOW, 100, 100, 600, 400, 0, 0, ghInst, 0);
    ::ShowWindow(hwMain, SW_NORMAL);
}

void Run() {
    ConnectToServer("localhost");
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
