
#ifndef __OSPF_H
#define __OSPF_H

#include <omnetpp.h>
#include <vector>
#include <map>
#include <cstdint>
#include "ospf_info_router.h"
#include "ospf_packet.h"

using namespace omnetpp;
using namespace std;

class routerOspf : public cSimpleModule
{
    private:
        OspfRouterState* state;
        uint32_t routerId;
    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void finish() override;
};

#endif

