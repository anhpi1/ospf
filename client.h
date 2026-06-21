#ifndef __CLIENT_H
#define __CLIENT_H

#include <cstdint>
#include <omnetpp.h>

using namespace omnetpp;

class Client : public cSimpleModule
{
    private:
        uint32_t routerId;

    protected:
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void finish() override;
};

#endif
