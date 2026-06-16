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

    // InterfaceUp: Down → PointToPoint (Section 9.3, P2P)
    for (int i = 0; i < n; i++)
        state->interfaces[i].state = IF_POINTTOPOINT;

    // Tạo Router-LSA đầu tiên với stub links (Section 12.4.1)
    state->originateRouterLSA();

    // Gửi Hello ngay lập tức trên tất cả interface
    for (int i = 0; i < n; i++) {
        helloData::sendHello(i, *state, routerId, this);
    }

    // Đặt lịch gửi Hello định kỳ
    helloTimer = new cMessage("helloTimer");
    scheduleAt(simTime() + state->interfaces[0].helloInterval, helloTimer);

    // Ghi state dump đầu tiên — trạng thái ngay sau Phase 0 (0.G)
    state->printState();
}


void routerOspf::handleMessage(cMessage *msg)
{
    // xử lý timer
    if (msg->isSelfMessage()) {
        if (msg == helloTimer) {
            // Gửi Hello trên tất cả interface
            for (int i = 0; i < (int)state->interfaces.size(); i++) {
                helloData::sendHello(i, *state, routerId, this);
            }
            scheduleAt(simTime() + state->interfaces[0].helloInterval, msg);
        } else {
            // Tìm timer nào đã cháy: inactivityTimer hoặc rxmtTimer
            bool found = false;
            for (int i = 0; i < (int)state->interfaces.size(); i++) {
                InterfaceData* iface = &state->interfaces[i];
                NeighborData* nbr = iface->neighbor;
                if (nbr && nbr->inactivityTimer == msg) {
                    nbr->inactivityTimer = nullptr;
                    nbr->state = NBR_DOWN;
                    nbr->IDNeighbor = 0;
                    nbr->databaseSummaryList.clear();
                    nbr->linkStateRequestList.clear();
                    nbr->linkStateRetransmissionList.clear();
                    state->logTransition("1a", "InactivityTimer",
                                         simTime().dbl(), i);
                    found = true;
                    break;
                }
                if (nbr && nbr->rxmtTimer == msg) {
                    // rxmtTimer cháy → retransmit DD (Section 10.8)
                    if (nbr->state == NBR_EXSTART) {
                        // ExStart: master retransmit DD rỗng I+M+MS
                        databaseDescriptionData::sendDD(i, *state, routerId, this);
                        scheduleAt(simTime() + iface->rxmtInterval, msg);
                    } else if (nbr->state == NBR_EXCHANGE && nbr->isMaster) {
                        // Exchange (Master): retransmit DD cuối
                        databaseDescriptionData::sendDD(i, *state, routerId, this);
                        scheduleAt(simTime() + iface->rxmtInterval, msg);
                    } else {
                        // Other states (Exchange/Slave, Loading, Full): cancel
                        cancelEvent(msg);
                        delete msg;
                        nbr->rxmtTimer = nullptr;
                    }
                    found = true;
                    break;
                }
            }
            if (!found) delete msg;
        }
        return;
    }

    //code chính xử lý gói tin

    int ifIndex = msg->getArrivalGate()->getIndex();
    InterfaceData* iface = &state->interfaces[ifIndex];

    headerOspf hdr;
    std::vector<uint8_t> data;
    uint8_t pktType = OspfMess::parse((Mess*)msg, iface, hdr, data);
    
    if (pktType == 0) {
        delete msg;
        return;
    }

    if (pktType == 1) {
        NeighborData* nbr = iface->neighbor;
        int oldState = nbr->state;
        bool helloOk = helloData::processHello(hdr, data, iface, ifIndex, routerId);

        if (helloOk) {
            // Reset inactivity timer (Section 10.5, bước 5)
            if (nbr->inactivityTimer) {
                cancelEvent(nbr->inactivityTimer);
                delete nbr->inactivityTimer;
            }
            nbr->inactivityTimer = new cMessage("inactivityTimer");
            scheduleAt(simTime() + iface->routerDeadInterval, nbr->inactivityTimer);

            // Log transition nếu state thay đổi (do processHello)
            if (nbr->state != oldState) {
                const char* event = nullptr;
                if (oldState == NBR_DOWN && nbr->state == NBR_INIT)
                    event = "Down→Init (HelloReceived)";
                else if (oldState <= NBR_INIT && nbr->state == NBR_TWOWAY)
                    event = "→2Way (2WayReceived)";
                else if (oldState >= NBR_TWOWAY && nbr->state == NBR_INIT)
                    event = "→Init (1WayReceived)";
                if (event)
                    state->logTransition("1a", event, simTime().dbl(), ifIndex);
            }

            // P2P: 2Way → ExStart ngay lập tức (Section 10.4 + 10.3)
            if (iface->type == 1 && nbr->state == NBR_TWOWAY) {
                oldState = nbr->state;
                nbr->state = NBR_EXSTART;
                nbr->ddSequenceNumber = (uint32_t)(simTime().raw() & 0xFFFFFFFF);
                nbr->isMaster = 1;
                nbr->lastDdIMs = DD_I | DD_M | DD_MS;
                nbr->lastDdOptions = OPT_E;
                // Gửi DD rỗng đầu tiên + schedule rxmtTimer
                databaseDescriptionData::sendDD(ifIndex, *state, routerId, this);
                if (!nbr->rxmtTimer)
                    nbr->rxmtTimer = new cMessage("rxmtTimer");
                scheduleAt(simTime() + iface->rxmtInterval, nbr->rxmtTimer);
                state->logTransition("1b1", "2Way→ExStart",
                                     simTime().dbl(), ifIndex);
            }
        }
    }

    // Database Description packet (type=2) — ExStart + Exchange
    else if (pktType == 2) {
        NeighborData* nbr = iface->neighbor;
        DdResult res = databaseDescriptionData::processDD(hdr, data, iface, routerId,
                                                           state->area.routerLSAs);

        // --- SeqNumberMismatch / lỗi → restart ExStart (Section 10.3) ---
        if (!res.valid) {
            int oldState = nbr->state;
            if (oldState >= NBR_EXSTART) {
                // Xóa 3 list + về ExStart
                nbr->state = NBR_EXSTART;
                nbr->databaseSummaryList.clear();
                nbr->linkStateRequestList.clear();
                nbr->linkStateRetransmissionList.clear();
                nbr->ddSequenceNumber++;
                nbr->isMaster = 1;
                // Cancel rxmtTimer cũ
                if (nbr->rxmtTimer) {
                    cancelEvent(nbr->rxmtTimer);
                    delete nbr->rxmtTimer;
                    nbr->rxmtTimer = nullptr;
                }
                // Gửi DD rỗng I+M+MS + schedule rxmtTimer mới
                nbr->lastDdIMs = DD_I | DD_M | DD_MS;
                nbr->lastDdOptions = OPT_E;
                databaseDescriptionData::sendDD(ifIndex, *state, routerId, this);
                nbr->rxmtTimer = new cMessage("rxmtTimer");
                scheduleAt(simTime() + iface->rxmtInterval, nbr->rxmtTimer);
                state->logTransition("1b1", "SeqNumberMismatch→ExStart",
                                     simTime().dbl(), ifIndex);
            }
            delete msg;
            return;
        }

        // --- NegotiationDone → ExStart → Exchange (Section 10.3) ---
        if (res.negotiationDone) {
            nbr->state = NBR_EXCHANGE;
            if (nbr->rxmtTimer) {
                cancelEvent(nbr->rxmtTimer);
                delete nbr->rxmtTimer;
                nbr->rxmtTimer = nullptr;
            }
            // Populate databaseSummaryList từ LSDB (Section 10.3)
            nbr->databaseSummaryList.clear();
            for (const auto& lsa : state->area.routerLSAs)
                nbr->databaseSummaryList.push_back(lsa.header);
            // Slave: gửi response (shouldSendDD=true từ ExStart case 1)
            // Master: gửi poll đầu tiên (shouldSendDD=true từ fall-through)
            // Cả 2 đều handle ở shouldSendDD check bên dưới
            // Schedule rxmtTimer mới
            nbr->rxmtTimer = new cMessage("rxmtTimer");
            scheduleAt(simTime() + iface->rxmtInterval, nbr->rxmtTimer);
            state->logTransition("1b1", "ExStart→Exchange (NegotiationDone)",
                                 simTime().dbl(), ifIndex);
        }

        // --- ExchangeDone → Loading hoặc Full (Section 10.3) ---
        if (res.exchangeDone) {
            if (nbr->linkStateRequestList.empty())
                nbr->state = NBR_FULL;
            else
                nbr->state = NBR_LOADING;
            // Cancel rxmtTimer (không còn DD)
            if (nbr->rxmtTimer) {
                cancelEvent(nbr->rxmtTimer);
                delete nbr->rxmtTimer;
                nbr->rxmtTimer = nullptr;
            }
            state->logTransition("1b1",
                nbr->state == NBR_FULL ? "ExchangeDone→Full" : "ExchangeDone→Loading",
                simTime().dbl(), ifIndex);
        }

        // --- Slave response hoặc Master poll tiếp theo ---
        if (res.shouldSendDD) {
            databaseDescriptionData::sendDD(ifIndex, *state, routerId, this);
        }
    }

    delete msg;
    
}

void routerOspf::finish()
{
    cancelAndDelete(helloTimer);
    for (auto& iface : state->interfaces) {
        NeighborData* nbr = iface.neighbor;
        if (nbr) {
            if (nbr->inactivityTimer) cancelAndDelete(nbr->inactivityTimer);
            if (nbr->rxmtTimer) cancelAndDelete(nbr->rxmtTimer);
        }
    }
    state->printState();
    delete state;
}
