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

    spfTimer = nullptr;                     // SPF timer chưa dùng

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
        } else if (msg == spfTimer) {
            // 1c.F: spfTimer cháy → [TODO] SPF calculation (Giai đoạn 2a)
            spfTimer = nullptr;
            delete msg;
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
                    // rxmtTimer cháy → retransmit (Section 10.8 / 10.9)
                    if (nbr->state == NBR_EXSTART) {
                        // ExStart: master retransmit DD rỗng I+M+MS
                        databaseDescriptionData::sendDD(i, *state, routerId, this);
                        scheduleAt(simTime() + iface->rxmtInterval, msg);
                    } else if (nbr->state == NBR_EXCHANGE && nbr->isMaster) {
                        // Exchange (Master): retransmit DD cuối
                        databaseDescriptionData::sendDD(i, *state, routerId, this);
                        scheduleAt(simTime() + iface->rxmtInterval, msg);
                    } else if (nbr->state == NBR_LOADING) {
                        // Loading: retransmit LSR (Section 10.9)
                        if (!nbr->linkStateRequestList.empty()) {
                            linkStateRequestData::sendLSR(i, *state, routerId, this);
                            scheduleAt(simTime() + iface->rxmtInterval, msg);
                        } else {
                            cancelEvent(msg);
                            delete msg;
                            nbr->rxmtTimer = nullptr;
                        }
                    } else if (nbr->state == NBR_FULL) {
                        // 1c.C: Full — retransmit LSA từ linkStateRetransmissionList
                        // RFC 2328 Section 13.6 (Retransmitting LSAs)
                        if (!nbr->linkStateRetransmissionList.empty()) {
                            // Tra LSDB → xây LSU → gửi (Section 13.6)
                            std::vector<LSA> lsas;
                            for (const auto& req : nbr->linkStateRetransmissionList) {
                                for (const auto& lsa : state->area.routerLSAs) {
                                    if (lsa.header.type == req.LSType
                                        && lsa.header.linkStateId == req.linkStateId
                                        && lsa.header.advertisingRouter == req.advertisingRouter)
                                    {
                                        LSA retransLsa = lsa;
                                        // Tăng LS age InfTransDelay (Section 13.6 + 13.3(5))
                                        int newAge = retransLsa.header.age + iface->infTransDelay;
                                        if (newAge > 0xFFFF) newAge = 0xFFFF;
                                        retransLsa.header.age = (uint16_t)newAge;
                                        lsas.push_back(retransLsa);
                                        break;
                                    }
                                }
                            }
                            if (!lsas.empty())
                                linkStateUpdateData::sendLSU(i, lsas, routerId,
                                                              iface->areaID, this);
                            scheduleAt(simTime() + iface->rxmtInterval, msg);
                        } else {
                            // List rỗng: cancel timer
                            cancelEvent(msg);
                            delete msg;
                            nbr->rxmtTimer = nullptr;
                        }
                    } else {
                        // Other states: cancel
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
            bool wasFull = false;
            if (nbr->linkStateRequestList.empty()) {
                nbr->state = NBR_FULL;
                wasFull = true;
            } else
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
            // 1c: ExchangeDone→Full → originate Router-LSA (Section 12.4 Event 4)
            if (wasFull) {
                state->originateRouterLSA();
                for (auto& lsa : state->area.routerLSAs) {
                    if (lsa.header.advertisingRouter == routerId) {
                        linkStateUpdateData::floodLSA(lsa, -1, *state, routerId, this);
                        break;
                    }
                }
            }
            // Nếu vào Loading: gửi LSR đầu tiên + schedule rxmtTimer (1b2)
            if (nbr->state == NBR_LOADING) {
                linkStateRequestData::sendLSR(ifIndex, *state, routerId, this);
                nbr->rxmtTimer = new cMessage("rxmtTimer");
                scheduleAt(simTime() + iface->rxmtInterval, nbr->rxmtTimer);
            }
        }

        // --- Slave response hoặc Master poll tiếp theo ---
        if (res.shouldSendDD) {
            databaseDescriptionData::sendDD(ifIndex, *state, routerId, this);
        }
    }

    // ============================================================
    // 1b2: Link State Request (type=3)
    // RFC 2328 Section 10.7 — Receiving Link State Request packets
    // ============================================================
    else if (pktType == 3) {
        NeighborData* nbr = iface->neighbor;
        if (nbr->state < NBR_EXCHANGE) {
            delete msg; return;  // ignore (Section 10.7)
        }

        LsrResult lsrRes = linkStateRequestData::processLSR(hdr, data, iface,
                                                             state->area.routerLSAs);

        if (lsrRes.badLSReq) {
            // BadLSReq → ExStart (Section 10.7 + 10.3)
            int oldState = nbr->state;
            if (oldState >= NBR_EXSTART) {
                nbr->state = NBR_EXSTART;
                nbr->databaseSummaryList.clear();
                nbr->linkStateRequestList.clear();
                nbr->linkStateRetransmissionList.clear();
                nbr->ddSequenceNumber++;
                nbr->isMaster = 1;
                if (nbr->rxmtTimer) {
                    cancelEvent(nbr->rxmtTimer);
                    delete nbr->rxmtTimer;
                    nbr->rxmtTimer = nullptr;
                }
                nbr->lastDdIMs = DD_I | DD_M | DD_MS;
                nbr->lastDdOptions = OPT_E;
                databaseDescriptionData::sendDD(ifIndex, *state, routerId, this);
                nbr->rxmtTimer = new cMessage("rxmtTimer");
                scheduleAt(simTime() + iface->rxmtInterval, nbr->rxmtTimer);
                state->logTransition("1b2", "BadLSReq→ExStart", simTime().dbl(), ifIndex);
            }
        } else if (lsrRes.valid && !lsrRes.lsus.empty()) {
            // Gửi LSU response (Section 10.7)
            linkStateUpdateData::sendLSU(ifIndex, lsrRes.lsus,
                                          routerId, iface->areaID, this);
        }
    }

    // ============================================================
    // 1b2: Link State Update (type=4)
    // RFC 2328 Section 13 (Flooding Procedure)
    // ============================================================
    else if (pktType == 4) {
        NeighborData* nbr = iface->neighbor;
        if (nbr->state < NBR_EXCHANGE) {
            delete msg; return;  // drop (Section 13)
        }

        // processLSU: validate LSA → so sánh LSDB → cài / ACK / discard
        // Truncate linkStateRequestList cho các LSA response
        LsuResult lsuRes = linkStateUpdateData::processLSU(
            hdr, data, iface, state->area, nbr->linkStateRequestList);

        if (lsuRes.badLSReq) {
            // Section 13 (6): BadLSReq
            if (nbr->state >= NBR_EXSTART) {
                nbr->state = NBR_EXSTART;
                nbr->databaseSummaryList.clear();
                nbr->linkStateRequestList.clear();
                nbr->linkStateRetransmissionList.clear();
                nbr->ddSequenceNumber++;
                nbr->isMaster = 1;
                if (nbr->rxmtTimer) {
                    cancelEvent(nbr->rxmtTimer);
                    delete nbr->rxmtTimer; nbr->rxmtTimer = nullptr;
                }
                nbr->lastDdIMs = DD_I | DD_M | DD_MS;
                nbr->lastDdOptions = OPT_E;
                databaseDescriptionData::sendDD(ifIndex, *state, routerId, this);
                nbr->rxmtTimer = new cMessage("rxmtTimer");
                scheduleAt(simTime() + iface->rxmtInterval, nbr->rxmtTimer);
                state->logTransition("1b2", "BadLSReq(LSU)→ExStart",
                                     simTime().dbl(), ifIndex);
            }
        }

        // Gửi LSAck (Section 13 (5e) / (7b))
        if (!lsuRes.ackHeaders.empty()) {
            linkStateAcknowledgementData::sendLSAck(ifIndex, lsuRes.ackHeaders,
                                                     routerId, iface->areaID, this);
        }

        // LoadingDone? (Section 10.3)
        if (nbr->state == NBR_LOADING && lsuRes.loadingDone) {
            nbr->state = NBR_FULL;
            if (nbr->rxmtTimer) {
                cancelEvent(nbr->rxmtTimer);
                delete nbr->rxmtTimer; nbr->rxmtTimer = nullptr;
            }
            state->logTransition("1b2", "LoadingDone→Full", simTime().dbl(), ifIndex);

            // 1c: Neighbor đầu tiên Full → originate Router-LSA (Section 12.4 Event 4)
            state->originateRouterLSA();
            // Flood LSA mới ra tất cả neighbor
            for (auto& lsa : state->area.routerLSAs) {
                if (lsa.header.advertisingRouter == routerId) {
                    linkStateUpdateData::floodLSA(lsa, -1, *state, routerId, this);
                    break;
                }
            }
        }
        // Loading còn item → gửi LSR tiếp (Section 10.9)
        else if (nbr->state == NBR_LOADING && !nbr->linkStateRequestList.empty()) {
            linkStateRequestData::sendLSR(ifIndex, *state, routerId, this);
            // Reset rxmtTimer (cancel cũ + schedule mới)
            if (nbr->rxmtTimer) {
                cancelEvent(nbr->rxmtTimer);
                delete nbr->rxmtTimer; nbr->rxmtTimer = nullptr;
            }
            nbr->rxmtTimer = new cMessage("rxmtTimer");
            scheduleAt(simTime() + iface->rxmtInterval, nbr->rxmtTimer);
        }

        // 1c: Flood LSA mới ra neighbor khác (Section 13.3 step (5b))
        if (!lsuRes.newLsas.empty()) {
            for (const auto& lsaHdr : lsuRes.newLsas) {
                // Tra LSDB để lấy LSA đầy đủ
                for (auto& lsa : state->area.routerLSAs) {
                    if (lsa.header.type == lsaHdr.type
                        && lsa.header.linkStateId == lsaHdr.linkStateId
                        && lsa.header.advertisingRouter == lsaHdr.advertisingRouter)
                    {
                        linkStateUpdateData::floodLSA(lsa, ifIndex, *state, routerId, this);
                        break;
                    }
                }
            }
        }

        // Schedule SPF nếu có LSA mới (Section 13.2)
        if (lsuRes.scheduleSPF) {
            if (!spfTimer) {
                spfTimer = new cMessage("spfTimer");
                scheduleAt(simTime() + iface->infTransDelay, spfTimer);
                state->logTransition("1c", "ScheduleSPF", simTime().dbl(), ifIndex);
            }
        }
    }

    // ============================================================
    // 1c.B: Link State Acknowledgment (type=5)
    // RFC 2328 Section 13.7 (Receiving link state acknowledgments)
    // ============================================================
    else if (pktType == 5) {
        NeighborData* nbr = iface->neighbor;

        // Pre-check: neighbor >= Exchange? (Section 13.7)
        if (nbr->state < NBR_EXCHANGE) {
            delete msg; return;
        }

        // Parse LSAck body: list of LSA headers (Section A.3.6)
        int remain = (int)data.size();
        if (remain > 0 && remain % 20 == 0) {
            int off = 0;
            int nAcks = remain / 20;
            bool listChanged = false;

            for (int i = 0; i < nAcks; i++) {
                LSAHeader ackHdr;
                ackHdr.age              = (uint16_t)((data[off] << 8) | data[off+1]); off += 2;
                ackHdr.options          = data[off++];
                ackHdr.type             = data[off++];
                ackHdr.linkStateId      = (uint32_t)(data[off] << 24) | (data[off+1] << 16)
                                        | (data[off+2] << 8) | data[off+3]; off += 4;
                ackHdr.advertisingRouter = (uint32_t)(data[off] << 24) | (data[off+1] << 16)
                                        | (data[off+2] << 8) | data[off+3]; off += 4;
                ackHdr.sequenceNumber   = (int32_t)((uint32_t)(data[off] << 24) | (data[off+1] << 16)
                                                  | (data[off+2] << 8) | data[off+3]); off += 4;
                ackHdr.checksum         = (uint16_t)((data[off] << 8) | data[off+1]); off += 2;
                ackHdr.length           = (uint16_t)((data[off] << 8) | data[off+1]); off += 2;

                // Tìm trong linkStateRetransmissionList (Section 13.7)
                for (auto it = nbr->linkStateRetransmissionList.begin();
                     it != nbr->linkStateRetransmissionList.end(); ++it)
                {
                    if (it->LSType == ackHdr.type
                        && it->linkStateId == ackHdr.linkStateId
                        && it->advertisingRouter == ackHdr.advertisingRouter)
                    {
                        // Cùng instance? (so sánh sequenceNumber)
                        // Lưu ý: linkStateRetransmissionList là LSARequest (ko có seq)
                        // Nên chấp nhận mọi ACK cho LSA này (Section 13.7 đơn giản hóa)
                        nbr->linkStateRetransmissionList.erase(it);
                        listChanged = true;
                        break;
                    }
                }
            }

            // Nếu list rỗng → cancel rxmtTimer (Section 13.6)
            if (listChanged && nbr->linkStateRetransmissionList.empty() && nbr->rxmtTimer) {
                cancelEvent(nbr->rxmtTimer);
                delete nbr->rxmtTimer;
                nbr->rxmtTimer = nullptr;
            }
        }
    }

    delete msg;
    
}

void routerOspf::finish()
{
    cancelAndDelete(helloTimer);
    if (spfTimer) {
        cancelAndDelete(spfTimer);
        spfTimer = nullptr;
    }
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
