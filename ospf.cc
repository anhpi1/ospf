#include "ospf.h"
#include <queue>
#include <set>
#include <algorithm>
#include <climits>
#include <cstring>

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
    msgSeq = 0;                             // message dump counter

    // Ghi state dump đầu tiên — trạng thái ngay sau Phase 0 (0.G)
    state->printState("init");
}


void routerOspf::handleMessage(cMessage *msg)
{
    // Biến tạm để dump message ở cuối hàm (sau khi xử lý xong)
    std::vector<uint8_t> dumpBytes;

    // xử lý timer
    if (msg->isSelfMessage()) {
        if (msg == helloTimer) {
            // Gửi Hello trên tất cả interface
            for (int i = 0; i < (int)state->interfaces.size(); i++) {
                helloData::sendHello(i, *state, routerId, this);
            }
            scheduleAt(simTime() + state->interfaces[0].helloInterval, msg);
        } else if (msg == spfTimer) {
            // 2a: spfTimer cháy → tính SPF + build routing table
            // Backup routing table cũ để phát hiện thay đổi (Section 16 step 1)
            std::vector<RoutingTableEntry> oldRT = state->RoutingTable;

            // Clear + tính lại
            state->RoutingTable.clear();
            calculateSpf();

            // Phát hiện thay đổi (Section 12.4 Event 4)
            // So sánh thủ công vì RoutingTableEntry chưa có operator==
            bool rtChanged = (oldRT.size() != state->RoutingTable.size());
            if (!rtChanged) {
                for (int i = 0; i < (int)oldRT.size(); i++) {
                    if (oldRT[i].destinationId != state->RoutingTable[i].destinationId
                        || oldRT[i].cost != state->RoutingTable[i].cost
                        || oldRT[i].nextHop != state->RoutingTable[i].nextHop)
                    { rtChanged = true; break; }
                }
            }
            if (rtChanged) {
                state->originateRouterLSA();
                // Flood LSA mới ra tất cả interface
                for (auto& lsa : state->area.routerLSAs) {
                    if (lsa.header.advertisingRouter == routerId) {
                        linkStateUpdateData::floodLSA(lsa, -1, *state, routerId, this);
                        break;
                    }
                }
            }

            state->logTransition("2a", "SPF completed", simTime().dbl(), -1);

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

    // Data packet từ Client module (clientGate)
    if (msg->arrivedOn("clientGate$i")) {
        Mess* dataMsg = dynamic_cast<Mess*>(msg);
        if (dataMsg) {
            int n = dataMsg->getPayloadArraySize();
            dumpBytes.resize(n);
            for (int i = 0; i < n; i++) dumpBytes[i] = dataMsg->getPayload(i);
            forwardData(dataMsg, -2);  // -2 = from client
            if (!dumpBytes.empty()) dumpMessage(dumpBytes.data(), dumpBytes.size());
        } else
            delete msg;
        return;
    }

    int ifIndex = msg->getArrivalGate()->getIndex();
    InterfaceData* iface = &state->interfaces[ifIndex];

    headerOspf hdr;
    std::vector<uint8_t> data;
    uint8_t pktType = OspfMess::parse((Mess*)msg, iface, hdr, data);

    // Dump raw message binary (OSPF header 24B + body)
    {
        uint16_t totalLen = 24 + (uint16_t)data.size();
        std::vector<uint8_t> raw(24 + data.size());
        raw[0] = hdr.version;
        raw[1] = pktType;
        raw[2] = (totalLen >> 8) & 0xFF;
        raw[3] = totalLen & 0xFF;
        raw[4] = (hdr.routerId >> 24) & 0xFF;
        raw[5] = (hdr.routerId >> 16) & 0xFF;
        raw[6] = (hdr.routerId >> 8) & 0xFF;
        raw[7] = hdr.routerId & 0xFF;
        raw[8] = (hdr.areaId >> 24) & 0xFF;
        raw[9] = (hdr.areaId >> 16) & 0xFF;
        raw[10] = (hdr.areaId >> 8) & 0xFF;
        raw[11] = hdr.areaId & 0xFF;
        raw[12] = (hdr.checksum >> 8) & 0xFF;
        raw[13] = hdr.checksum & 0xFF;
        raw[14] = (hdr.authType >> 8) & 0xFF;
        raw[15] = hdr.authType & 0xFF;
        raw[16] = (hdr.authData1 >> 24) & 0xFF;
        raw[17] = (hdr.authData1 >> 16) & 0xFF;
        raw[18] = (hdr.authData1 >> 8) & 0xFF;
        raw[19] = hdr.authData1 & 0xFF;
        raw[20] = (hdr.authData2 >> 24) & 0xFF;
        raw[21] = (hdr.authData2 >> 16) & 0xFF;
        raw[22] = (hdr.authData2 >> 8) & 0xFF;
        raw[23] = hdr.authData2 & 0xFF;
        std::memcpy(raw.data() + 24, data.data(), data.size());
        dumpBytes = std::move(raw);
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
                state->logTransition("1c", "Originate+Flood (1st Full)",
                                     simTime().dbl(), ifIndex);
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
            state->logTransition("1c", "Originate+Flood (1st Full)",
                                 simTime().dbl(), ifIndex);
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
            state->logTransition("1c", "FloodFwd",
                                 simTime().dbl(), ifIndex);
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

    // 2b: Data packet (not OSPF protocol type 1-5)
    else if (pktType == 0) {
        Mess* dataMsg = dynamic_cast<Mess*>(msg);
        if (dataMsg) {
            int n = dataMsg->getPayloadArraySize();
            dumpBytes.resize(n);
            for (int i = 0; i < n; i++) dumpBytes[i] = dataMsg->getPayload(i);
            forwardData(dataMsg, ifIndex);
            if (!dumpBytes.empty()) dumpMessage(dumpBytes.data(), dumpBytes.size());
        }
        else
            delete msg;
        return; // forwardData handles send/delete; don't fall to delete msg
    }

    if (!dumpBytes.empty()) dumpMessage(dumpBytes.data(), dumpBytes.size());
    delete msg;
    
}

void routerOspf::dumpMessage(const uint8_t* bytes, size_t len)
{
    namespace fs = std::filesystem;

    msgSeq++;
    std::string dir = "mess/" + state->lastStateSubdir;
    fs::create_directories(dir);

    std::string path = dir + "/" + state->lastStateName
                     + "__a" + std::to_string(msgSeq) + ".bin";
    std::ofstream f(path, std::ios::binary);
    if (f.is_open()) {
        f.write((const char*)bytes, len);
        f.close();
    }
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
    state->printState("finish");
    delete state;
}



// MaximumAge (RFC 2328 Section 13.2): LSA hết hạn
static const uint32_t MAX_AGE = 3600;
// INFINITY dùng cho dist[] trong SPF
static const uint32_t SPF_INFINITY = 0xFFFFFFFF;


//
// calcNextHop: tìm gate index kết nối trực tiếp tới destId (Section 16.1.1)
// Chỉ gọi khi parent == self (direct neighbor), V != self dùng inherit.
//
unsigned int routerOspf::calcNextHop(uint32_t destId)
{
    for (int i = 0; i < (int)state->interfaces.size(); i++) {
        NeighborData* nbr = state->interfaces[i].neighbor;
        if (nbr && nbr->IDNeighbor == destId)
            return (unsigned int)i;
    }
    return UINT_MAX; // không tìm thấy (lỗi)
}


//
// forwardData: forward data packet using routing table lookup (Section 11.1)
// payload[0-3] = dest Router ID, payload[4-7] = source Router ID (network byte order)
// ifIndex = -1 if originated locally (self-message), >=0 if from gate
//
void routerOspf::forwardData(Mess* msg, int ifIndex)
{
    // Guard: must have 8-byte payload
    if (msg->getPayloadArraySize() < 8) { delete msg; return; }

    // Parse dest + source
    uint32_t dest = ((uint32_t)msg->getPayload(0) << 24)
                  | ((uint32_t)msg->getPayload(1) << 16)
                  | ((uint32_t)msg->getPayload(2) << 8)
                  |  msg->getPayload(3);
    uint32_t srcId = ((uint32_t)msg->getPayload(4) << 24)
                   | ((uint32_t)msg->getPayload(5) << 16)
                   | ((uint32_t)msg->getPayload(6) << 8)
                   |  msg->getPayload(7);

    // Log test initiation (from Client, was initForwardingTest)
    if (ifIndex == -2) {
        state->logTransition("2b", ("Test:" + std::to_string(srcId)
            + "->" + std::to_string(dest)).c_str(), simTime().dbl(), -1);
    }

    // A: Arrived at destination?
    if (dest == routerId) {
        uint32_t cost = 0;
        for (auto& e : state->RoutingTable)
            if (e.destinationType == 'R' && e.destinationId == srcId)
                { cost = e.cost; break; }
        state->logTransition("2b", ("Rcvd:" + std::to_string(srcId)
            + "->self cost=" + std::to_string(cost)).c_str(),
            simTime().dbl(), ifIndex);
        // Forward to local Client module
        send(msg, "clientGate$o");
        return;
    }

    // B: Routing table lookup (Section 11.1 — exact match on Router ID)
    uint32_t nextHopId = 0;
    bool found = false;
    for (auto& e : state->RoutingTable) {
        if (e.destinationType == 'R' && e.destinationId == dest) {
            nextHopId = e.nextHop;
            found = true;
            break;
        }
    }
    if (!found || nextHopId == 0) {
        state->logTransition("2b", ("NoRoute:" + std::to_string(dest)).c_str(),
                             simTime().dbl(), ifIndex);
        delete msg;
        return;
    }

    // C: Map nextHop Router ID -> gate index (reuse calcNextHop pattern)
    unsigned int gate = UINT_MAX;
    for (int i = 0; i < (int)state->interfaces.size(); i++) {
        NeighborData* nbr = state->interfaces[i].neighbor;
        if (nbr && nbr->IDNeighbor == nextHopId) {
            gate = (unsigned int)i;
            break;
        }
    }
    if (gate == UINT_MAX) {
        state->logTransition("2b", ("NHNotConn:" + std::to_string(nextHopId)).c_str(),
                             simTime().dbl(), ifIndex);
        delete msg;
        return;
    }

    // D: Forward to next hop
    state->logTransition("2b", ("Fwd:" + std::to_string(srcId) + "->"
        + std::to_string(dest) + " g[" + std::to_string(gate) + "]->"
        + std::to_string(nextHopId)).c_str(),
        simTime().dbl(), (int)gate);
    send(msg, "gate$o", gate);
    // After send: OMNeT++ owns msg — do NOT delete
}


//
// initForwardingTest: generate 9 data packets, one to each other router (2b)
// Called when testTimer fires (after SPF has completed)
//
// calculateSpf: tính shortest-path tree + build routing table
// Stage 1: Dijkstra trên transit vertices (Section 16.1)
// Stage 2: Thêm stub networks (Section 16.1)
//
void routerOspf::calculateSpf()
{
    // === STAGE 1: Dijkstra ===
    // Cấu trúc dữ liệu tạm (biến local)
    std::map<uint32_t, uint32_t> dist;               // distance từ root
    std::map<uint32_t, uint32_t> prev;               // predecessor
    std::map<uint32_t, unsigned int> nh;              // next-hop gate index
    std::set<uint32_t> inTree;                       // đã trong SPF tree
    // Priority queue: (distance, vertexId), min-heap
    using P = std::pair<uint32_t, uint32_t>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;

    // Input validation (Section 16.1)
    if (state->area.routerLSAs.empty()) return;    // LSDB rỗng → không tính được
    bool selfFound = false;
    for (const auto& lsa : state->area.routerLSAs)
        if (lsa.header.advertisingRouter == routerId) { selfFound = true; break; }
    if (!selfFound) return;                        // self không có LSA → không tính

    // Khởi tạo: tất cả dist = INFINITY
    for (const auto& lsa : state->area.routerLSAs)
        dist[lsa.header.advertisingRouter] = SPF_INFINITY;
    dist[routerId] = 0;
    pq.push({0, routerId});
    state->area.transitCapability = false; // Section 16.1 step (1)

    // Dijkstra loop (Section 16.1 steps 2-5)
    while (!pq.empty()) {
        auto [d, V] = pq.top(); pq.pop();
        if (inTree.count(V)) continue;
        inTree.insert(V);

        // Tìm Router-LSA của V
        LSA* lsaV = nullptr;
        for (auto& lsa : state->area.routerLSAs) {
            if (lsa.header.advertisingRouter == V) {
                lsaV = &lsa;
                break;
            }
        }
        if (!lsaV || lsaV->header.age >= MAX_AGE) continue;

        // Kiểm tra bit V (virtual link) — single-area bỏ qua
        if (lsaV->flags & LSA_FLAG_V)
            state->area.transitCapability = true;

        // Duyệt từng link (Section 16.1 step 2)
        for (const auto& link : lsaV->links) {
            if (link.type == LINK_STUB)
                continue; // stub → Stage 2

            if (link.type == LINK_P2P) {
                uint32_t W = link.linkID;

                // Bidirectional check + MaxAge check (Section 16.1 step 2b)
                LSA* lsaW = nullptr;
                for (auto& lsa : state->area.routerLSAs) {
                    if (lsa.header.advertisingRouter == W) {
                        lsaW = &lsa;
                        break;
                    }
                }
                if (!lsaW || lsaW->header.age >= MAX_AGE) continue;
                // Kiểm tra link ngược W→V
                bool backLink = false;
                for (const auto& wl : lsaW->links) {
                    if (wl.type == LINK_P2P && wl.linkID == V) {
                        backLink = true;
                        break;
                    }
                }
                if (!backLink) continue;

                // W đã trong tree? (Section 16.1 step 2c)
                if (inTree.count(W)) continue;

                // Tính D = dist[V] + metric (Section 16.1 step 2d)
                uint32_t D = d + link.metric;
                if (D < dist[V]) // overflow check
                    D = SPF_INFINITY;

                if (D > dist[W]) continue; // đường dài hơn → skip

                // D < dist[W] hoặc W chưa có: đường tốt hơn
                dist[W] = D;
                prev[W] = V;
                // Tính next-hop: V == self → direct, V != self → inherit
                if (V == routerId)
                    nh[W] = calcNextHop(W);
                else
                    nh[W] = nh[V];
                pq.push({D, W});
            }
            // LINK_TRANSIT: bỏ qua (P2P only)
        }
    }

    // Build SpfVertices tree từ kết quả Dijkstra
    state->area.spfVertices.clear();
    std::map<uint32_t, int> vertexIdx; // routerId → index trong spfVertices

    // Pre-allocate: đếm số đỉnh reachable
    for (auto& kv : dist) {
        if (kv.second < SPF_INFINITY) {
            SpfVertex sv;
            sv.vertexId = kv.first;
            sv.distance = (uint16_t)kv.second;
            sv.nextHop = nh.count(kv.first) ? nh[kv.first] : UINT_MAX;
            sv.parent = nullptr;
            state->area.spfVertices.push_back(sv);
            vertexIdx[kv.first] = (int)state->area.spfVertices.size() - 1;
        }
    }

    // Gắn parent/neighbors pointers (sau khi vector ổn định)
    for (auto& sv : state->area.spfVertices) {
        if (sv.vertexId == routerId) continue; // root
        auto it = prev.find(sv.vertexId);
        if (it == prev.end()) continue;
        uint32_t parentId = it->second; // predecessor duy nhất
        auto pi = vertexIdx.find(parentId);
        if (pi != vertexIdx.end()) {
            sv.parent = &state->area.spfVertices[pi->second];
            state->area.spfVertices[pi->second].neighbors.push_back(&sv);
        }
    }

    // === BUILD ROUTING TABLE (Section 16 step 4) ===
    state->RoutingTable.clear();

    // Router entries: mỗi reachable router ≠ self
    for (auto& kv : dist) {
        uint32_t dest = kv.first;
        uint32_t cost = kv.second;
        if (cost >= SPF_INFINITY || dest == routerId) continue;

        RoutingTableEntry e;
        e.destinationType = 'R';
        e.destinationId = dest;
        e.addressMask = 0;
        e.area = state->area.areaID;
        e.pathType = PATH_INTRA_AREA;
        e.cost = cost;
        // Chuyển gate index → Router ID
        auto ni = nh.find(dest);
        if (ni != nh.end() && ni->second < state->interfaces.size()
            && state->interfaces[ni->second].neighbor)
            e.nextHop = state->interfaces[ni->second].neighbor->IDNeighbor;
        else
            e.nextHop = 0;
        state->RoutingTable.push_back(e);
    }

    // === STAGE 2: Stub networks (Section 16.1) ===
    for (auto& kv : dist) {
        uint32_t V = kv.first;
        uint32_t dV = kv.second;
        if (dV >= SPF_INFINITY) continue;

        // Tìm Router-LSA của V
        LSA* lsaV = nullptr;
        for (auto& lsa : state->area.routerLSAs) {
            if (lsa.header.advertisingRouter == V) {
                lsaV = &lsa;
                break;
            }
        }
        if (!lsaV) continue;

        // Duyệt từng stub link
        for (const auto& link : lsaV->links) {
            if (link.type != LINK_STUB) continue;

            uint32_t stubId = link.linkID;
            uint32_t stubCost = dV + link.metric;
            unsigned int stubNh = (V == routerId) ? UINT_MAX : nh[V];

            // Kiểm tra xem đã có route cho stub này chưa
            bool found = false;
            for (auto& entry : state->RoutingTable) {
                if (entry.destinationType == 'N' && entry.destinationId == stubId) {
                    found = true;
                    if (stubCost < entry.cost) {
                        entry.cost = stubCost;
                        if (stubNh < state->interfaces.size()
                            && state->interfaces[stubNh].neighbor)
                            entry.nextHop = state->interfaces[stubNh].neighbor->IDNeighbor;
                    }
                    break;
                }
            }
            if (!found) {
                RoutingTableEntry e;
                e.destinationType = 'N';
                e.destinationId = stubId;
                e.addressMask = 0xFFFFFFFF;
                e.area = state->area.areaID;
                e.pathType = PATH_INTRA_AREA;
                e.cost = stubCost;
                if (stubNh < state->interfaces.size()
                    && state->interfaces[stubNh].neighbor)
                    e.nextHop = state->interfaces[stubNh].neighbor->IDNeighbor;
                else
                    e.nextHop = 0;
                state->RoutingTable.push_back(e);
            }
        }
    }

    // Sort routing table theo destinationId
    std::sort(state->RoutingTable.begin(), state->RoutingTable.end(),
        [](const RoutingTableEntry& a, const RoutingTableEntry& b) {
            return a.destinationId < b.destinationId;
        });
}