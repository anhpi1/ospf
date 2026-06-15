#include "ospf.h"

// ============================================================
// OspfRouterState (từ ospf_info_router.cc)
// ============================================================

OspfRouterState::OspfRouterState(uint32_t routerId, int numInterfaces)
{
    routerID = routerId;
    externalRoutes.clear();

    interfaces.resize(numInterfaces);
    for (int i = 0; i < numInterfaces; i++)
    {
        InterfaceData* iface = &interfaces[i];

        iface->type = 1;
        iface->state = IF_DOWN;
        iface->ipAddress = routerId;
        iface->mask = 0;
        iface->areaID = 0;
        iface->helloInterval = 10;
        iface->routerDeadInterval = 40;
        iface->infTransDelay = 1;
        iface->routerPriority = 0;
        iface->cost = 1;
        iface->rxmtInterval = 5;

        iface->neighbor = new NeighborData;
        iface->neighbor->IDNeighbor = 0;
        iface->neighbor->state = NBR_DOWN;
        iface->neighbor->inactivityTimer = nullptr;
        iface->neighbor->isMaster = false;
        iface->neighbor->ddSequenceNumber = 0;
        iface->neighbor->lastDdOptions = 0;
        iface->neighbor->lastDdIMs = 0;
        iface->neighbor->priorityNeighbor = 0;
        iface->neighbor->rxmtTimer = nullptr;
    }

    area.areaID = 0;
    area.transitCapability = false;
    area.externalRoutingCapability = true;
    area.stubDefaultCost = 1;
    area.interfaceIndices.clear();
    for (int i = 0; i < numInterfaces; i++)
        area.interfaceIndices.push_back(i);
    area.routerLSAs.clear();
    area.spfVertices.clear();

    RoutingTable.clear();
}

OspfRouterState::~OspfRouterState()
{
    for (auto& iface : interfaces)
        delete iface.neighbor;
}

static const char* nbrStateName(int s) {
    switch (s) {
        case NBR_DOWN:     return "Down";
        case NBR_ATTEMPT:  return "Attempt";
        case NBR_INIT:     return "Init";
        case NBR_TWOWAY:   return "2Way";
        case NBR_EXSTART:  return "ExStart";
        case NBR_EXCHANGE: return "Exchange";
        case NBR_LOADING:  return "Loading";
        case NBR_FULL:     return "Full";
        default:           return "?";
    }
}
static const char* ifStateName(int s) {
    switch (s) {
        case IF_DOWN:         return "Down";
        case IF_LOOPBACK:     return "Loopback";
        case IF_WAITING:      return "Waiting";
        case IF_POINTTOPOINT: return "PointToPoint";
        case IF_DROTHER:      return "DROther";
        case IF_BACKUP:       return "Backup";
        case IF_DR:           return "DR";
        default:              return "?";
    }
}

void OspfRouterState::printState()
{
    namespace fs = std::filesystem;
    fs::create_directories("state_dump");

    int maxNum = 0;
    for (const auto& entry : fs::directory_iterator("state_dump")) {
        std::string name = entry.path().stem().string();
        try { int n = std::stoi(name); if (n > maxNum) maxNum = n; }
        catch (...) {}
    }
    int fileNumber = maxNum + 1;

    std::string filename = "state_dump/" + std::to_string(fileNumber) + ".log";
    std::ofstream f(filename);
    if (!f.is_open()) return;

    f << "OSPF State dump #" << fileNumber << "\n";
    f << "Router ID: " << routerID << "\n\n";

    f << "--- Interfaces (" << interfaces.size() << ") ---\n";
    for (size_t i = 0; i < interfaces.size(); i++) {
        const InterfaceData& iface = interfaces[i];
        f << "  [" << i << "] type=" << iface.type
          << " state=" << ifStateName(iface.state)
          << " area=0x" << std::hex << iface.areaID << std::dec
          << " cost=" << iface.cost
          << " helloInt=" << iface.helloInterval
          << " deadInt=" << iface.routerDeadInterval << "\n";
        f << "       ip=0x" << std::hex << iface.ipAddress << std::dec
          << " mask=0x" << std::hex << iface.mask << std::dec
          << " priority=" << (int)iface.routerPriority << "\n";

        if (iface.neighbor) {
            const NeighborData& nbr = *iface.neighbor;
            f << "       nbr: id=" << nbr.IDNeighbor
              << " state=" << nbrStateName(nbr.state)
              << " priority=" << (int)nbr.priorityNeighbor
              << " master=" << nbr.isMaster
              << " ddSeq=" << nbr.ddSequenceNumber << "\n";
        }
    }

    f << "\n--- Area ---\n";
    f << "  areaID=0x" << std::hex << area.areaID << std::dec
      << " transit=" << area.transitCapability
      << " extRouting=" << area.externalRoutingCapability
      << " stubCost=" << area.stubDefaultCost << "\n";
    f << "  interfaces: ";
    for (int idx : area.interfaceIndices) f << idx << " ";
    f << "\n";
    f << "  LSDB: " << area.routerLSAs.size() << " Router-LSAs\n";
    f << "  SPF vertices: " << area.spfVertices.size() << " entries\n";

    f << "\n--- Routing Table (" << RoutingTable.size() << ") ---\n";
    for (size_t i = 0; i < RoutingTable.size(); i++) {
        const RoutingTableEntry& rte = RoutingTable[i];
        f << "  [" << i << "] dest=0x" << std::hex << rte.destinationId << std::dec
          << " mask=0x" << std::hex << rte.addressMask << std::dec
          << " pathType=" << (int)rte.pathType
          << " cost=" << rte.cost
          << " nextHop=" << rte.nextHop << "\n";
    }

    f.close();
}


// ============================================================
// Serialization helpers (big-endian)
// ============================================================

