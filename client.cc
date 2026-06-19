#include "client.h"
#include "ospf_m.h"
#include "ospf_struct.h"  // for OspfRouterState, logTransition helper via extern?

Define_Module(Client);

void Client::initialize()
{
    // Router ID từ tên module (c1 → 1, c10 → 10)
    std::string name = getName();
    routerId = std::stoi(name.substr(1));

    // Schedule testTimer tại t=11.1s (sau khi SPF đã hoàn tất ở tất cả router)
    testTimer = new cMessage("testTimer");
    scheduleAt(simTime() + 11.1, testTimer);
}

void Client::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        if (msg == testTimer) {
            sendTestPackets();
            testTimer = nullptr;
            delete msg;
        }
        return;
    }

    // Nhận data packet từ router
    Mess* dataMsg = dynamic_cast<Mess*>(msg);
    if (dataMsg) {
        // Parse source từ payload bytes 4-7
        uint32_t srcId = ((uint32_t)dataMsg->getPayload(4) << 24)
                       | ((uint32_t)dataMsg->getPayload(5) << 16)
                       | ((uint32_t)dataMsg->getPayload(6) << 8)
                       |  dataMsg->getPayload(7);

        // Ghi log nhận (dùng EV hoặc tự ghi file)
        EV << "R" << routerId << " Rcvd:" << srcId << "->self" << std::endl;

        delete msg;
        return;
    }

    delete msg;
}

void Client::sendTestPackets()
{
    // Generate 9 test packets (one to each router 1..10 except self)
    for (uint32_t destId = 1; destId <= 10; destId++) {
        if (destId == routerId) continue;

        Mess* msg = new Mess("dataTest");
        msg->setPayloadArraySize(8);

        // Bytes 0-3: destination Router ID
        msg->setPayload(0, (destId >> 24) & 0xFF);
        msg->setPayload(1, (destId >> 16) & 0xFF);
        msg->setPayload(2, (destId >> 8) & 0xFF);
        msg->setPayload(3, destId & 0xFF);

        // Bytes 4-7: source Router ID (self)
        msg->setPayload(4, (routerId >> 24) & 0xFF);
        msg->setPayload(5, (routerId >> 16) & 0xFF);
        msg->setPayload(6, (routerId >> 8) & 0xFF);
        msg->setPayload(7, routerId & 0xFF);

        send(msg, "gate$o");
    }
}

void Client::finish()
{
    if (testTimer) {
        cancelAndDelete(testTimer);
        testTimer = nullptr;
    }
}
