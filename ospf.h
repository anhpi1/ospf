
#ifndef __OSPF_H
#define __OSPF_H


#include <vector>
#include <map>
#include <cstdint>
#include "ospf_struct.h"

using namespace omnetpp;
using namespace std;

class routerOspf : public cSimpleModule
{
    private:
        OspfRouterState* state;
        uint32_t routerId;
        cMessage* helloTimer;
        cMessage* spfTimer;      // SPF delay timer (Section 16)
        cMessage* testTimer;     // 2b: forwarding test trigger timer

       
    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void finish() override;

    public:
         // SPF calculation (Phase 2a)
        void calculateSpf();     // Dijkstra Stage 1 + Stage 2 + build routing table
        unsigned int calcNextHop(uint32_t destId); // next-hop gate index cho direct neighbor
        // Phase 2b: Data forwarding test
        void forwardData(Mess* msg, int ifIndex);    // forward data packet (Section 11.1)
        void initForwardingTest();   
                         // initiate 9 test packets
};

#endif