static void put32(uint8_t* buf, int& off, uint32_t v) {
    buf[off++] = (v >> 24) & 0xFF;
    buf[off++] = (v >> 16) & 0xFF;
    buf[off++] = (v >> 8) & 0xFF;
    buf[off++] = v & 0xFF;
}
static void put16(uint8_t* buf, int& off, uint16_t v) {
    buf[off++] = (v >> 8) & 0xFF;
    buf[off++] = v & 0xFF;
}
static void put8(uint8_t* buf, int& off, uint8_t v) {
    buf[off++] = v;
}
static uint32_t get32(const uint8_t* buf, int& off) {
    uint32_t v = ((uint32_t)buf[off] << 24) | ((uint32_t)buf[off+1] << 16)
               | ((uint32_t)buf[off+2] << 8) | buf[off+3];
    off += 4;
    return v;
}
static uint16_t get16(const uint8_t* buf, int& off) {
    uint16_t v = ((uint16_t)buf[off] << 8) | buf[off+1];
    off += 2;
    return v;
}
static uint8_t get8(const uint8_t* buf, int& off) {
    return buf[off++];
}


// ============================================================
// OspfMess::send — đóng OSPF header + body, gửi qua Mess
// RFC 2328 A.3.1 (OSPF header format)
// ============================================================

void OspfMess::send(uint8_t type, const std::vector<uint8_t>& body,
                    uint32_t routerId, uint32_t areaId,
                    int ifIndex, omnetpp::cSimpleModule* mod)
{
    int totalLen = 24 + body.size();
    std::vector<uint8_t> buf(totalLen);
    int off = 0;

    // OSPF Header 24 byte (A.3.1)
    put8(buf.data(), off, 2);           // Version # = OSPFv2
    put8(buf.data(), off, type);        // Type (1=Hello,2=DD,3=LSR,4=LSU,5=LSAck)
    put16(buf.data(), off, totalLen);   // Packet length
    put32(buf.data(), off, routerId);   // Router ID
    put32(buf.data(), off, areaId);     // Area ID
    put16(buf.data(), off, 0);          // Checksum (không dùng)
    put16(buf.data(), off, 0);          // AuType = Null authentication
    put32(buf.data(), off, 0);          // Authentication
    put32(buf.data(), off, 0);

    // Body
    for (size_t i = 0; i < body.size(); i++)
        buf[24 + i] = body[i];

    // Gửi qua Mess
    Mess* msg = new Mess("OSPF");
    msg->setPayloadArraySize(totalLen);
    for (int i = 0; i < totalLen; i++)
        msg->setPayload(i, buf[i]);

    mod->send(msg, "gate$o", ifIndex);
}


// ============================================================
// OspfMess::parse — tách Mess thành OSPF header + body
// RFC 2328 Section 8.2 (Receiving protocol packets)
// ============================================================

uint8_t OspfMess::parse(const Mess* msg, const InterfaceData* iface,
                         headerOspf& hdr, std::vector<uint8_t>& data)
{
    size_t len = msg->getPayloadArraySize();
    if (len < 24) return 0;

    // Đọc toàn bộ payload vào buffer
    std::vector<uint8_t> buf(len);
    for (size_t i = 0; i < len; i++)
        buf[i] = msg->getPayload(i);

    // Parse OSPF header (24 byte)
    int off = 0;
    hdr.version   = get8(buf.data(), off);
    hdr.type      = get8(buf.data(), off);
    hdr.length    = get16(buf.data(), off);
    hdr.routerId  = get32(buf.data(), off);
    hdr.areaId    = get32(buf.data(), off);
    hdr.checksum  = get16(buf.data(), off);
    hdr.authType  = get16(buf.data(), off);
    hdr.authData1 = get32(buf.data(), off);
    hdr.authData2 = get32(buf.data(), off);

    // Kiểm tra tính hợp lệ (Section 8.2)
    if (hdr.version != 2)        return 0;
    if (hdr.areaId != iface->areaID) return 0;
    if (hdr.authType != 0)       return 0;

    // Body = phần còn lại sau header
    data.assign(buf.begin() + 24, buf.end());
    return hdr.type;
}


// ============================================================
// helloData::sendHello
// RFC 2328 Section 9.5 + A.3.2 — tạo body, gọi OspfMess::send
// ============================================================

void helloData::sendHello(int ifIndex, OspfRouterState& state, uint32_t routerId,
                          omnetpp::cSimpleModule* mod)
{
    InterfaceData* iface = &state.interfaces[ifIndex];
    NeighborData* nbr = iface->neighbor;

    // Tạo Hello body (RFC A.3.2)
    int nNbr = (nbr->IDNeighbor != 0) ? 1 : 0;
    int bodyLen = 20 + nNbr * 4;
    std::vector<uint8_t> body(bodyLen);
    int off = 0;
    put32(body.data(), off, iface->mask);                  // Network Mask
    put16(body.data(), off, iface->helloInterval);         // HelloInterval
    put8(body.data(), off, OPT_E);                          // Options (E-bit=1)
    put8(body.data(), off, iface->routerPriority);         // Router Priority
    put32(body.data(), off, iface->routerDeadInterval);    // RouterDeadInterval
    put32(body.data(), off, 0);                             // DR (P2P=0)
    put32(body.data(), off, 0);                             // BDR (P2P=0)
    if (nNbr)
        put32(body.data(), off, nbr->IDNeighbor);          // neighbor list

    OspfMess::send(1, body, routerId, iface->areaID, ifIndex, mod);
}


// ============================================================
// helloData::processHello
// RFC 2328 Section 10.5 (Receiving Hello packets)
// ============================================================

