
#if !defined(NetLight_h)
#define NetLight_h

#if !defined(NETLIGHT_API)
#define NETLIGHT_API 
#endif

struct Connection;
struct Packet;

class NetLight
{
public:
    virtual Connection *getNewConnection() = 0;
    virtual bool connectionIsAlive(Connection *c) = 0;
    virtual char const *getConnectionAddress(Connection *c) = 0;
    virtual void destroyConnection(Connection *c) = 0;
    virtual void sendPacketToConnection(void const *data, unsigned short size, Connection *c) = 0;
    virtual void sendPacketToAllConnections(void const *data, unsigned short size) = 0;
    virtual Packet *receivePacketFromConnection(Connection *c) = 0;
    virtual void *packetData(Packet *p) = 0;
    virtual unsigned short packetSize(Packet *p) = 0;
    virtual void destroyPacket(Packet *p) = 0;
    virtual char const *version() = 0;
    virtual void shutdown() = 0;
};

NETLIGHT_API NetLight *OpenNetLightServer(unsigned short port);
NETLIGHT_API NetLight *OpenNetLightClient(char const *addr, unsigned short port);

#endif  //  NetLight_h
