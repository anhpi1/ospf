
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
    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void finish() override;
};

#endif