void helloData::processHello(const headerOspf& hdr, const std::vector<uint8_t>& data,
                              InterfaceData* iface, int ifIndex, uint32_t myRouterId,
                              omnetpp::cSimpleModule* mod)
{
    if (data.size() < 20) return;  // tối thiểu 20 byte body (không có neighbor)

    // Parse Hello body (RFC A.3.2)
    helloData hello;
    int off = 0;
    hello.networkMask         = get32(data.data(), off);
    hello.helloInterval       = get16(data.data(), off);
    hello.options             = get8(data.data(), off);
    hello.routerPriority      = get8(data.data(), off);
    hello.routerDeadInterval  = get32(data.data(), off);
    hello.designatedRouter    = get32(data.data(), off);
    hello.backupDesignatedRouter = get32(data.data(), off);

    // Danh sách neighbor (nếu có)
    int nbrCount = ((int)data.size() - 20) / 4;
    for (int i = 0; i < nbrCount; i++)
        hello.neighborId.push_back(get32(data.data(), off));

    // Kiểm tra tham số cấu hình phải khớp (Section 10.5)
    if (hello.helloInterval != iface->helloInterval)       return;
    if (hello.routerDeadInterval != iface->routerDeadInterval) return;

    // Kiểm tra E-bit: backbone (area 0) bắt buộc có E-bit
    bool eBit = (hello.options & OPT_E);
    if (iface->areaID == 0 && !eBit)            return;

    // Cập nhật neighbor
    NeighborData* nbr = iface->neighbor;
    if (nbr->IDNeighbor == 0)
        nbr->IDNeighbor = hdr.routerId;        // học Router ID từ header
    nbr->priorityNeighbor = hello.routerPriority;

    // Bước 5: HelloReceived → chuyển trạng thái + reset inactivity timer
    bool helloReceived = true;  // Hello đã pass tất cả bước kiểm tra

    if (helloReceived) {
        // Reset inactivity timer (Section 10.5, bước 5)
        if (nbr->inactivityTimer) {
            mod->cancelEvent(nbr->inactivityTimer);
            delete nbr->inactivityTimer;
        }
        nbr->inactivityTimer = new omnetpp::cMessage("inactivityTimer");
        mod->scheduleAt(omnetpp::simTime() + iface->routerDeadInterval, nbr->inactivityTimer);

        if (nbr->state == NBR_DOWN)
            nbr->state = NBR_INIT;
    }

    // Bước 6: Kiểm tra 2-way (có thấy Router ID của mình trong neighbor list?)
    bool seen = false;
    for (uint32_t nid : hello.neighborId) {
        if (nid == myRouterId) { seen = true; break; }
    }

    if (seen && nbr->state < NBR_TWOWAY) {
        nbr->state = NBR_TWOWAY;
        // P2P: đạt 2Way → khởi động adjacency ngay (Section 7.2)
        databaseDescriptionData::sendExStart(iface, ifIndex, myRouterId, mod);
    }
    else if (!seen && nbr->state >= NBR_TWOWAY)
        nbr->state = NBR_INIT;
}


// ============================================================
// databaseDescriptionData::sendExStart
// RFC 2328 Section 10.3 — BƯỚC 1: gửi DD rỗng khởi động ExStart
// ============================================================

void databaseDescriptionData::sendExStart(InterfaceData* iface, int ifIndex, uint32_t routerId,
                                          omnetpp::cSimpleModule* mod)
{
    NeighborData* nbr = iface->neighbor;

    // 1. P2P → luôn form adjacency (Section 10.4)

    // 2. Tăng DD sequence number, lần đầu gán unique từ clock
    if (nbr->ddSequenceNumber == 0)
        nbr->ddSequenceNumber = (uint32_t)omnetpp::simTime().raw();
    else
        nbr->ddSequenceNumber++;

    // 3. Tự đặt Master (tạm thời, sẽ thương lượng ở BƯỚC 2)
    nbr->isMaster = true;
    nbr->state = NBR_EXSTART;

    // 4. Gửi DD rỗng: I=1, M=1, MS=1, không có LSA header
    //    Flags: bit2=I, bit1=M, bit0=MS → 0x07
    std::vector<uint8_t> body(8);
    int off = 0;
    put16(body.data(), off, 1500);   // Interface MTU
    put8(body.data(), off, 0x02);    // Options (E-bit=1)
    put8(body.data(), off, DD_I | DD_M | DD_MS);    // I=1, M=1, MS=1
    put32(body.data(), off, nbr->ddSequenceNumber);

    OspfMess::send(2, body, routerId, iface->areaID, ifIndex, mod);

    // 5. Lên lịch rxmtTimer = RxmtInterval
    if (nbr->rxmtTimer) {
        mod->cancelEvent(nbr->rxmtTimer);
        delete nbr->rxmtTimer;
    }
    nbr->rxmtTimer = new omnetpp::cMessage("rxmtTimer");
    nbr->rxmtTimer->setKind(ifIndex);  // để biết timer thuộc interface nào
    mod->scheduleAt(omnetpp::simTime() + iface->rxmtInterval, nbr->rxmtTimer);
}


// ============================================================
// databaseDescriptionData::processExStart
// RFC 2328 Section 10.6 — BƯỚC 2: nhận DD, thương lượng Master/Slave
// ============================================================

