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
        if (msg == helloTimer) {
            // Gửi Hello trên tất cả interface
            for (int i = 0; i < (int)state->interfaces.size(); i++) {
                helloData::sendHello(i, *state, routerId, this);
            }
            scheduleAt(simTime() + state->interfaces[0].helloInterval, msg);
        } else {
            // rxmtTimer cháy
            int ifIndex = msg->getKind();
            InterfaceData* iface = &state->interfaces[ifIndex];
            NeighborData* nbr = iface->neighbor;
            if (nbr && nbr->state == NBR_EXSTART) {
                // BƯỚC 3: retransmit DD rỗng
                nbr->rxmtTimer = nullptr;
                databaseDescriptionData::sendExStart(iface, ifIndex, routerId, this);
            } else if (nbr && nbr->state >= NBR_EXCHANGE) {
                // BƯỚC 4: retransmit DD trong Exchange
                nbr->rxmtTimer = nullptr;
                if (nbr->isMaster)
                    databaseDescriptionData::sendExchange(iface, ifIndex, routerId, *state, this);
                // Slave: gửi lại ACK (rebuild từ LSDB)
                // → không cần làm gì thêm, Master sẽ retransmit
            } else if (nbr && nbr->state == NBR_LOADING) {
                // BƯỚC 5: retransmit LS Request trong Loading (RFC 10.9)
                nbr->rxmtTimer = nullptr;
                linkStateRequestData::sendLSR(iface, ifIndex, routerId,
                                              nbr->linkStateRetransmissionList, this);
            } else if (nbr) {
                nbr->rxmtTimer = nullptr;
            }
            delete msg;
        }
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
            helloData::processHello(hdr, data, iface, ifIndex, routerId, this);
        } else if (pktType == 2) {
            NeighborData* nbr = iface->neighbor;
            if (nbr->state == NBR_EXSTART) {
                databaseDescriptionData::processExStart(hdr, data, iface, ifIndex, routerId, *state, this);
            } else if (nbr->state >= NBR_EXCHANGE) {
                if (nbr->isMaster)
                    databaseDescriptionData::processExchangeForMaster(hdr, data, iface, ifIndex, routerId, *state, this);
                else
                    databaseDescriptionData::processExchangeForSlave(hdr, data, iface, ifIndex, routerId, *state, this);
            }
        } else if (pktType == 3) {
            // FLOW B: neighbor xin LSA từ mình
            linkStateRequestData::processLSR(hdr, data, iface, ifIndex, routerId, *state, this);
        } else if (pktType == 4) {
            // FLOW A: nhận LSU → cài LSDB
            linkStateUpdateData::processLSU(hdr, data, iface, ifIndex, routerId, *state, this);
        } else if (pktType == 5) {
            // Nhận LSAck → xóa khỏi retransmission list
            linkStateAcknowledgementData::processAck(hdr, data, iface, ifIndex, *state, this);
        }
        delete msg;
    }
}

void routerOspf::finish()
{
    cancelAndDelete(helloTimer);
    // Dọn timer của từng neighbor
    for (auto& iface : state->interfaces) {
        NeighborData* nbr = iface.neighbor;
        if (nbr) {
            if (nbr->inactivityTimer)
                cancelAndDelete(nbr->inactivityTimer);
            if (nbr->rxmtTimer)
                cancelAndDelete(nbr->rxmtTimer);
        }
    }
    state->printState();
    delete state;
}
