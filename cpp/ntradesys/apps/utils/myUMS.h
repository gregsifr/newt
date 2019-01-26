
#ifndef __UMS_H
#define __UMS_H

#include "Common/SockMessage.h"
#include "Common/TowerNet.h"
#include "Common/TowerPort.h"

#include <vector>

class UDPSocket;

/** UserMessage Sender -- sends user messages through broadcast,
    bypassing the daemons. */
class UMS {
public:
    /// Creates a UserMessage sender (def = NULL)
    UMS (const char *addr = NULL);
    /// Sends this user message out on the set ports to the set interface
    bool Send (int code, const char *msg1, const char *msg2);
    /// Adds a port to the list to broadcast to.
    void AddPort (int port);
    /// Clears out broadcast list.
    void ClearPorts ();
    /// Sets the broadcast interface.
    void SetInterface (const char *_addr);
    const char *GetInterface () const { return addr; }

private:
    const char *addr;
    UDPSocket *socket;
    typedef std::vector<int> port_set;
    port_set ports;
    int index;
    SockMessageStruct sms;
};

#endif