void databaseDescriptionData::processExStart(const headerOspf& hdr,
                                             const std::vector<uint8_t>& data,
                                             InterfaceData* iface, int ifIndex,
                                             uint32_t myRouterId,
                                             OspfRouterState& state,
                                             omnetpp::cSimpleModule* mod)
{
    NeighborData* nbr = iface->neighbor;
    if (data.size() < 8) return;   // tối thiểu 8 byte body (MTU + Options + Flags + Seq)

    // Parse DD body
    int off = 0;
    uint16_t pktMTU     = get16(data.data(), off);
    uint8_t  pktOptions = get8(data.data(), off);
    uint8_t  pktFlags   = get8(data.data(), off);
    uint32_t pktSeq     = get32(data.data(), off);

    bool I_bit  = (pktFlags & DD_I);
    bool M_bit  = (pktFlags & DD_M);

    // Bỏ qua DD có I=1 (ExStart DD đến muộn) — tránh SeqNumberMismatch vô hạn
    bool MS_bit = (pktFlags & DD_MS);

    // 2. Lưu DD fields vào neighbor, phát hiện duplicate
    uint8_t prevFlags = nbr->lastDdIMs;
    if (prevFlags == pktFlags && nbr->lastDdOptions == pktOptions
        && nbr->ddSequenceNumber == pktSeq)
        return;  // duplicate → bỏ qua

    nbr->lastDdOptions = pktOptions;
    nbr->lastDdIMs     = pktFlags;

    // 3. Kiểm tra Interface MTU
    if (pktMTU > 1500) return;  // MTU neighbor > MTU mình → drop

    // 4. Phân tích quyết định Master/Slave
    bool bodyRong = (data.size() == 8);  // không có LSA header nào

    // CASE A: Neighbor claim Master, mình thành Slave
    //   Điều kiện: I=1, M=1, MS=1, packet rỗng, neighbor RouterID > mình
    if (I_bit && M_bit && MS_bit && bodyRong && hdr.routerId > myRouterId) {
        // Hủy rxmtTimer vì đã sang Exchange
        if (nbr->rxmtTimer) {
            mod->cancelEvent(nbr->rxmtTimer);
            delete nbr->rxmtTimer;
            nbr->rxmtTimer = nullptr;
        }
        nbr->isMaster = false;                       // mình là Slave
        nbr->ddSequenceNumber = pktSeq;              // dùng seq của Master
        nbr->state = NBR_EXCHANGE;                   // NegotiationDone → Exchange
        // Gửi ACK: DD(I=0, MS=0) với seq của Master
        std::vector<uint8_t> ack(8);
        int o = 0;
        put16(ack.data(), o, 1500);
        put8(ack.data(), o, 0x02);
        put8(ack.data(), o, 0x00);   // I=0, M=0, MS=0
        put32(ack.data(), o, nbr->ddSequenceNumber);
        OspfMess::send(2, ack, myRouterId, iface->areaID, ifIndex, mod);
        return;
    }

    // CASE B: Neighbor ACK, mình là Master
    //   Điều kiện: I=0, MS=0, seq == mình đã gửi, neighbor RouterID < mình
    if (!I_bit && !MS_bit && pktSeq == nbr->ddSequenceNumber && hdr.routerId < myRouterId) {
        // Hủy rxmtTimer vì đã sang Exchange
        if (nbr->rxmtTimer) {
            mod->cancelEvent(nbr->rxmtTimer);
            delete nbr->rxmtTimer;
            nbr->rxmtTimer = nullptr;
        }
        nbr->isMaster = true;                        // xác nhận mình là Master
        nbr->state = NBR_EXCHANGE;                   // NegotiationDone → Exchange
        // Master tăng seq trước khi gửi DD Exchange đầu tiên
        // (Slave expect pktSeq == nbr->ddSequenceNumber + 1)
        nbr->ddSequenceNumber++;
        databaseDescriptionData::sendExchange(iface, ifIndex, myRouterId, state, mod);
        return;
    }

    // Không khớp case nào → bỏ qua, chờ retransmit
}


// ============================================================
// databaseDescriptionData::sendExchange
// GIAI ĐOẠN 2 BƯỚC 1: Master gửi DD chứa LSA headers từ LSDB
// ============================================================

void databaseDescriptionData::sendExchange(InterfaceData* iface, int ifIndex,
                                           uint32_t routerId, OspfRouterState& state,
                                           omnetpp::cSimpleModule* mod)
{
    NeighborData* nbr = iface->neighbor;

    // Lấy LSA headers từ LSDB (area.routerLSAs)
    std::vector<LSAHeader> headers;
    for (const LSA& lsa : state.area.routerLSAs) {
        headers.push_back(lsa.header);
    }

    // Xác định M bit: M=1 nếu còn LSA chưa gửi, M=0 nếu hết
    bool hasMore = !headers.empty();  // đơn giản: gửi hết 1 lần
    uint8_t flags = DD_MS;              // I=0, MS=1 (Master), M=0
    if (hasMore) flags |= DD_M;         // M=1

    // Dựng DD body: 8 byte header + LSA headers
    int bodyLen = 8 + headers.size() * 20;
    std::vector<uint8_t> body(bodyLen);
    int off = 0;
    put16(body.data(), off, 1500);                  // Interface MTU
    put8(body.data(), off, 0x02);                   // Options (E-bit=1)
    put8(body.data(), off, flags);                  // I=0, MS=1, M
    put32(body.data(), off, nbr->ddSequenceNumber);

    // Mỗi LSA header 20 byte (RFC A.4.1)
    for (const LSAHeader& h : headers) {
        put16(body.data(), off, h.age);
        put8(body.data(), off, h.options);
        put8(body.data(), off, h.type);
        put32(body.data(), off, h.linkStateId);
        put32(body.data(), off, h.advertisingRouter);
        put32(body.data(), off, h.sequenceNumber);
        put16(body.data(), off, h.checksum);
        put16(body.data(), off, h.length);
    }

    OspfMess::send(2, body, routerId, iface->areaID, ifIndex, mod);

    // Lên lịch rxmtTimer
    if (nbr->rxmtTimer) {
        mod->cancelEvent(nbr->rxmtTimer);
        delete nbr->rxmtTimer;
    }
    nbr->rxmtTimer = new omnetpp::cMessage("rxmtTimer");
    nbr->rxmtTimer->setKind(ifIndex);
    mod->scheduleAt(omnetpp::simTime() + iface->rxmtInterval, nbr->rxmtTimer);
}


