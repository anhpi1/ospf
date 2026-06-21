
#ifndef __OSPF_H
#define __OSPF_H


#include <vector>
#include <map>
#include <set>
#include <cstdint>
#include "ospf_struct.h"

using namespace omnetpp;
using namespace std;

// Link Flap Event: tắt/bật link tại một thời điểm
struct LinkFlapEvent {
    simtime_t time;             // thời điểm kích hoạt
    int targetRouterId;         // router bên kia link (dùng để lookup interface)
    bool isDown;                // true=tắt link, false=bật link
};

// Client Send Event: tất cả router đồng loạt gửi data packet
struct ClientSendEvent {
    simtime_t time;             // thời điểm gửi
};

class routerOspf : public cSimpleModule
{
    private:
        OspfRouterState* state;
        uint32_t routerId;
        int numRouters;
        cMessage* helloTimer;
        cMessage* spfTimer;      // SPF delay timer (Section 16)

        // === Link Flap Scheduler ===
        std::vector<LinkFlapEvent> flapEvents;
        int flapTotal;
        int flapRemaining;
        cMessage* flapTimer;
        std::set<int> blockedInterfaces;

        // === Client Send Scheduler ===
        std::vector<ClientSendEvent> clientSendEvents;
        int clientSendRemaining;
        cMessage* clientSendTimer;


    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void finish() override;

        // Link Flap Scheduler
        void parseLinkFlaps(const char* filename);
        int findInterfaceByNeighbor(uint32_t targetId);

        // Debug: dump toàn bộ trạng thái router ra file JSON
        void dumpStateToJson(const char* dir = "log");

        // Debug: dump payload[] của Mess ra file binary mỗi lần handleMessage
        void dumpMessageBinary(cMessage *msg);

        // Qtenv: cập nhật display string (màu link + text overlay)
        void updateDisplay();

    public:
         // SPF calculation (Phase 2a)
        void calculateSpf();     // Dijkstra Stage 1 + Stage 2 + build routing table
        unsigned int calcNextHop(uint32_t destId); // next-hop gate index cho direct neighbor
        // Phase 2b: Data forwarding test
        void forwardData(Mess* msg, int ifIndex);    // forward data packet (Section 11.1)
};

#endif

