#include "ospf.h"

Define_Module(routerOspf);


void routerOspf::initialize()
{
    // Router ID từ tên module (r1 → 1, r10 → 10)
    {
        std::string name = getName();
        routerId = std::stoi(name.substr(1));
    }
    int n = gateSize("gate");
    state = new OspfRouterState(routerId, n);

    // Gửi Hello ngay lập tức trên tất cả interface
    for (int i = 0; i < n; i++) {
        helloData::sendHello(i, *state, routerId, this);
    }

    // Đặt lịch gửi Hello định kỳ
    helloTimer = new cMessage("helloTimer");
    scheduleAt(simTime() + state->interfaces[0].helloInterval, helloTimer);
}


void routerOspf::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // báo thức reo → gửi Hello trên tất cả interface
        for (int i = 0; i < (int)state->interfaces.size(); i++) {
            helloData::sendHello(i, *state, routerId, this);
        }
        scheduleAt(simTime() + state->interfaces[0].helloInterval, msg);
    } else {
        // Area ID là định danh 32-bit của area (RFC 2328 Section 3.6).
        // Header OSPF có field areaId — phải khớp areaID của interface nhận
        // thì mới chấp nhận gói (Section 8.2).
        // Dự án này tất cả đều là backbone 0.0.0.0 nên luôn khớp,
        // nhưng vẫn phải check theo RFC.
        int ifIndex = msg->getArrivalGate()->getIndex();
        InterfaceData* iface = &state->interfaces[ifIndex];

        // Tách header + kiểm tra (RFC 2328 Section 8.2)
        headerOspf hdr;
        std::vector<uint8_t> data;
        uint8_t pktType = OspfMess::parse((Mess*)msg, iface, hdr, data);
        if (pktType == 0) {
            delete msg;
            return;
        }

        // Dispatch theo type
        if (pktType == 1) {
            helloData::processHello(hdr, data, iface, routerId);
            
        }
        // TODO: type 2=DD, 3=LSR, 4=LSU, 5=LSAck
        delete msg;
    }
}

void routerOspf::finish()
{
    cancelAndDelete(helloTimer);
    state->printState();
    delete state;
}