// ============================================================
// databaseDescriptionData::processExchange
// GIAI ĐOẠN 2 BƯỚC 2: Master nhận ACK từ Slave
// (RFC 2328 Section 10.6 — Exchange state, Master path)
// ============================================================

void databaseDescriptionData::processExchangeForMaster(const headerOspf& hdr,
                                                       const std::vector<uint8_t>& data,
                                                       InterfaceData* iface, int ifIndex,
                                                       uint32_t myRouterId,
                                                       OspfRouterState& state,
                                                       omnetpp::cSimpleModule* mod)
{
    NeighborData* nbr = iface->neighbor;
    if (data.size() < 8) return;

    // 1. Parse DD body
    int off = 0;
    uint16_t pktMTU     = get16(data.data(), off);
    uint8_t  pktOptions = get8(data.data(), off);
    uint8_t  pktFlags   = get8(data.data(), off);
    uint32_t pktSeq     = get32(data.data(), off);

    bool M_bit  = (pktFlags & DD_M);

    // ExStart DD cũ (I=1) bay lạc vào Exchange → bỏ qua, không SeqNumberMismatch
    if (pktFlags & DD_I) return;

    // Parse LSA headers từ Slave
    std::vector<LSAHeader> rxHeaders;
    int remain = (int)data.size() - 8;
    while (remain >= 20) {
        LSAHeader lh;
        lh.age             = get16(data.data(), off);
        lh.options         = get8(data.data(), off);
        lh.type            = get8(data.data(), off);
        lh.linkStateId     = get32(data.data(), off);
        lh.advertisingRouter = get32(data.data(), off);
        lh.sequenceNumber  = get32(data.data(), off);
        lh.checksum        = get16(data.data(), off);
        lh.length          = get16(data.data(), off);
        rxHeaders.push_back(lh);
        remain -= 20;
    }

    // 2. Master: expect Slave echo seq mình gửi
    //    RFC: "If the router is master, the next packet received should have
    //          DD sequence number equal to the DD sequence number in the
    //          neighbor data structure."
    if (pktSeq != nbr->ddSequenceNumber) {
        // SeqNumberMismatch → quay về ExStart
        databaseDescriptionData::sendExStart(iface, ifIndex, myRouterId, mod);
        return;
    }

    // 3. So sánh LSA headers từ Slave với LSDB của mình (Section 13.1)
    for (const LSAHeader& rxh : rxHeaders) {
        bool needRequest = false;
        bool found = false;
        for (const LSA& myLsa : state.area.routerLSAs) {
            if (myLsa.header.type == rxh.type &&
                myLsa.header.linkStateId == rxh.linkStateId &&
                myLsa.header.advertisingRouter == rxh.advertisingRouter)
            {
                found = true;
                if ((int32_t)rxh.sequenceNumber > (int32_t)myLsa.header.sequenceNumber)
                    needRequest = true;
                break;
            }
        }
        if (!found || needRequest) {
            LSARequest req;
            req.LSType = rxh.type;
            req.linkStateId = rxh.linkStateId;
            req.advertisingRouter = rxh.advertisingRouter;
            nbr->linkStateRetransmissionList.push_back(req);
        }
    }

    // 4. Cập nhật neighbor data + increment seq
    nbr->lastDdOptions = pktOptions;
    nbr->lastDdIMs = pktFlags;
    nbr->ddSequenceNumber++;  // sẵn sàng cho DD tiếp theo

    // 5. ExchangeDone: mình hết LSA + Slave M=0
    if (state.area.routerLSAs.empty() && !M_bit) {
        if (nbr->rxmtTimer) {
            mod->cancelEvent(nbr->rxmtTimer);
            delete nbr->rxmtTimer;
            nbr->rxmtTimer = nullptr;
        }
        // RFC 10.3: request list rỗng → Full, không rỗng → Loading
        if (nbr->linkStateRetransmissionList.empty()) {
            nbr->state = NBR_FULL;
        } else {
            nbr->state = NBR_LOADING;
            linkStateRequestData::sendLSR(iface, ifIndex, myRouterId,
                                          nbr->linkStateRetransmissionList, mod);
        }
        return;
    }

    // Còn LSA chưa gửi → gửi DD tiếp
    databaseDescriptionData::sendExchange(iface, ifIndex, myRouterId, state, mod);
}


// ============================================================
// GIAI ĐOẠN 2 BƯỚC 2: Slave nhận DD từ Master, so sánh LSDB, gửi ACK
// (RFC 2328 Section 10.6 — Exchange state, Slave path)
// ============================================================

