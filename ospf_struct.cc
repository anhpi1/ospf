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
        iface->neighbor->neighborID = 0;
        iface->neighbor->state = NBR_DOWN;
        iface->neighbor->inactivityTimer = nullptr;
        iface->neighbor->isMaster = false;
        iface->neighbor->ddSequenceNumber = 0;
        iface->neighbor->lastDdOptions = 0;
        iface->neighbor->lastDdIMs = 0;
        iface->neighbor->priority = 0;
    }

    area.areaID = 0;
    area.transitCapability = false;
    area.externalRoutingCapability = true;
    area.stubDefaultCost = 1;
    area.interfaceIndices.clear();
    for (int i = 0; i < numInterfaces; i++)
        area.interfaceIndices.push_back(i);
    area.routerLSAs.clear();
    area.spfTree.clear();

    routingTable.clear();
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
            f << "       nbr: id=" << nbr.neighborID
              << " state=" << nbrStateName(nbr.state)
              << " priority=" << (int)nbr.priority
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
    f << "  SPF tree: " << area.spfTree.size() << " entries\n";

    f << "\n--- Routing Table (" << routingTable.size() << ") ---\n";
    for (size_t i = 0; i < routingTable.size(); i++) {
        const RoutingTableEntry& rte = routingTable[i];
        f << "  [" << i << "] dest=0x" << std::hex << rte.destinationId << std::dec
          << " mask=0x" << std::hex << rte.addressMask << std::dec
          << " pathType=" << (int)rte.pathType
          << " cost=" << rte.cost
          << " nextHopIf=" << rte.nextHopInterfaceIndex
          << " nextHopRtr=0x" << std::hex << rte.nextHopRouterID << std::dec << "\n";
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
    int nNbr = (nbr->neighborID != 0) ? 1 : 0;
    int bodyLen = 20 + nNbr * 4;
    std::vector<uint8_t> body(bodyLen);
    int off = 0;
    put32(body.data(), off, iface->mask);                  // Network Mask
    put16(body.data(), off, iface->helloInterval);         // HelloInterval
    put8(body.data(), off, 0x02);                          // Options (E-bit=1)
    put8(body.data(), off, iface->routerPriority);         // Router Priority
    put32(body.data(), off, iface->routerDeadInterval);    // RouterDeadInterval
    put32(body.data(), off, 0);                             // DR (P2P=0)
    put32(body.data(), off, 0);                             // BDR (P2P=0)
    if (nNbr)
        put32(body.data(), off, nbr->neighborID);          // neighbor list

    OspfMess::send(1, body, routerId, iface->areaID, ifIndex, mod);
}


// ============================================================
// helloData::processHello
// RFC 2328 Section 10.5 (Receiving Hello packets)
// ============================================================

void helloData::processHello(const headerOspf& hdr, const std::vector<uint8_t>& data,
                              InterfaceData* iface, uint32_t myRouterId)
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
    bool eBit = (hello.options & 0x02);
    if (iface->areaID == 0 && !eBit)            return;

    // Cập nhật neighbor
    NeighborData* nbr = iface->neighbor;
    if (nbr->neighborID == 0)
        nbr->neighborID = hdr.routerId;        // học Router ID từ header
    nbr->priority = hello.routerPriority;

    // Bước 5: HelloReceived → chuyển trạng thái
    if (nbr->state == NBR_DOWN)
        nbr->state = NBR_INIT;

    // Bước 6: Kiểm tra 2-way (có thấy Router ID của mình trong neighbor list?)
    bool seen = false;
    for (uint32_t nid : hello.neighborId) {
        if (nid == myRouterId) { seen = true; break; }
    }

    if (seen && nbr->state < NBR_TWOWAY)
        nbr->state = NBR_TWOWAY;
    else if (!seen && nbr->state >= NBR_TWOWAY)
        nbr->state = NBR_INIT;
}
