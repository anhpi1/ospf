#ifndef __CLIENT_H
#define __CLIENT_H

#include <cstdint>
#include <omnetpp.h>

using namespace omnetpp;

class Client : public cSimpleModule
{
    private:
        uint32_t routerId;
        cMessage* testTimer;

    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void finish() override;

        void sendTestPackets();
};

#endif