void databaseDescriptionData::processExchangeForSlave(const headerOspf& hdr,
                                                      const std::vector<uint8_t>& data,
                                                      InterfaceData* iface, int ifIndex,
                                                      uint32_t myRouterId,
                                                      OspfRouterState& state,
                                                      omnetpp::cSimpleModule* mod)
{
    NeighborData* nbr = iface->neighbor;
    if (data.size() < 8) return;

    // 1. Parse DD body
    int off = 0;
    uint16_t pktMTU     = get16(data.data(), off);
    uint8_t  pktOptions = get8(data.data(), off);
    uint8_t  pktFlags   = get8(data.data(), off);
    uint32_t pktSeq     = get32(data.data(), off);

    bool M_bit  = (pktFlags & 0x02);
    // Bỏ qua DD có I=1 (ExStart DD cũ bay lạc) — tránh SeqNumberMismatch vô hạn
    if (pktFlags & 0x04) return;

    // Parse LSA headers từ Master
    std::vector<LSAHeader> rxHeaders;
    int remain = (int)data.size() - 8;
    while (remain >= 20) {
        LSAHeader lh;
        lh.age             = get16(data.data(), off);
        lh.options         = get8(data.data(), off);
        lh.type            = get8(data.data(), off);
        lh.linkStateId     = get32(data.data(), off);
        lh.advertisingRouter = get32(data.data(), off);
        lh.sequenceNumber  = get32(data.data(), off);
        lh.checksum        = get16(data.data(), off);
        lh.length          = get16(data.data(), off);
        rxHeaders.push_back(lh);
        remain -= 20;
    }

    // 2. Slave: expect Master tăng seq rồi gửi
    //    RFC: "If the router is slave, the next packet received should have
    //          DD sequence number equal to one more than the DD sequence
    //          number stored in the neighbor data structure."
    if (pktSeq == nbr->ddSequenceNumber) {
        // Duplicate → gửi lại DD cũ (rebuild từ LSDB)
        goto sendAck;
    }

    if (pktSeq != nbr->ddSequenceNumber + 1) {
        // SeqNumberMismatch → quay về ExStart
        databaseDescriptionData::sendExStart(iface, ifIndex, myRouterId, mod);
        return;
    }

    // 3. So sánh từng LSA header với LSDB của mình (Section 13.1)
    for (const LSAHeader& rxh : rxHeaders) {
        bool needRequest = false;
        bool found = false;
        for (const LSA& myLsa : state.area.routerLSAs) {
            if (myLsa.header.type == rxh.type &&
                myLsa.header.linkStateId == rxh.linkStateId &&
                myLsa.header.advertisingRouter == rxh.advertisingRouter)
            {
                found = true;
                // Mình có + neighbor seq mới hơn → cần xin
                if ((int32_t)rxh.sequenceNumber > (int32_t)myLsa.header.sequenceNumber)
                    needRequest = true;
                break;
            }
        }
        if (!found || needRequest) {
            // Mình không có hoặc neighbor mới hơn → thêm vào request list
            LSARequest req;
            req.LSType = rxh.type;
            req.linkStateId = rxh.linkStateId;
            req.advertisingRouter = rxh.advertisingRouter;
            nbr->linkStateRetransmissionList.push_back(req);
        }
    }

    // 4. Cập nhật neighbor data
    nbr->lastDdOptions = pktOptions;
    nbr->lastDdIMs = pktFlags;
    nbr->ddSequenceNumber = pktSeq;  // lưu seq từ Master

sendAck:
    // 5. Gửi DD phản hồi (ACK): echo seq của Master, kèm LSA headers của mình
    {
        std::vector<LSAHeader> myHeaders;
        for (const LSA& lsa : state.area.routerLSAs)
            myHeaders.push_back(lsa.header);

        bool myMore = !myHeaders.empty();
        uint8_t ackFlags = 0x00;  // I=0, MS=0
        if (myMore) ackFlags |= DD_M;  // M=1

        int bodyLen = 8 + myHeaders.size() * 20;
        std::vector<uint8_t> ack(bodyLen);
        int o = 0;
        put16(ack.data(), o, 1500);
        put8(ack.data(), o, 0x02);
        put8(ack.data(), o, ackFlags);
        put32(ack.data(), o, nbr->ddSequenceNumber);  // echo seq của Master

        for (const LSAHeader& h : myHeaders) {
            put16(ack.data(), o, h.age);
            put8(ack.data(), o, h.options);
            put8(ack.data(), o, h.type);
            put32(ack.data(), o, h.linkStateId);
            put32(ack.data(), o, h.advertisingRouter);
            put32(ack.data(), o, h.sequenceNumber);
            put16(ack.data(), o, h.checksum);
            put16(ack.data(), o, h.length);
        }

        OspfMess::send(2, ack, myRouterId, iface->areaID, ifIndex, mod);
    }

    // 6. ExchangeDone (Slave): Master M=0 và mình M=0
    if (!M_bit && state.area.routerLSAs.empty()) {
        if (nbr->rxmtTimer) {
            mod->cancelEvent(nbr->rxmtTimer);
            delete nbr->rxmtTimer;
            nbr->rxmtTimer = nullptr;
        }
        // RFC 10.3: request list rỗng → Full, không rỗng → Loading
        if (nbr->linkStateRetransmissionList.empty()) {
            nbr->state = NBR_FULL;
        } else {
            nbr->state = NBR_LOADING;
            linkStateRequestData::sendLSR(iface, ifIndex, myRouterId,
                                          nbr->linkStateRetransmissionList, mod);
        }
    }
}


// ============================================================
// ============================================================
// FLOW A — BƯỚC A1: gửi Link State Request (type 3)
// RFC 10.9 + A.3.4 — mỗi request = 12 byte (LS type + LinkStateID + AdvRouter)
// ============================================================

void linkStateRequestData::sendLSR(InterfaceData* iface, int ifIndex,
                                       uint32_t routerId,
                                       const std::vector<LSARequest>& reqs,
                                       omnetpp::cSimpleModule* mod)
{
    NeighborData* nbr = iface->neighbor;
    if (reqs.empty()) return;

    // Đóng gói LSR body: mỗi request 12 byte (LS type + LinkStateID + AdvRouter)
    int bodyLen = reqs.size() * 12;
    std::vector<uint8_t> body(bodyLen);
    int off = 0;
    for (const LSARequest& r : reqs) {
        put32(body.data(), off, r.LSType);
        put32(body.data(), off, r.linkStateId);
        put32(body.data(), off, r.advertisingRouter);
    }

    // Gửi packet type 3
    OspfMess::send(3, body, routerId, iface->areaID, ifIndex, mod);

    // Lên lịch rxmtTimer = RxmtInterval (chưa có phản hồi → retransmit)
    if (nbr->rxmtTimer) {
        mod->cancelEvent(nbr->rxmtTimer);
        delete nbr->rxmtTimer;
    }
    nbr->rxmtTimer = new omnetpp::cMessage("rxmtTimer");
    nbr->rxmtTimer->setKind(ifIndex);
    mod->scheduleAt(omnetpp::simTime() + iface->rxmtInterval, nbr->rxmtTimer);
}


