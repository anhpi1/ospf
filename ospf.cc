#include "ospf.h"
#include <queue>
#include <set>
#include <algorithm>
#include <climits>
#include <cstring>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

Define_Module(routerOspf);



void routerOspf::initialize()
{
    // Router ID từ tên module (r1 → 1, r10 → 10)
    {
        std::string name = getName();
        routerId = std::stoi(name.substr(1));
    }
    numRouters = par("numRouters");
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

    // Initialize link flap scheduler
    flapTotal = 0;
    flapRemaining = 0;
    flapTimer = nullptr;
    blockedInterfaces.clear();

    // Initialize client send scheduler
    clientSendEvents.clear();
    clientSendRemaining = 0;
    clientSendTimer = nullptr;

    parseLinkFlaps("link_flaps.txt");
}


void routerOspf::handleMessage(cMessage *msg)
{
    // Debug: dump state mỗi lần handleMessage được gọi
    dumpStateToJson("log");
    dumpMessageBinary(msg);

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

        spfTimer = nullptr;
        delete msg;
        } else if (msg == flapTimer) {
            // Xử lý link flap event
            int evIdx = flapTotal - flapRemaining;
            LinkFlapEvent& ev = flapEvents[evIdx];
            // Lookup interface từ targetRouterId
            int ifIdx = findInterfaceByNeighbor(ev.targetRouterId);
            if (ifIdx >= 0) {
                if (ev.isDown) {
                    blockedInterfaces.insert(ifIdx);
                    state->interfaces[ifIdx].linkDisabled = true;
                } else {
                    blockedInterfaces.erase(ifIdx);
                    state->interfaces[ifIdx].linkDisabled = false;
                }
            } else {
            }

            flapRemaining--;

            // Schedule event kế tiếp
            if (flapRemaining > 0) {
                int nextIdx = flapTotal - flapRemaining;
                scheduleAt(flapEvents[nextIdx].time, msg);
            } else {
                flapTimer = nullptr;
                delete msg;
            }
        } else if (msg == clientSendTimer) {
            // Xử lý client send event: tất cả router gửi data đến nhau
            for (uint32_t destId = 1; destId <= (uint32_t)numRouters; destId++) {
                if (destId == routerId) continue;
                Mess* dataMsg = new Mess("dataTest");
                dataMsg->setPayloadArraySize(8);
                // Bytes 0-3: destination Router ID
                dataMsg->setPayload(0, (destId >> 24) & 0xFF);
                dataMsg->setPayload(1, (destId >> 16) & 0xFF);
                dataMsg->setPayload(2, (destId >> 8) & 0xFF);
                dataMsg->setPayload(3, destId & 0xFF);
                // Bytes 4-7: source Router ID (self)
                dataMsg->setPayload(4, (routerId >> 24) & 0xFF);
                dataMsg->setPayload(5, (routerId >> 16) & 0xFF);
                dataMsg->setPayload(6, (routerId >> 8) & 0xFF);
                dataMsg->setPayload(7, routerId & 0xFF);
                forwardData(dataMsg, -1);  // -1 = originated locally
            }
            clientSendRemaining--;
            // Schedule event kế tiếp
            int evIdx = (int)clientSendEvents.size() - clientSendRemaining - 1;
            if (clientSendRemaining > 0) {
                scheduleAt(clientSendEvents[evIdx + 1].time, msg);
            } else {
                clientSendTimer = nullptr;
                delete msg;
            }
        } else {
            // Tìm timer nào đã cháy: inactivityTimer hoặc rxmtTimer
            bool found = false;
            for (int i = 0; i < (int)state->interfaces.size(); i++) {
                InterfaceData* iface = &state->interfaces[i];
                NeighborData* nbr = iface->neighbor;
                if (nbr && nbr->inactivityTimer == msg) {
                    nbr->inactivityTimer = nullptr;
                    nbr->state = NBR_DOWN;
                    // Khong clear IDNeighbor: giu lai de findInterfaceByNeighbor()
                    // van tim duoc interface khi flap UP sau nay
                    nbr->databaseSummaryList.clear();
                    nbr->linkStateRequestList.clear();
                    nbr->linkStateRetransmissionList.clear();
                    // InactivityTimer cháy: originate LSA mới + schedule SPF
                    state->originateRouterLSA();
                    for (auto& lsa : state->area.routerLSAs) {
                        if (lsa.header.advertisingRouter == routerId) {
                            linkStateUpdateData::floodLSA(lsa, -1, *state, routerId, this);
                            break;
                        }
                    }
                    if (!spfTimer) {
                        spfTimer = new cMessage("spfTimer");
                        scheduleAt(simTime() + iface->infTransDelay, spfTimer);
                    }
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

    // === LINK FLAP BLOCK CHECK ===
    if (!msg->isSelfMessage() && !blockedInterfaces.empty()
        && !msg->arrivedOn("clientGate$i")) {
        int ifIndex = msg->getArrivalGate()->getIndex();
        if (blockedInterfaces.count(ifIndex)) {
            delete msg;
            return;
        }
    }

    //code chính xử lý gói tin

    // Data packet từ Client module (clientGate)
    if (msg->arrivedOn("clientGate$i")) {
        Mess* dataMsg = dynamic_cast<Mess*>(msg);
        if (dataMsg) {
            forwardData(dataMsg, -2);  // -2 = from client
        } else
            delete msg;
        return;
    }

    int ifIndex = msg->getArrivalGate()->getIndex();
    InterfaceData* iface = &state->interfaces[ifIndex];

    headerOspf hdr;
    std::vector<uint8_t> data;
    uint8_t pktType = OspfMess::parse((Mess*)msg, iface, hdr, data);

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
            forwardData(dataMsg, ifIndex);
        }
        else
            delete msg;
        return; // forwardData handles send/delete; don't fall to delete msg
    }

    delete msg;

}

