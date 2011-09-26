
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



struct Game {
    Game() {
        a = 0;
        b = 0;
    }
    Connection *a;
    Connection *b;
};

Game *next;
std::list<Game *> games;
bool dirty;

void MakeDirty() {
    if (!dirty) {
        dirty = true;
        ::InvalidateRect(hwMain, 0, TRUE);
    }
}

void StartGame(Game *g) {
    games.push_back(g);
    nl->sendPacketToConnection("start", 5, g->a);
    nl->sendPacketToConnection("start", 5, g->b);
}

void CheckNewConnections() {
    Connection *c;
    while ((c = nl->getNewConnection()) != NULL) {
        MakeDirty();
        if (next) {
            next->b = c;
            StartGame(next);
            next = NULL;
        }
        else {
            next = new Game();
            next->a = c;
        }
    }
}

void UpdateGames() {
    for (std::list<Game *>::iterator ptr(games.begin()), end(games.end()); ptr != end; ++ptr) {
        //  Forward one packet per client per tick, to limit tick rate.
        //  Also limit size of each packet forwarded so that attempts to flood don't work.
        Game *g = *ptr;
        if (g->a == 0 || g->b == 0) {
            //  nothing to do
            return;
        }
        if (!nl->connectionIsAlive(g->a)) {
            nl->destroyConnection(g->a);
            g->a = 0;
            if (g->b) {
                nl->sendPacketToConnection("drop", 4, g->b);
            }
        }
        else {
            //  read packet and forward
            Packet *p = nl->receivePacketFromConnection(g->a);
            if (p != 0) {
                if (nl->packetSize(p) < 64 && g->b != 0) {
                    nl->sendPacketToConnection(nl->packetData(p), nl->packetSize(p), g->b);
                }
                else {
                    //  guy is bad, drop him!
                    nl->destroyConnection(g->a);
                    g->a = 0;
                }
                nl->destroyPacket(p);
            }
        }
        if (!nl->connectionIsAlive(g->b)) {
            nl->destroyConnection(g->b);
            g->b = 0;
            if (g->a) {
                nl->sendPacketToConnection("drop", 4, g->a);
            }
        }
        else {
            //  read packet and forward
            Packet *p = nl->receivePacketFromConnection(g->b);
            if (p != 0) {
                if (nl->packetSize(p) < 64 && g->a != 0) {
                    nl->sendPacketToConnection(nl->packetData(p), nl->packetSize(p), g->a);
                }
                else {
                    //  guy is bad, drop him!
                    nl->destroyConnection(g->b);
                    g->b = 0;
                }
                nl->destroyPacket(p);
            }
        }
    }
}

void RemoveEmptyGames() {
    for (std::list<Game *>::iterator ptr(games.begin()), end(games.begin()), del; ptr != end;) {
        del = ptr;
        Game *g = *ptr;
         ++ptr;
         if (g->a == 0 && g->b == 0) {
            games.erase(del);
            delete g;
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
                if (!nl) {
                    wchar_t const *str = L"Error opening server port.";
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
                if (nl != NULL) {
                    CheckNewConnections();
                    UpdateGames();
                    RemoveEmptyGames();
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
}

void Run()
{
    //  netlight port for pong is 5718, let's say!
    nl = ::OpenNetLightServer(5718);
    MSG msg;
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
    return 0;
}