// ============================================================
// linkStateAcknowledgementData::sendAck
// Gửi Link State Acknowledgment (type 5) — danh sách LSAHeader
// RFC A.3.6 — body = các LSAHeader, mỗi cái 20 byte
// ============================================================

void linkStateAcknowledgementData::sendAck(const std::vector<LSAHeader>& headers,
                                           InterfaceData* iface, int ifIndex,
                                           uint32_t routerId,
                                           omnetpp::cSimpleModule* mod)
{
    int bodyLen = headers.size() * 20;
    std::vector<uint8_t> body(bodyLen);
    int off = 0;
    for (const LSAHeader& h : headers) {
        put16(body.data(), off, h.age);
        put8(body.data(), off, h.options);
        put8(body.data(), off, h.type);
        put32(body.data(), off, h.linkStateId);
        put32(body.data(), off, h.advertisingRouter);
        put32(body.data(), off, h.sequenceNumber);
        put16(body.data(), off, h.checksum);
        put16(body.data(), off, h.length);
    }
    OspfMess::send(5, body, routerId, iface->areaID, ifIndex, mod);
}


// ============================================================
// linkStateUpdateData::processLSU
// FLOW A — BƯỚC A2: nhận LSU, cài LSDB, gửi LSAck
// RFC 13.2 + 13.5 — parse LSU → cài từng LSA vào LSDB → ACK
// ============================================================

void linkStateUpdateData::processLSU(const headerOspf& hdr,
                                     const std::vector<uint8_t>& data,
                                     InterfaceData* iface, int ifIndex,
                                     uint32_t myRouterId,
                                     OspfRouterState& state,
                                     omnetpp::cSimpleModule* mod)
{
    NeighborData* nbr = iface->neighbor;
    if (data.size() < 4) return;

    // 1. Parse LSU body
    int off = 0;
    uint32_t nLSA = get32(data.data(), off);

    std::vector<LSA> receivedLSAs;
    int remain = (int)data.size() - 4;
    for (uint32_t i = 0; i < nLSA && remain >= 20; i++) {
        LSA lsa;
        lsa.header.age             = get16(data.data(), off);
        lsa.header.options         = get8(data.data(), off);
        lsa.header.type            = get8(data.data(), off);
        lsa.header.linkStateId     = get32(data.data(), off);
        lsa.header.advertisingRouter = get32(data.data(), off);
        lsa.header.sequenceNumber  = get32(data.data(), off);
        lsa.header.checksum        = get16(data.data(), off);
        lsa.header.length          = get16(data.data(), off);
        remain -= 20;

        // Phần body LSA = length - 20 (tạm bỏ qua với Router-LSA đơn giản)
        int lsaBodyLen = lsa.header.length - 20;
        if (lsaBodyLen > 0 && remain >= lsaBodyLen) {
            off += lsaBodyLen;
            remain -= lsaBodyLen;
        }
        receivedLSAs.push_back(lsa);
    }

    std::vector<LSAHeader> ackList;

    // 2. Với mỗi LSA: RFC 13 step (6) check BadLSReq, step (5) cài vào LSDB
    for (LSA& rxLsa : receivedLSAs) {
        // RFC 13 step (6): nếu LSA đã có trong request list của neighbor gửi → BadLSReq
        for (const LSARequest& req : nbr->linkStateRetransmissionList) {
            if (req.LSType == rxLsa.header.type &&
                req.linkStateId == rxLsa.header.linkStateId &&
                req.advertisingRouter == rxLsa.header.advertisingRouter)
            {
                // Lỗi Database Exchange → quay về ExStart
                nbr->state = NBR_EXSTART;
                if (nbr->rxmtTimer) {
                    mod->cancelEvent(nbr->rxmtTimer);
                    delete nbr->rxmtTimer;
                    nbr->rxmtTimer = nullptr;
                }
                return;
            }
        }

        // Tìm LSA trong LSDB
        bool found = false;
        bool installed = false;
        for (LSA& myLsa : state.area.routerLSAs) {
            if (myLsa.header.type == rxLsa.header.type &&
                myLsa.header.linkStateId == rxLsa.header.linkStateId &&
                myLsa.header.advertisingRouter == rxLsa.header.advertisingRouter)
            {
                found = true;
                // Seq mới hơn → thay thế (RFC 13 step 5d)
                if ((int32_t)rxLsa.header.sequenceNumber > (int32_t)myLsa.header.sequenceNumber) {
                    myLsa = rxLsa;
                    installed = true;
                }
                break;
            }
        }
        if (!found) {
            // Chưa có → thêm mới
            state.area.routerLSAs.push_back(rxLsa);
            installed = true;
        }

        // Chỉ ack LSA thực sự được cài đặt (RFC 13.5)
        if (installed)
            ackList.push_back(rxLsa.header);
    }

    // Gửi LSAck (type 5) — RFC 13 step (5e)
    if (!ackList.empty())
        linkStateAcknowledgementData::sendAck(ackList, iface, ifIndex, myRouterId, mod);

    // 3. Xóa LSA đã nhận khỏi request list
    for (const LSAHeader& ackHdr : ackList) {
        for (auto it = nbr->linkStateRetransmissionList.begin();
             it != nbr->linkStateRetransmissionList.end(); ++it) {
            if (it->LSType == ackHdr.type &&
                it->linkStateId == ackHdr.linkStateId &&
                it->advertisingRouter == ackHdr.advertisingRouter) {
                nbr->linkStateRetransmissionList.erase(it);
                break;
            }
        }
    }

    // 4. Hủy rxmtTimer
    if (nbr->rxmtTimer) {
        mod->cancelEvent(nbr->rxmtTimer);
        delete nbr->rxmtTimer;
        nbr->rxmtTimer = nullptr;
    }

    // 5. Kiểm tra request list → gửi tiếp hoặc LoadingDone → Full
    if (!nbr->linkStateRetransmissionList.empty()) {
        // Còn LSA cần xin → gửi LSR tiếp (RFC 10.9)
        linkStateRequestData::sendLSR(iface, ifIndex, myRouterId,
                                      nbr->linkStateRetransmissionList, mod);
    } else if (nbr->state == NBR_LOADING) {
        // Request list rỗng + đang Loading → LoadingDone → Full (RFC 10.3)
        nbr->state = NBR_FULL;
    }
}