void routerOspf::finish()
{
    // Cleanup flapTimer
    if (flapTimer) {
        cancelAndDelete(flapTimer);
        flapTimer = nullptr;
    }

    cancelAndDelete(helloTimer);
    if (spfTimer) {
        cancelAndDelete(spfTimer);
        spfTimer = nullptr;
    }
    if (clientSendTimer) {
        cancelAndDelete(clientSendTimer);
        clientSendTimer = nullptr;
    }
    for (auto& iface : state->interfaces) {
        NeighborData* nbr = iface.neighbor;
        if (nbr) {
            if (nbr->inactivityTimer) cancelAndDelete(nbr->inactivityTimer);
            if (nbr->rxmtTimer) cancelAndDelete(nbr->rxmtTimer);
        }
    }
    delete state;
}


// === Link Flap Scheduler ===

void routerOspf::parseLinkFlaps(const char* filename)
{
    std::ifstream f(filename);
    if (!f.is_open()) return;  // file không tồn tại -> bỏ qua

    std::string myName = getName();  // "r1", "r2", ...
    int myId = std::stoi(myName.substr(1));

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);

        if (tokens.size() == 4) {
            // Link flap: rA rB down/up T
            std::string rA = tokens[0], rB = tokens[1], action = tokens[2];
            double t = std::stod(tokens[3]);

            // Lọc: có liên quan đến router này không?
            int targetId = -1;
            if (rA == myName)       targetId = std::stoi(rB.substr(1));
            else if (rB == myName)  targetId = std::stoi(rA.substr(1));
            else continue;  // không liên quan

            bool isDown;
            if (action == "down")       isDown = true;
            else if (action == "up")    isDown = false;
            else continue;  // action không hợp lệ

            LinkFlapEvent ev;
            ev.time = simTime() + t;
            ev.targetRouterId = targetId;
            ev.isDown = isDown;
            flapEvents.push_back(ev);
        } else if (tokens.size() == 3 && tokens[0] == "client" && tokens[1] == "send") {
            // Client send: client send T
            double t = std::stod(tokens[2]);
            ClientSendEvent ev;
            ev.time = simTime() + t;
            clientSendEvents.push_back(ev);
        }
        // Dòng không hợp lệ: bỏ qua
    }

    // Sort theo time
    std::sort(flapEvents.begin(), flapEvents.end(),
        [](const LinkFlapEvent& a, const LinkFlapEvent& b) {
            return a.time < b.time;
        });

    flapTotal = (int)flapEvents.size();
    flapRemaining = flapTotal;

    // Schedule event đầu tiên
    if (flapRemaining > 0) {
        flapTimer = new cMessage("flapTimer");
        scheduleAt(flapEvents[0].time, flapTimer);
    }

    // === Client Send Events ===
    if (!clientSendEvents.empty()) {
        std::sort(clientSendEvents.begin(), clientSendEvents.end(),
            [](const ClientSendEvent& a, const ClientSendEvent& b) {
                return a.time < b.time;
            });
        clientSendRemaining = (int)clientSendEvents.size();
        clientSendTimer = new cMessage("clientSendTimer");
        scheduleAt(clientSendEvents[0].time, clientSendTimer);
    }
}

