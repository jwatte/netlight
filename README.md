NetLight simple networking library
==================================

Check out "download" for the latest release. Currently, it's only available for Windows XP and up. Porting to Linux should be easy; x86 MacOS X also except I don't have a Mac.

Link against the .lib, ship the .dll, and compile with the .h file.

This is a super early release. It does some things, but not a lot of things.

To use it in a server, call OpenNetLightServer(unsigned short port) to start listening for connections from clients.
# Then keep calling getNewConnection() to receive new connections; this returns NULL if there are currently no waiting new connections.
# Then keep calling receivePacketFromConnection() for each connection you've received to get packets sent by those connections.
# Then call packetData() and packetSize() (NetLight will make sure there is always a terminating 0 after the packet data, for your convenience!)
# For each packet you've received, call destroyPacket(), else you will be leaking packets.
# For each client, you can call clientIsAlive() to see whether it's dropped from the network (this information is updated when you try to receive data from the clietn).
# Calling destroyClient() removes the client from the list of active clients -- you should forget your own copy of the client pointer.
# Finally, if you want to stop serving data, call shutdown().

To use it in a client, call OpenNetLightServer(char const *host, unsigned short port) with the host address of the server ("localhost" for testing) and the port that the server is listening on.
# Then call getNewConnection() once, to receive the connection established to the server.
# Then call receivePacketFromConnection() and sendPacketToConnection() to communicate.
# Again, packetData() and packetSize() get the data of each packet, and you must call destroyPacket() after you've handled each packet.

There are some samples in samples\pong (a client and a server), although they are not yet a full game.

Please let me know whether this works for you at all -- "release early, gather feedback" is the mantra I'm going for here.
