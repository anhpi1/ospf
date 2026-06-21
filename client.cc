#include "client.h"
#include "ospf_m.h"

Define_Module(Client);

void Client::initialize()
{
    // Router ID từ tên module (c1 → 1, c10 → 10)
    std::string name = getName();
    routerId = std::stoi(name.substr(1));

    // Client send được điều khiển qua link_flaps.txt (client send T)
}

void Client::handleMessage(cMessage *msg)
{
    // Nhận data packet từ router
    Mess* dataMsg = dynamic_cast<Mess*>(msg);
    if (dataMsg) {
        // Parse source từ payload bytes 4-7
        uint32_t srcId = ((uint32_t)dataMsg->getPayload(4) << 24)
                       | ((uint32_t)dataMsg->getPayload(5) << 16)
                       | ((uint32_t)dataMsg->getPayload(6) << 8)
                       |  dataMsg->getPayload(7);

        delete msg;
        return;
    }

    delete msg;
}

void Client::finish()
{
}