int routerOspf::findInterfaceByNeighbor(uint32_t targetId)
{
    for (int i = 0; i < (int)state->interfaces.size(); i++) {
        NeighborData* nbr = state->interfaces[i].neighbor;
        if (nbr && nbr->IDNeighbor == targetId)
            return i;
    }
    return -1;  // không tìm thấy -> link không tồn tại
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

    // A: Arrived at destination?
    if (dest == routerId) {
        uint32_t cost = 0;
        for (auto& e : state->RoutingTable)
            if (e.destinationType == 'R' && e.destinationId == srcId)
                { cost = e.cost; break; }
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
        delete msg;
        return;
    }

    // D: Forward to next hop
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

//////////////////////////////////////////////////////////////////
// Debug: dump toàn bộ trạng thái router ra file JSON
//////////////////////////////////////////////////////////////////

// ── enum → tên ──────────────────────────────────────────────────
static const char* nbrStateName(unsigned int s) {
    switch (s) {
        case NBR_DOWN:     return "NBR_DOWN";
        case NBR_ATTEMPT:  return "NBR_ATTEMPT";
        case NBR_INIT:     return "NBR_INIT";
        case NBR_TWOWAY:   return "NBR_TWOWAY";
        case NBR_EXSTART:  return "NBR_EXSTART";
        case NBR_EXCHANGE: return "NBR_EXCHANGE";
        case NBR_LOADING:  return "NBR_LOADING";
        case NBR_FULL:     return "NBR_FULL";
        default:           return "UNKNOWN";
    }
}

static const char* ifStateName(unsigned int s) {
    switch (s) {
        case IF_DOWN:         return "IF_DOWN";
        case IF_LOOPBACK:     return "IF_LOOPBACK";
        case IF_WAITING:      return "IF_WAITING";
        case IF_POINTTOPOINT: return "IF_POINTTOPOINT";
        case IF_DROTHER:      return "IF_DROTHER";
        case IF_BACKUP:       return "IF_BACKUP";
        case IF_DR:           return "IF_DR";
        default:              return "UNKNOWN";
    }
}

static const char* pathTypeName(unsigned int p) {
    switch (p) {
        case PATH_INTRA_AREA: return "PATH_INTRA_AREA";
        case PATH_INTER_AREA: return "PATH_INTER_AREA";
        case PATH_TYPE1_EXT:  return "PATH_TYPE1_EXT";
        case PATH_TYPE2_EXT:  return "PATH_TYPE2_EXT";
        default:              return "UNKNOWN";
    }
}

static const char* linkTypeName(unsigned int t) {
    switch (t) {
        case LINK_P2P:     return "LINK_P2P";
        case LINK_TRANSIT: return "LINK_TRANSIT";
        case LINK_STUB:    return "LINK_STUB";
        case LINK_VIRTUAL: return "LINK_VIRTUAL";
        default:           return "UNKNOWN";
    }
}

// ── IP helper ────────────────────────────────────────────────────
static std::string ipToStr(uint32_t ip) {
    std::ostringstream oss;
    oss << ((ip >> 24) & 0xFF) << "."
        << ((ip >> 16) & 0xFF) << "."
        << ((ip >> 8)  & 0xFF) << "."
        << (ip & 0xFF);
    return oss.str();
}

// ── mkdir -p helper ──────────────────────────────────────────────
static void mkpath(const char* path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

// ── serialize helpers (file-scope, forward-declared where needed) ──

static void writeJson(std::ofstream& f, const LSAHeader& h, const std::string& indent) {
    f << indent << "{\n";
    f << indent << "  \"age\": " << h.age << ",\n";
    f << indent << "  \"options\": " << (int)h.options << ",\n";
    f << indent << "  \"type\": " << (int)h.type << ",\n";
    f << indent << "  \"linkStateId\": \"" << ipToStr(h.linkStateId) << "\",\n";
    f << indent << "  \"advertisingRouter\": \"" << ipToStr(h.advertisingRouter) << "\",\n";
    f << indent << "  \"sequenceNumber\": " << h.sequenceNumber << ",\n";
    f << indent << "  \"checksum\": " << h.checksum << ",\n";
    f << indent << "  \"length\": " << h.length << "\n";
    f << indent << "}";
}

static void writeJson(std::ofstream& f, const TOSData& t, const std::string& indent) {
    f << indent << "{\n";
    f << indent << "  \"TOSid\": " << (int)t.TOSid << ",\n";
    f << indent << "  \"zero\": " << (int)t.zero << ",\n";
    f << indent << "  \"metric\": " << t.metric << "\n";
    f << indent << "}";
}

static void writeJson(std::ofstream& f, const LSALink& l, const std::string& indent) {
    f << indent << "{\n";
    f << indent << "  \"linkID\": \"" << ipToStr(l.linkID) << "\",\n";
    f << indent << "  \"linkData\": \"" << ipToStr(l.linkData) << "\",\n";
    f << indent << "  \"type\": \"" << linkTypeName(l.type) << "\",\n";
    f << indent << "  \"numTOS\": " << (int)l.numTOS << ",\n";
    f << indent << "  \"metric\": " << l.metric << ",\n";
    f << indent << "  \"TOSData\": [\n";
    for (size_t i = 0; i < l.Data.size(); i++) {
        writeJson(f, l.Data[i], indent + "    ");
        if (i + 1 < l.Data.size()) f << ",";
        f << "\n";
    }
    f << indent << "  ]\n";
    f << indent << "}";
}

static void writeJson(std::ofstream& f, const LSA& lsa, const std::string& indent) {
    f << indent << "{\n";
    f << indent << "  \"header\": ";
    writeJson(f, lsa.header, indent + "  ");
    f << ",\n";
    f << indent << "  \"flags\": " << (int)lsa.flags << ",\n";
    f << indent << "  \"zero\": " << (int)lsa.zero << ",\n";
    f << indent << "  \"numLinks\": " << lsa.numLinks << ",\n";
    f << indent << "  \"links\": [\n";
    for (size_t i = 0; i < lsa.links.size(); i++) {
        writeJson(f, lsa.links[i], indent + "    ");
        if (i + 1 < lsa.links.size()) f << ",";
        f << "\n";
    }
    f << indent << "  ]\n";
    f << indent << "}";
}

static void writeJson(std::ofstream& f, const LSARequest& r, const std::string& indent) {
    f << indent << "{\n";
    f << indent << "  \"LSType\": " << r.LSType << ",\n";
    f << indent << "  \"linkStateId\": \"" << ipToStr(r.linkStateId) << "\",\n";
    f << indent << "  \"advertisingRouter\": \"" << ipToStr(r.advertisingRouter) << "\"\n";
    f << indent << "}";
}

static void writeJson(std::ofstream& f, const NeighborData& nbr, const std::string& indent) {
    f << indent << "{\n";
    f << indent << "  \"state\": \"" << nbrStateName(nbr.state) << "\",\n";
    f << indent << "  \"isMaster\": " << nbr.isMaster << ",\n";
    f << indent << "  \"ddSequenceNumber\": " << nbr.ddSequenceNumber << ",\n";
    f << indent << "  \"lastDdOptions\": " << (int)nbr.lastDdOptions << ",\n";
    f << indent << "  \"lastDdIMs\": " << (int)nbr.lastDdIMs << ",\n";
    f << indent << "  \"IDNeighbor\": \"" << ipToStr(nbr.IDNeighbor) << "\",\n";
    f << indent << "  \"priorityNeighbor\": " << (int)nbr.priorityNeighbor << ",\n";
    f << indent << "  \"IPNeighbor\": \"" << ipToStr(nbr.IPNeighbor) << "\",\n";
    f << indent << "  \"optionsNeighbor\": " << (int)nbr.optionsNeighbor << ",\n";

    f << indent << "  \"databaseSummaryList\": [\n";
    for (size_t i = 0; i < nbr.databaseSummaryList.size(); i++) {
        writeJson(f, nbr.databaseSummaryList[i], indent + "    ");
        if (i + 1 < nbr.databaseSummaryList.size()) f << ",";
        f << "\n";
    }
    f << indent << "  ],\n";

    f << indent << "  \"linkStateRequestList\": [\n";
    for (size_t i = 0; i < nbr.linkStateRequestList.size(); i++) {
        writeJson(f, nbr.linkStateRequestList[i], indent + "    ");
        if (i + 1 < nbr.linkStateRequestList.size()) f << ",";
        f << "\n";
    }
    f << indent << "  ],\n";

    f << indent << "  \"linkStateRetransmissionList\": [\n";
    for (size_t i = 0; i < nbr.linkStateRetransmissionList.size(); i++) {
        writeJson(f, nbr.linkStateRetransmissionList[i], indent + "    ");
        if (i + 1 < nbr.linkStateRetransmissionList.size()) f << ",";
        f << "\n";
    }
    f << indent << "  ],\n";

    // timers
    f << indent << "  \"rxmtTimer\": " << (nbr.rxmtTimer ? "true" : "null") << ",\n";
    f << indent << "  \"inactivityTimer\": " << (nbr.inactivityTimer ? "true" : "null") << "\n";
    f << indent << "}";
}

// Forward declaration for SPF recursion
static void writeSpfVertex(std::ofstream& f, const SpfVertex& v, const std::string& indent,
                           std::set<uint32_t>& visited);

static void writeJson(std::ofstream& f, const InterfaceData& iface, int idx, const std::string& indent) {
    f << indent << "{\n";
    f << indent << "  \"index\": " << idx << ",\n";
    f << indent << "  \"type\": " << iface.type << ",\n";
    f << indent << "  \"state\": \"" << ifStateName(iface.state) << "\",\n";
    f << indent << "  \"linkDisabled\": " << (iface.linkDisabled ? "true" : "false") << ",\n";
    f << indent << "  \"ipAddress\": \"" << ipToStr(iface.ipAddress) << "\",\n";
    f << indent << "  \"mask\": \"" << ipToStr(iface.mask) << "\",\n";
    f << indent << "  \"areaID\": \"" << ipToStr(iface.areaID) << "\",\n";
    f << indent << "  \"helloInterval\": " << iface.helloInterval << ",\n";
    f << indent << "  \"routerDeadInterval\": " << iface.routerDeadInterval << ",\n";
    f << indent << "  \"infTransDelay\": " << iface.infTransDelay << ",\n";
    f << indent << "  \"routerPriority\": " << (int)iface.routerPriority << ",\n";
    f << indent << "  \"cost\": " << iface.cost << ",\n";
    f << indent << "  \"rxmtInterval\": " << iface.rxmtInterval << ",\n";
    f << indent << "  \"neighbor\": ";
    if (iface.neighbor) {
        writeJson(f, *iface.neighbor, indent + "  ");
    } else {
        f << "null";
    }
    f << "\n" << indent << "}";
}

static void writeSpfVertex(std::ofstream& f, const SpfVertex& v, const std::string& indent,
                           std::set<uint32_t>& visited) {
    f << indent << "{\n";
    f << indent << "  \"vertexId\": \"" << ipToStr(v.vertexId) << "\",\n";
    f << indent << "  \"distance\": " << v.distance << ",\n";
    f << indent << "  \"nextHop\": " << v.nextHop << ",\n";
    f << indent << "  \"parent\": ";
    if (v.parent)
        f << "\"" << ipToStr(v.parent->vertexId) << "\"";
    else
        f << "null";
    f << ",\n";

    visited.insert(v.vertexId);
    f << indent << "  \"neighbors\": [\n";
    bool first = true;
    for (size_t i = 0; i < v.neighbors.size(); i++) {
        if (v.neighbors[i]) {
            if (!first) f << ",\n";
            first = false;
            if (visited.count(v.neighbors[i]->vertexId)) {
                f << indent << "    {\"ref\": \"" << ipToStr(v.neighbors[i]->vertexId) << "\"}";
            } else {
                writeSpfVertex(f, *v.neighbors[i], indent + "    ", visited);
            }
        }
    }
    if (!first) f << "\n";
    f << indent << "  ]\n";
    f << indent << "}";
}

static void writeJson(std::ofstream& f, const RoutingTableEntry& e, const std::string& indent) {
    f << indent << "{\n";
    f << indent << "  \"destinationType\": \"" << (char)e.destinationType << "\",\n";
    f << indent << "  \"destinationId\": \"" << ipToStr(e.destinationId) << "\",\n";
    f << indent << "  \"addressMask\": \"" << ipToStr(e.addressMask) << "\",\n";
    f << indent << "  \"area\": \"" << ipToStr(e.area) << "\",\n";
    f << indent << "  \"pathType\": \"" << pathTypeName(e.pathType) << "\",\n";
    f << indent << "  \"cost\": " << e.cost << ",\n";
    f << indent << "  \"nextHop\": \"" << ipToStr(e.nextHop) << "\"\n";
    f << indent << "}";
}

// ── main dump function ──────────────────────────────────────────
void routerOspf::dumpStateToJson(const char* dir) {
    // Tạo thư mục log/{router}/
    std::string rname = getName();
    char subdir[256];
    snprintf(subdir, sizeof(subdir), "%s/%s", dir, rname.c_str());
    mkpath(subdir);

    // Tìm số thứ tự file tiếp theo
    static int counter = 0;
    counter++;

    char path[256];
    snprintf(path, sizeof(path), "%s/%d.json", subdir, counter);

    std::ofstream f(path);
    if (!f.is_open()) {
        EV << "dumpStateToJson: cannot open " << path << "\n";
        return;
    }

    const std::string I  = "  ";
    const std::string I2 = "    ";
    const std::string I3 = "      ";
    const std::string I4 = "        ";

    f << "{\n";

    // ── routerOspf top-level fields ──
    f << I << "\"routerId\": \"" << ipToStr(routerId) << "\",\n";
    f << I << "\"numRouters\": " << numRouters << ",\n";
    f << I << "\"simTime\": " << simTime().dbl() << ",\n";

    // timers
    f << I << "\"timers\": {\n";
    f << I2 << "\"helloTimer\": " << (helloTimer ? std::to_string(helloTimer->getArrivalTime().dbl()) : "null") << ",\n";
    f << I2 << "\"spfTimer\": " << (spfTimer ? std::to_string(spfTimer->getArrivalTime().dbl()) : "null") << ",\n";
    f << I2 << "\"flapTimer\": " << (flapTimer ? std::to_string(flapTimer->getArrivalTime().dbl()) : "null") << "\n";
    f << I << "},\n";

    // link flap
    f << I << "\"linkFlap\": {\n";
    f << I2 << "\"total\": " << flapTotal << ",\n";
    f << I2 << "\"remaining\": " << flapRemaining << ",\n";
    f << I2 << "\"blockedInterfaces\": [";
    bool first = true;
    for (int ifIdx : blockedInterfaces) {
        if (!first) f << ", ";
        first = false;
        f << ifIdx;
    }
    f << "],\n";
    f << I2 << "\"events\": [\n";
    for (size_t i = 0; i < flapEvents.size(); i++) {
        f << I3 << "{\"time\": " << flapEvents[i].time.dbl()
          << ", \"targetRouterId\": " << flapEvents[i].targetRouterId
          << ", \"isDown\": " << (flapEvents[i].isDown ? "true" : "false") << "}";
        if (i + 1 < flapEvents.size()) f << ",";
        f << "\n";
    }
    f << I2 << "]\n";
    f << I << "},\n";

    // ── OspfRouterState ──
    f << I << "\"state\": {\n";
    f << I2 << "\"routerID\": \"" << ipToStr(state->routerID) << "\",\n";

    // interfaces
    f << I2 << "\"interfaces\": [\n";
    for (size_t i = 0; i < state->interfaces.size(); i++) {
        writeJson(f, state->interfaces[i], (int)i, I3);
        if (i + 1 < state->interfaces.size()) f << ",";
        f << "\n";
    }
    f << I2 << "],\n";

    // area
    const AreaData& a = state->area;
    f << I2 << "\"area\": {\n";
    f << I3 << "\"areaID\": \"" << ipToStr(a.areaID) << "\",\n";
    f << I3 << "\"transitCapability\": " << (a.transitCapability ? "true" : "false") << ",\n";
    f << I3 << "\"externalRoutingCapability\": " << (a.externalRoutingCapability ? "true" : "false") << ",\n";
    f << I3 << "\"stubDefaultCost\": " << a.stubDefaultCost << ",\n";

    f << I3 << "\"interfaceIndices\": [";
    first = true;
    for (auto idx : a.interfaceIndices) {
        if (!first) f << ", ";
        first = false;
        f << idx;
    }
    f << "],\n";

    // LSDB
    f << I3 << "\"routerLSAs\": [\n";
    for (size_t i = 0; i < a.routerLSAs.size(); i++) {
        writeJson(f, a.routerLSAs[i], I4);
        if (i + 1 < a.routerLSAs.size()) f << ",";
        f << "\n";
    }
    f << I3 << "],\n";

    // SPF tree
    f << I3 << "\"spfVertices\": [\n";
    std::set<uint32_t> visited;
    int verticesWritten = 0;
    for (size_t i = 0; i < a.spfVertices.size(); i++) {
        if (visited.count(a.spfVertices[i].vertexId) == 0) {
            if (verticesWritten > 0) f << ",\n";
            writeSpfVertex(f, a.spfVertices[i], I4, visited);
            verticesWritten++;
        }
    }
    if (verticesWritten > 0) f << "\n";
    f << I3 << "]\n";
    f << I2 << "},\n";

    // routing table
    f << I2 << "\"routingTable\": [\n";
    for (size_t i = 0; i < state->RoutingTable.size(); i++) {
        writeJson(f, state->RoutingTable[i], I3);
        if (i + 1 < state->RoutingTable.size()) f << ",";
        f << "\n";
    }
    f << I2 << "],\n";

    // external routes
    f << I2 << "\"externalRoutes\": {";
    first = true;
    for (const auto& kv : state->externalRoutes) {
        if (!first) f << ",";
        first = false;
        f << "\n" << I3 << "\"" << ipToStr(kv.first) << "\": " << kv.second;
    }
    if (!state->externalRoutes.empty()) f << "\n" << I2;
    f << "}\n";

    f << I << "}\n";  // close state

    f << "}\n";  // close root
    f.close();

    EV << "dumpStateToJson: written " << path << " (" << counter << ")\n";
}

// ── Binary packet dump ─────────────────────────────────────────
void routerOspf::dumpMessageBinary(cMessage *msg)
{
    // Chỉ dump Mess* (có payload), bỏ qua self-message (timer)
    Mess *m = dynamic_cast<Mess*>(msg);
    if (!m) return;

    std::string rname = getName();

    // Xác định thư mục con dựa trên gate
    char subdir[256];
    if (msg->arrivedOn("clientGate$i")) {
        snprintf(subdir, sizeof(subdir), "bin/%s/client", rname.c_str());
    } else {
        int ifIndex = msg->getArrivalGate()->getIndex();
        const char* nbrSt = "NO_NBR";  // fallback an toàn
        if (ifIndex < (int)state->interfaces.size()
            && state->interfaces[ifIndex].neighbor) {
            nbrSt = nbrStateName(state->interfaces[ifIndex].neighbor->state);
        }
        snprintf(subdir, sizeof(subdir), "bin/%s/if%d/%s",
                 rname.c_str(), ifIndex, nbrSt);
    }
    mkpath(subdir);

    static int counter = 0;
    counter++;

    // Định dạng simTime: thay dấu chấm bằng gạch dưới
    char timeStr[64];
    snprintf(timeStr, sizeof(timeStr), "%.6f", simTime().dbl());
    for (char *p = timeStr; *p; p++)
        if (*p == '.') *p = '_';

    char path[512];
    snprintf(path, sizeof(path), "%s/%06d_%s.bin", subdir, counter, timeStr);

    FILE *f = fopen(path, "wb");
    if (!f) {
        EV << "dumpMessageBinary: cannot open " << path << "\n";
        return;
    }

    // Ghi trực tiếp payload[] byte ra file
    size_t sz = m->getPayloadArraySize();
    for (size_t i = 0; i < sz; i++) {
        uint8_t b = m->getPayload(i);
        fwrite(&b, 1, 1, f);
    }
    fclose(f);

    EV << "dumpMessageBinary: written " << path
       << " (" << sz << " bytes)\n";
}