// ============================================================
// linkStateUpdateData::sendLSU
// FLOW B: gửi Link State Update (type 4) trả lời LS Request
// RFC A.3.5 — numberOfLSA + các LSA đầy đủ
// ============================================================

void linkStateUpdateData::sendLSU(const std::vector<LSA>& lsas,
                                  InterfaceData* iface, int ifIndex,
                                  uint32_t routerId, omnetpp::cSimpleModule* mod)
{
    // Tính body: 4 byte numberOfLSA + mỗi LSA (header 20B)
    int bodyLen = 4;
    for (const LSA& lsa : lsas) {
        bodyLen += 20;  // LSA header
        // TODO: + LSA body khi có originate Router-LSA
    }

    std::vector<uint8_t> body(bodyLen);
    int off = 0;
    put32(body.data(), off, lsas.size());  // numberOfLSA

    for (const LSA& lsa : lsas) {
        put16(body.data(), off, lsa.header.age);
        put8(body.data(), off, lsa.header.options);
        put8(body.data(), off, lsa.header.type);
        put32(body.data(), off, lsa.header.linkStateId);
        put32(body.data(), off, lsa.header.advertisingRouter);
        put32(body.data(), off, lsa.header.sequenceNumber);
        put16(body.data(), off, lsa.header.checksum);
        put16(body.data(), off, lsa.header.length);
        // TODO: serialize LSA body (flags, zero, numLinks, links)
    }

    OspfMess::send(4, body, routerId, iface->areaID, ifIndex, mod);
}


// ============================================================
// linkStateRequestData::processLSR
// FLOW B: nhận LS Request (type 3) từ neighbor,
// tìm LSA trong LSDB, gửi LSU trả lời (RFC 10.7)
// ============================================================

void linkStateRequestData::processLSR(const headerOspf& hdr,
                                      const std::vector<uint8_t>& data,
                                      InterfaceData* iface, int ifIndex,
                                      uint32_t myRouterId,
                                      OspfRouterState& state,
                                      omnetpp::cSimpleModule* mod)
{
    NeighborData* nbr = iface->neighbor;

    // RFC 10.7: chỉ chấp nhận khi neighbor ở Exchange, Loading, hoặc Full
    if (nbr->state != NBR_EXCHANGE &&
        nbr->state != NBR_LOADING &&
        nbr->state != NBR_FULL)
        return;

    // Parse LSR body: mỗi request 12 byte (LS type + LinkStateID + AdvRouter)
    int off = 0;
    int remain = (int)data.size();
    std::vector<LSA> foundLSAs;

    while (remain >= 12) {
        LSARequest req;
        req.LSType           = get32(data.data(), off);
        req.linkStateId      = get32(data.data(), off);
        req.advertisingRouter = get32(data.data(), off);
        remain -= 12;

        // Tìm LSA trong LSDB
        bool found = false;
        for (const LSA& myLsa : state.area.routerLSAs) {
            if (myLsa.header.type == req.LSType &&
                myLsa.header.linkStateId == req.linkStateId &&
                myLsa.header.advertisingRouter == req.advertisingRouter)
            {
                foundLSAs.push_back(myLsa);
                found = true;
                break;
            }
        }

        // RFC 10.7: không tìm thấy LSA → BadLSReq → quay về ExStart
        if (!found) {
            nbr->state = NBR_EXSTART;
            databaseDescriptionData::sendExStart(iface, ifIndex, myRouterId, mod);
            return;
        }
    }

    // Gửi LSU chứa các LSA đã tìm thấy (RFC 10.7: KHÔNG thêm vào retransmission list)
    if (!foundLSAs.empty())
        linkStateUpdateData::sendLSU(foundLSAs, iface, ifIndex, myRouterId, mod);
}


// ============================================================
// linkStateAcknowledgementData::processAck
// Nhận LSAck (type 5), xóa LSA đã ack khỏi retransmission list
// ============================================================

void linkStateAcknowledgementData::processAck(const headerOspf& hdr,
                                              const std::vector<uint8_t>& data,
                                              InterfaceData* iface, int ifIndex,
                                              OspfRouterState& state,
                                              omnetpp::cSimpleModule* mod)
{
    NeighborData* nbr = iface->neighbor;
    if (data.size() < 20) return;

    // Parse LSAck body: danh sách LSAHeader (mỗi cái 20 byte)
    int off = 0;
    int remain = (int)data.size();

    while (remain >= 20) {
        LSAHeader ackHdr;
        ackHdr.age              = get16(data.data(), off);
        ackHdr.options          = get8(data.data(), off);
        ackHdr.type             = get8(data.data(), off);
        ackHdr.linkStateId      = get32(data.data(), off);
        ackHdr.advertisingRouter = get32(data.data(), off);
        ackHdr.sequenceNumber   = get32(data.data(), off);
        ackHdr.checksum         = get16(data.data(), off);
        ackHdr.length           = get16(data.data(), off);
        remain -= 20;

        // Xóa LSA đã ack khỏi retransmission list
        for (auto it = nbr->linkStateRetransmissionList.begin();
             it != nbr->linkStateRetransmissionList.end(); ++it) {
            if (it->LSType == ackHdr.type &&
                it->linkStateId == ackHdr.linkStateId &&
                it->advertisingRouter == ackHdr.advertisingRouter) {
                nbr->linkStateRetransmissionList.erase(it);
                break;
            }
        }
    }
}
