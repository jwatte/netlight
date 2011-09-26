
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#define NETLIGHT_API __declspec(dllexport)
#include "NetLight.h"
#include <list>
#include <string>
#include <algorithm>
#include <vector>

#pragma warning(disable: 4996)
#pragma comment(lib, "ws2_32.lib")

//  Max packet size is length field plus the actual payload (which really maxes out at 65535 because we allow 0-length packets)
#define MAX_PACKET_SIZE (sizeof(short)+65536UL)
//  max amount of data I allow to be buffered -- if more than this, then 
//  the client will be dropped (output) or pended (input)
#define MAX_BUFFER_SIZE (2*MAX_PACKET_SIZE)

HINSTANCE ghInst;

NETLIGHT_API BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    ghInst = hinstDLL;
}

struct Packet {
    Packet(Connection *c, void const *d, unsigned short sz) {
        this->c = c;
        this->d = new char[sz + 1];
        this->sz = sz;
        memcpy(this->d, d, sz);
        //  make people who do bad things with strings slightly more safe
        this->d[sz] = 0;
    }
    ~Packet() {
        delete[] d;
    }
    Connection *c;
    char *d;
    unsigned short sz;
};

struct Connection {
    bool alive() {
        return h_ != INVALID_HANDLE_VALUE;
    }
    char const *address() {
        return addr_.c_str();
    }
    void close() {
        if (h_ != INVALID_HANDLE_VALUE) {
            ::closesocket((SOCKET)h_);
            h_ = INVALID_HANDLE_VALUE;
        }
    }
    void poll() {
    again:
        if (inBuf_.size() < MAX_BUFFER_SIZE && alive()) {
            char buf[MAX_PACKET_SIZE];
            int r = ::recv((SOCKET)h_, buf, MAX_PACKET_SIZE, 0);
            if (r > 0) {
                inBuf_.insert(inBuf_.end(), &buf[0], &buf[r]);
                goto again;
            }
            else {
                int err = WSAGetLastError();
                if (err == WSAEINPROGRESS || err == WSAEWOULDBLOCK) {
                    //  do nothing
                }
                else {
                    //  connection died
                    close();
                }
            }
        }
        if (outBuf_.size() > 0 && alive()) {
            int i = ::send((SOCKET)h_, &outBuf_[0], outBuf_.size(), 0);
            if (i > 0) {
                outBuf_.erase(outBuf_.begin(), outBuf_.begin() + i);
            }
            else if (i < 0) {
                int err = WSAGetLastError();
                if (err == WSAEINPROGRESS || err == WSAENOBUFS || err == WSAEWOULDBLOCK) {
                    //  do nothing
                }
                else {
                    //  connection died
                    close();
                }
            }
        }
    }
    Packet *receivePacket() {
        for (int i = 0; i < 2; ++i) {
            Packet *r = 0;
            if (inBuf_.size() < sizeof(short)) {
                goto dopoll;
            }
            unsigned short size = inBuf_[0] + inBuf_[1] * 256;
            if (inBuf_.size() < size + sizeof(short)) {
                goto dopoll;
            }
            r = new Packet(this, &inBuf_[sizeof(short)], size);
            inBuf_.erase(inBuf_.begin(), inBuf_.begin() + sizeof(short) + size);
            return r;
        dopoll:
            if (i == 0) {
                poll();
            }
            //  else break out
        }
    }
    Connection() {
        h_ = INVALID_HANDLE_VALUE;
    }
    Connection(HANDLE h, void const *saddr, int alen) {
        h_ = h;
        char buf[50];
        if (((sockaddr const *)saddr)->sa_family == AF_INET) {
            unsigned char const *uc = (unsigned char const *)&((sockaddr_in const *)saddr)->sin_addr;
            sprintf(buf, "%d.%d.%d.%d:%d", uc[0], uc[1], uc[2], uc[3], ntohs(((sockaddr_in const *)saddr)->sin_port));
            addr_ = buf;
        }
        else {
            sprintf(buf, "unknown family %d", ((sockaddr const *)saddr)->sa_family);
        }
    }
    ~Connection() {
        if (h_ != INVALID_HANDLE_VALUE) {
            ::closesocket((SOCKET)h_);
        }
        for (std::list<Packet *>::iterator ptr(packets_.begin()), end(packets_.end()); ptr != end; ++ptr) {
            delete *ptr;
        }
    }
    HANDLE h_;
    std::string addr_;
    std::list<Packet *> packets_;
    std::vector<char> inBuf_;
    std::vector<char> outBuf_;
};


class NetLightImpl : public NetLight {
public:
    NetLightImpl() {
        acceptSocket_ = INVALID_HANDLE_VALUE;
    }
    virtual Connection *getNewConnection() {
        if (inConnections_.empty()) {
            poll(); //  this means, don't deliver packets while connections are pending
            if (inConnections_.empty()) {
                return 0;
            }
        }
        Connection *r = inConnections_.front();
        activeConnections_.push_back(r);
        inConnections_.pop_front();
        return r;
    }
    virtual bool connectionIsAlive(Connection *c) {
        return c->alive();
    }
    virtual char const *getConnectionAddress(Connection *c) {
        return c->address();
    }
    virtual void destroyConnection(Connection *c) {
        delete c;
        activeConnections_.erase(std::find(activeConnections_.begin(), activeConnections_.end(), c));
    }
    virtual void sendPacketToConnection(void const *data, unsigned short size, Connection *c) {
        if (c->outBuf_.size() >= MAX_BUFFER_SIZE || !c->alive()) {
            c->close();
            return;
        }
        c->outBuf_.insert(c->outBuf_.end(), (char const *)&size, (char const *)&size + sizeof(short));
        c->outBuf_.insert(c->outBuf_.end(), (char const *)data, (char const *)data + size);
    }
    virtual void sendPacketToAllConnections(void const *data, unsigned short size) {
        for (std::list<Connection *>::iterator ptr(activeConnections_.begin()), end(activeConnections_.end());
            ptr != end; ++ptr) {
            sendPacketToConnection(data, size, *ptr);
        }
    }
    virtual Packet *receivePacketFromConnection(Connection *c) {
        if (!c->alive()) {
            return 0;
        }
        return c->receivePacket();
    }
    virtual Connection *packetConnection(Packet *p) {
        return p->c;
    }
    virtual void *packetData(Packet *p) {
        return p->d;
    }
    virtual unsigned short packetSize(Packet *p) {
        return p->sz;
    }
    virtual void destroyPacket(Packet *p) {
        delete p;
    }
    virtual char const *version() {
        return "1.0 " __DATE__ " Copyright 2011 Jon Watte (www.enchantedage.com)";
    }
    virtual void shutdown() {
        delete this;
    }

    HANDLE acceptSocket_;
    std::list<Connection *> inConnections_;
    std::list<Connection *> activeConnections_;

    void poll() {
        if (acceptSocket_ != INVALID_HANDLE_VALUE) {
            while (true) {
                char addr[256];
                int alen = 256;
                SOCKET s = ::accept((SOCKET)acceptSocket_, (sockaddr *)&addr, &alen);
                if (s == -1 || (HANDLE)s == INVALID_HANDLE_VALUE) {
                    break;
                }
                int i = -1;
                ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char const *)&i, sizeof(i));
                Connection *c = new Connection((HANDLE)s, addr, alen);
                inConnections_.push_back(c);
            }
        }
    }
};

bool inited;

bool init() {
    if (!inited) {
        WSAData wsa;
        memset(&wsa, 0, sizeof(wsa));
        if (WSAStartup(MAKEWORD(2, 2), &wsa) < 0) {
            return false;
        }
        inited = true;
    }
    return true;
}


NETLIGHT_API NetLight * OpenNetLightServer(unsigned short port) {
    if (port == 0) {
        return 0;
    }

    if (!init()) {
        return 0;
    }

    SOCKET s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1 || (HANDLE)s == INVALID_HANDLE_VALUE) {
        return 0;
    }

    u_long dw = -1;
    if (::ioctlsocket(s, FIONBIO, &dw) < 0) {
        ::closesocket(s);
        return 0;
    }

    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if (::bind(s, (sockaddr const *)&sin, sizeof(sin)) < 0) {
        ::closesocket(s);
        return 0;
    }

    if (::listen(s, 10) < 0) {
        ::closesocket(s);
        return 0;
    }

    NetLightImpl *nli = new NetLightImpl();
    nli->acceptSocket_ = (HANDLE)s;
    return nli;
}

NETLIGHT_API NetLight * OpenNetLightClient(char const *name, unsigned short port)
{
    if (port == 0 || name == 0 || name[0] == 0) {
        return 0;
    }

    if (!init()) {
        return 0;
    }

    hostent *hent = ::gethostbyname(name);
    if (hent == 0 || hent->h_addr_list[0] == 0 || hent->h_length != 4) {
        return 0;
    }

    SOCKET s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1 || (HANDLE)s == INVALID_HANDLE_VALUE) {
        return 0;
    }

    sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    memcpy(&sin.sin_addr, hent->h_addr_list[0], 4);
    if (::connect(s, (sockaddr const *)&sin, sizeof(sin)) < 0) {
        ::closesocket(s);
        return 0;
    }

    u_long dw = -1;
    if (::ioctlsocket(s, FIONBIO, &dw) < 0) {
        ::closesocket(s);
        return 0;
    }

    int i = -1;
    ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char const *)&i, sizeof(i));

    NetLightImpl *nli = new NetLightImpl();
    nli->inConnections_.push_back(new Connection((HANDLE)s, &sin, sizeof(sin)));

    return nli;
}

