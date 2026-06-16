#include "ospf.h"
#include <sstream>
#include <iomanip>

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

void OspfRouterState::originateRouterLSA()
{
    // Tạo Router-LSA mới (Section 12.4.1, RFC 2328)
    LSA lsa;

    // LSA Header (Section 12.1 + A.4.1)
    lsa.header.age = 0;
    lsa.header.options = OPT_E;                     // ExternalRoutingCapability
    lsa.header.type = 1;                             // Router-LSA
    lsa.header.linkStateId = routerID;
    lsa.header.advertisingRouter = routerID;
    lsa.header.sequenceNumber = 0x80000001;          // InitialSequenceNumber (Section 12.1.6)
    lsa.header.checksum = 0;                         // dự án bỏ qua checksum

    // Router-LSA flags (A.4.2): V=0 (không virtual link), E=0 (không ASBR), B=0 (không ABR)
    lsa.flags = 0;
    lsa.zero = 0;
    lsa.numLinks = 0;
    lsa.links.clear();

    // [Loop] Xây links[] — mỗi interface PointToPoint → 1 Type 3 stub (Section 12.4.1)
    for (size_t i = 0; i < interfaces.size(); i++) {
        InterfaceData* iface = &interfaces[i];

        // Down → bỏ qua (Section 12.4.1: "If state is Down, no links added")
        if (iface->state == IF_DOWN)
            continue;

        // P2P: thêm Type 3 stub link (Section 12.4.1.1 — thêm luôn, bất kể neighbor state)
        // neighbor chưa Full → không thêm Type 1, chỉ thêm stub
        LSALink link;
        link.linkID = 0;                              // chưa biết neighbor IP ở Phase 0
        link.linkData = 0xFFFFFFFF;                    // host route (Section 12.4.1.1 Option 1)
        link.type = LINK_STUB;                         // Type 3 (stub network)
        link.numTOS = 0;
        link.metric = iface->cost;                     // interface's configured output cost
        link.Data.clear();

        lsa.links.push_back(link);
        lsa.numLinks++;
    }

    // Tính tổng length: 20 byte header + 4 byte (flags+zero+numLinks) + numLinks * 12 byte (A.4.2)
    lsa.header.length = 24 + lsa.numLinks * 12;

    // Thay thế LSA cũ trong LSDB hoặc thêm mới (Section 12.4)
    for (size_t i = 0; i < area.routerLSAs.size(); i++) {
        if (area.routerLSAs[i].header.advertisingRouter == routerID) {
            area.routerLSAs[i] = lsa;
            return;  // đã thay thế xong
        }
    }
    // Chưa có → thêm mới
    area.routerLSAs.push_back(lsa);
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
          << " deadInt=" << iface.routerDeadInterval
          << " infTransDelay=" << iface.infTransDelay
          << " rxmtInt=" << iface.rxmtInterval << "\n";
        f << "       ip=0x" << std::hex << iface.ipAddress << std::dec
          << " mask=0x" << std::hex << iface.mask << std::dec
          << " priority=" << (int)iface.routerPriority << "\n";

        if (iface.neighbor) {
            const NeighborData& nbr = *iface.neighbor;
            f << "       nbr: id=" << nbr.IDNeighbor
              << " state=" << nbrStateName(nbr.state)
              << " ip=0x" << std::hex << nbr.IPNeighbor << std::dec
              << " priority=" << (int)nbr.priorityNeighbor
              << " options=0x" << std::hex << (int)nbr.optionsNeighbor << std::dec
              << " master=" << nbr.isMaster
              << " ddSeq=0x" << std::hex << nbr.ddSequenceNumber << std::dec
              << " lastDdOpt=0x" << std::hex << (int)nbr.lastDdOptions << std::dec
              << " lastDdIMs=0x" << std::hex << (int)nbr.lastDdIMs << std::dec
              << " rxmtTimer=" << (nbr.rxmtTimer ? "YES" : "no")
              << " inactivityTimer=" << (nbr.inactivityTimer ? "YES" : "no") << "\n";
            f << "    databaseSummaryList (" << nbr.databaseSummaryList.size() << "):";
            for (const auto& lsa : nbr.databaseSummaryList)
                f << " type=" << (int)lsa.type
                  << " id=0x" << std::hex << lsa.linkStateId << std::dec;
            f << "\n";
            f << "    linkStateRequestList (" << nbr.linkStateRequestList.size() << "):";
            for (const auto& lsa : nbr.linkStateRequestList)
                f << " type=" << (int)lsa.type
                  << " id=0x" << std::hex << lsa.linkStateId << std::dec;
            f << "\n";
            f << "    linkStateRetransmissionList (" << nbr.linkStateRetransmissionList.size() << "):";
            for (const auto& lr : nbr.linkStateRetransmissionList)
                f << " type=" << (int)lr.LSType
                  << " id=0x" << std::hex << lr.linkStateId << std::dec;
            f << "\n";
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

void OspfRouterState::logTransition(const char* subphase, const char* event,
                                     double simtime, int ifIndex)
{
    namespace fs = std::filesystem;
    std::string dir = std::string("state_dump/") + subphase;
    fs::create_directories(dir);

    // Tên file: <seq>_r<id>_<ifIdx>.log — seq global tăng dần để sort đúng thứ tự
    static int globalSeq = 0;
    int seq = ++globalSeq;

    std::string filename = dir + "/" + std::to_string(seq)
                         + "_r" + std::to_string(routerID)
                         + "_" + std::to_string(ifIndex) + ".log";
    std::ofstream f(filename);
    if (!f.is_open()) return;

    f << "t=" << simtime << " — " << event << "\n";
    f << "Router ID: " << routerID << "  ifIndex=" << ifIndex << "\n\n";

    f << "--- Interfaces (" << interfaces.size() << ") ---\n";
    for (size_t i = 0; i < interfaces.size(); i++) {
        const InterfaceData& iface = interfaces[i];
        f << "  [" << i << "] type=" << iface.type
          << " state=" << ifStateName(iface.state)
          << " area=0x" << std::hex << iface.areaID << std::dec
          << " cost=" << iface.cost
          << " helloInt=" << iface.helloInterval
          << " deadInt=" << iface.routerDeadInterval
          << " rxmtInt=" << iface.rxmtInterval << "\n";
        f << "       ip=0x" << std::hex << iface.ipAddress << std::dec
          << " mask=0x" << std::hex << iface.mask << std::dec
          << " priority=" << (int)iface.routerPriority
          << " infTransDelay=" << iface.infTransDelay << "\n";
        if (iface.neighbor) {
            const NeighborData& nbr = *iface.neighbor;
            f << "       nbr: id=" << nbr.IDNeighbor
              << " state=" << nbrStateName(nbr.state)
              << " ip=0x" << std::hex << nbr.IPNeighbor << std::dec
              << " priority=" << (int)nbr.priorityNeighbor
              << " options=0x" << std::hex << (int)nbr.optionsNeighbor << std::dec
              << " master=" << nbr.isMaster
              << " ddSeq=0x" << std::hex << nbr.ddSequenceNumber << std::dec
              << " lastDdOpt=0x" << std::hex << (int)nbr.lastDdOptions << std::dec
              << " lastDdIMs=0x" << std::hex << (int)nbr.lastDdIMs << std::dec
              << " rxmtTimer=" << (nbr.rxmtTimer ? "YES" : "no")
              << " inactivityTimer=" << (nbr.inactivityTimer ? "YES" : "no") << "\n";
            f << "       databaseSummaryList (" << nbr.databaseSummaryList.size() << "):";
            for (const auto& lsa : nbr.databaseSummaryList)
                f << " type=" << (int)lsa.type
                  << " id=0x" << std::hex << lsa.linkStateId << std::dec;
            f << "\n";
            f << "       linkStateRequestList (" << nbr.linkStateRequestList.size() << "):";
            for (const auto& lsa : nbr.linkStateRequestList)
                f << " type=" << (int)lsa.type
                  << " id=0x" << std::hex << lsa.linkStateId << std::dec;
            f << "\n";
            f << "       linkStateRetransmissionList (" << nbr.linkStateRetransmissionList.size() << "):";
            for (const auto& lr : nbr.linkStateRetransmissionList)
                f << " type=" << (int)lr.LSType
                  << " id=0x" << std::hex << lr.linkStateId << std::dec;
            f << "\n";
        }
    }

    f << "--- LSDB: " << area.routerLSAs.size() << " Router-LSAs ---\n";
    f << "--- RoutingTable: " << RoutingTable.size() << " entries ---\n";

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

bool helloData::processHello(const headerOspf& hdr, const std::vector<uint8_t>& data,
                              InterfaceData* iface, int ifIndex, uint32_t myRouterId)
{
    if (data.size() < 20) return false;  // tối thiểu 20 byte body (không có neighbor)

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
    if (hello.helloInterval != iface->helloInterval)       return false;
    if (hello.routerDeadInterval != iface->routerDeadInterval) return false;

    // Kiểm tra E-bit: backbone (area 0) bắt buộc có E-bit
    bool eBit = (hello.options & OPT_E);
    if (iface->areaID == 0 && !eBit)            return false;

    // Cập nhật neighbor
    NeighborData* nbr = iface->neighbor;
    if (nbr->IDNeighbor == 0)
        nbr->IDNeighbor = hdr.routerId;        // học Router ID từ header
    nbr->priorityNeighbor = hello.routerPriority;

    // Chuyển trạng thái: Down → Init
    if (nbr->state == NBR_DOWN)
        nbr->state = NBR_INIT;

    // Kiểm tra 2-way (có thấy Router ID của mình trong neighbor list?)
    bool seen = false;
    for (uint32_t nid : hello.neighborId) {
        if (nid == myRouterId) { seen = true; break; }
    }

    if (seen && nbr->state < NBR_TWOWAY) {
        nbr->state = NBR_TWOWAY;
    }
    else if (!seen && nbr->state >= NBR_TWOWAY) {
        nbr->state = NBR_INIT;
    }

    return true;  // Hello hợp lệ
}


// ============================================================
// databaseDescriptionData::sendDD
// RFC 2328 Section 10.8 + A.3.3 — Database Description packet
// ============================================================

void databaseDescriptionData::sendDD(int ifIndex, OspfRouterState& state,
                                     uint32_t routerId,
                                     omnetpp::cSimpleModule* mod)
{
    InterfaceData* iface = &state.interfaces[ifIndex];
    NeighborData* nbr = iface->neighbor;

    // Xác định flags (I, M, MS) dựa vào state (Section 10.8)
    uint8_t flags = 0;
    if (nbr->state == NBR_EXSTART) {
        flags = DD_I | DD_M | DD_MS;
    } else if (nbr->state == NBR_EXCHANGE) {
        flags = 0;                      // I=0
        if (nbr->isMaster) flags |= DD_MS;
    }

    // Tính body: 8 byte DD header + các LSA headers
    int lsaCount = (int)nbr->databaseSummaryList.size();
    int bodyLen = 8 + lsaCount * 20;    // mỗi LSA header 20 byte
    std::vector<uint8_t> body(bodyLen);
    int off = 0;
    put16(body.data(), off, 0);                   // InterfaceMTU (P2P: 0)
    put8(body.data(), off, OPT_E);                 // Options (E-bit)
    put8(body.data(), off, flags);                 // Flags (I, M, MS)
    put32(body.data(), off, nbr->ddSequenceNumber);// DD Sequence Number

    // Exchange state: gửi kèm LSA headers (Section 10.8)
    if (nbr->state == NBR_EXCHANGE) {
        for (const auto& h : nbr->databaseSummaryList) {
            put16(body.data(), off, h.age);
            put8(body.data(), off, h.options);
            put8(body.data(), off, h.type);
            put32(body.data(), off, h.linkStateId);
            put32(body.data(), off, h.advertisingRouter);
            put32(body.data(), off, (uint32_t)h.sequenceNumber);
            put16(body.data(), off, h.checksum);
            put16(body.data(), off, h.length);
        }
    }

    OspfMess::send(2, body, routerId, iface->areaID, ifIndex, mod);

    // Exchange: đã gửi headers → xóa khỏi databaseSummaryList (coi như sent)
    if (nbr->state == NBR_EXCHANGE)
        nbr->databaseSummaryList.clear();
}


// ============================================================
// databaseDescriptionData::processDD
// RFC 2328 Section 10.6 — Receiving Database Description packets
// ============================================================

DdResult databaseDescriptionData::processDD(const headerOspf& hdr,
                                             const std::vector<uint8_t>& data,
                                             InterfaceData* iface,
                                             uint32_t myRouterId,
                                             const std::vector<LSA>& routerLSAs)
{
    DdResult r = {false, false, false, false, false};
    if (data.size() < 8) return r;

    // Parse DD body (RFC A.3.3 + 10.6)
    int off = 0;
    uint16_t interfaceMTU = get16(data.data(), off);
    uint8_t options       = get8(data.data(), off);
    uint8_t flags         = get8(data.data(), off);
    uint32_t ddSeq        = get32(data.data(), off);

    // Parse LSA headers từ body
    std::vector<LSAHeader> recvHeaders;
    int remain = (int)data.size() - 8;
    if (remain > 0 && remain % 20 == 0) {
        int nHeaders = remain / 20;
        for (int i = 0; i < nHeaders; i++) {
            LSAHeader lh;
            lh.age              = get16(data.data(), off);
            lh.options          = get8(data.data(), off);
            lh.type             = get8(data.data(), off);
            lh.linkStateId      = get32(data.data(), off);
            lh.advertisingRouter = get32(data.data(), off);
            lh.sequenceNumber   = (int32_t)get32(data.data(), off);
            lh.checksum         = get16(data.data(), off);
            lh.length           = get16(data.data(), off);
            recvHeaders.push_back(lh);
        }
    }

    NeighborData* nbr = iface->neighbor;
    bool isExStartFallthrough = false;  // đánh dấu fall-through từ ExStart→Exchange

    // ============================================================
    // EXSTART — Master/Slave negotiation (Section 10.6)
    // ============================================================
    if (nbr->state == NBR_EXSTART) {
        // Case 1: Slave — I+M+MS=1, body rỗng, RouterID neighbor > self
        if ((flags & (DD_I | DD_M | DD_MS)) == (DD_I | DD_M | DD_MS)
            && data.size() == 8
            && hdr.routerId > myRouterId)
        {
            nbr->isMaster = 0;
            nbr->ddSequenceNumber = ddSeq;
            nbr->optionsNeighbor = options;
            nbr->lastDdOptions = options;
            nbr->lastDdIMs = flags;
            r.negotiationDone = true;
            r.neighborStateChanged = true;
            r.shouldSendDD = true;          // slave reply (empty, sẽ gửi Exchange DD sau)
        }
        // Case 2: Master — I=0, MS=0, seq khớp, RouterID neighbor < self
        // Packets này có thể chứa LSA headers từ slave → cần fall-through
        else if (!(flags & DD_I) && !(flags & DD_MS)
                 && ddSeq == nbr->ddSequenceNumber
                 && hdr.routerId < myRouterId)
        {
            nbr->optionsNeighbor = options;
            nbr->lastDdOptions = options;
            nbr->lastDdIMs = flags;
            r.negotiationDone = true;
            r.neighborStateChanged = true;
            r.shouldSendDD = false;
            isExStartFallthrough = true;    // tiếp tục xử lý LSA headers bên dưới
        }
        else {
            r.valid = true;                 // ignore
            return r;
        }
    }

    // ============================================================
    // EXCHANGE / ExStart-fallthrough — xử lý gói DD (Section 10.6)
    // ============================================================
    if (nbr->state == NBR_EXCHANGE || isExStartFallthrough) {
        // --- Check DUPLICATE (chỉ khi state == Exchange) ---
        if (!isExStartFallthrough) {
            if (flags == nbr->lastDdIMs && options == nbr->lastDdOptions) {
                // Cần xác nhận bằng ddSeq để tránh false positive
                // (ExStart response có thể có flags giống Exchange DD)
                bool isDup = false;
                if (nbr->isMaster) {
                    // Master: duplicate nếu ddSeq < current (slave echo cũ)
                    isDup = (ddSeq < nbr->ddSequenceNumber);
                } else {
                    // Slave: duplicate nếu ddSeq == current (master retransmit)
                    isDup = (ddSeq == nbr->ddSequenceNumber);
                }
                if (isDup) {
                    if (nbr->isMaster) {
                        r.valid = true;         // master: discard duplicate
                    } else {
                        r.shouldSendDD = true;  // slave: resend last DD
                        r.valid = true;
                    }
                    return r;
                }
            }

            // --- VALIDATION (Section 10.6) ---
            bool msInconsistent = (nbr->isMaster && (flags & DD_MS))
                               || (!nbr->isMaster && !(flags & DD_MS));
            if (msInconsistent || (flags & DD_I) || options != nbr->optionsNeighbor) {
                r.valid = false;            // SeqNumberMismatch
                r.neighborStateChanged = true;
                return r;
            }

            // --- SEQUENCE CHECK (Section 10.6) ---
            bool seqOk = nbr->isMaster
                ? (ddSeq == nbr->ddSequenceNumber)          // master: slave echoes
                : (ddSeq == nbr->ddSequenceNumber + 1);     // slave: master increments
            if (!seqOk) {
                r.valid = false;
                r.neighborStateChanged = true;
                return r;
            }
        }

        // ============================================================
        // ACCEPT — gói hợp lệ
        // ============================================================
        // Ghi nhận gói cuối
        nbr->lastDdOptions = options;
        nbr->lastDdIMs = flags;
        r.valid = true;

        // Xử lý từng LSA header: so sánh với LSDB (Section 10.6)
        for (const auto& lh : recvHeaders) {
            if (lh.type < 1 || lh.type > 5) {   // LS type không hợp lệ
                r.valid = false;
                r.neighborStateChanged = true;
                return r;
            }
            // Tìm trong LSDB
            bool needRequest = true;
            for (const auto& lsa : routerLSAs) {
                if (lsa.header.type == lh.type
                    && lsa.header.linkStateId == lh.linkStateId
                    && lsa.header.advertisingRouter == lh.advertisingRouter)
                {
                    if (lsa.header.sequenceNumber >= lh.sequenceNumber)
                        needRequest = false;   // bản local mới hơn hoặc bằng
                    break;
                }
            }
            // Thêm vào linkStateRequestList nếu cần (tránh trùng)
            if (needRequest) {
                bool already = false;
                for (const auto& req : nbr->linkStateRequestList) {
                    if (req.type == lh.type
                        && req.linkStateId == lh.linkStateId
                        && req.advertisingRouter == lh.advertisingRouter) {
                        already = true; break;
                    }
                }
                if (!already)
                    nbr->linkStateRequestList.push_back(lh);
            }
        }

        // === Master/Slave actions (Section 10.6) ===
        if (nbr->isMaster) {
            if (isExStartFallthrough) {
                // Fall-through từ ExStart case 2: master vừa nhận slave's
                // ExStart response → accept + increment ddSeq (RFC 10.6)
                // Master chưa gửi Exchange DD nào → gửi poll đầu tiên.
                nbr->ddSequenceNumber++;
                r.shouldSendDD = true;
            } else {
                // Normal Exchange: increment ddSeq, check ExchangeDone
                nbr->ddSequenceNumber++;
                if (nbr->databaseSummaryList.empty() && !(flags & DD_M)) {
                    r.exchangeDone = true;
                    r.neighborStateChanged = true;
                } else {
                    r.shouldSendDD = true;
                }
            }
        } else {
            // Slave: set ddSeq + response
            nbr->ddSequenceNumber = ddSeq;
            r.shouldSendDD = true;
            // ExchangeDone: databaseSummaryList empty AND M-bit=0
            if (nbr->databaseSummaryList.empty() && !(flags & DD_M)) {
                r.exchangeDone = true;
                r.neighborStateChanged = true;
            }
        }
        return r;
    }

    // ============================================================
    // LOADING / FULL — chỉ duplicate (Section 10.6)
    // ============================================================
    if (nbr->state == NBR_LOADING || nbr->state == NBR_FULL) {
        if (options != nbr->optionsNeighbor || (flags & DD_I)) {
            r.valid = false;
            r.neighborStateChanged = true;
            return r;
        }
        if (flags == nbr->lastDdIMs && options == nbr->lastDdOptions) {
            // Xác nhận bằng ddSeq: master nếu seq < current, slave nếu seq == current
            bool isDup = nbr->isMaster
                ? (ddSeq < nbr->ddSequenceNumber)
                : (ddSeq == nbr->ddSequenceNumber);
            if (isDup) {
                if (nbr->isMaster) {
                    r.valid = true;         // master discard
                } else {
                    r.shouldSendDD = true;  // slave resend last DD
                    r.valid = true;
                }
            } else {
                r.valid = false;            // không duplicate → SeqNumberMismatch
                r.neighborStateChanged = true;
            }
        } else {
            r.valid = false;            // SeqNumberMismatch
            r.neighborStateChanged = true;
        }
        return r;
    }

    // Down/Init/2Way — ignore
    r.valid = true;
    return r;
}


// ============================================================
// Helper: so sánh 2 LSA instance — trả về true nếu a mới hơn b
// RFC 2328 Section 13.1 (Determining which LSA is newer)
// ============================================================
static bool isNewerLSA(const LSAHeader& a, const LSAHeader& b) {
    if (a.sequenceNumber != b.sequenceNumber)
        return a.sequenceNumber > b.sequenceNumber;
    if (a.checksum != b.checksum)
        return (uint16_t)a.checksum > (uint16_t)b.checksum;
    // Cùng seq + checksum: MaxAge ⇒ cái kia mới hơn
    if (a.age == 0xFFFF && b.age != 0xFFFF) return false;
    if (b.age == 0xFFFF && a.age != 0xFFFF) return true;
    return false;   // same instance
}


// ============================================================
// linkStateRequestData::sendLSR
// RFC 2328 Section 10.9 + A.3.4 — Link State Request packet
// ============================================================

void linkStateRequestData::sendLSR(int ifIndex, OspfRouterState& state,
                                    uint32_t routerId,
                                    omnetpp::cSimpleModule* mod)
{
    InterfaceData* iface = &state.interfaces[ifIndex];
    NeighborData* nbr = iface->neighbor;

    // Lấy các request từ đầu linkStateRequestList
    // ⚠ KHÔNG xóa khỏi list — giữ lại để retransmit (Section 10.9)
    int nReq = (int)nbr->linkStateRequestList.size();
    if (nReq == 0) return;  // không có gì để request

    // Xây LSR body: mỗi request 12 byte (LS type, Link State ID, Advertising Router)
    int bodyLen = nReq * 12;
    std::vector<uint8_t> body(bodyLen);
    int off = 0;
    for (const auto& lh : nbr->linkStateRequestList) {
        put32(body.data(), off, lh.type);           // LS type
        put32(body.data(), off, lh.linkStateId);    // Link State ID
        put32(body.data(), off, lh.advertisingRouter); // Advertising Router
    }

    OspfMess::send(3, body, routerId, iface->areaID, ifIndex, mod);
}


// ============================================================
// linkStateRequestData::processLSR
// RFC 2328 Section 10.7 — Receiving Link State Request packets
// ============================================================

LsrResult linkStateRequestData::processLSR(const headerOspf& hdr,
                                            const std::vector<uint8_t>& data,
                                            InterfaceData* iface,
                                            const std::vector<LSA>& routerLSAs)
{
    LsrResult r = {false, false, {}};

    // Parse LSR body (Section A.3.4)
    int remain = (int)data.size();
    if (remain == 0 || remain % 12 != 0) return r;  // invalid

    int nReq = remain / 12;

    // Duyệt từng request, tra LSDB (Section 10.7)
    for (int i = 0; i < nReq; i++) {
        int off = i * 12;
        uint32_t lsType      = get32(data.data(), off);
        uint32_t linkStateId = get32(data.data(), off);
        uint32_t advRouter   = get32(data.data(), off);

        if (lsType != 1) {  // Dự án chỉ có Router-LSA (type=1)
            r.badLSReq = true;
            return r;
        }

        // Tra LSDB: tìm LSA có (type, linkStateId, advertisingRouter)
        bool found = false;
        for (const auto& lsa : routerLSAs) {
            if (lsa.header.type == lsType
                && lsa.header.linkStateId == linkStateId
                && lsa.header.advertisingRouter == advRouter)
            {
                r.lsus.push_back(lsa);   // copy vào LSU body
                found = true;
                break;
            }
        }

        if (!found) {
            r.badLSReq = true;   // Section 10.7: "If an LSA cannot be found → BadLSReq"
            return r;
        }
    }

    // ⚠ Section 10.7: "These LSAs should NOT be placed on the Link state
    //    retransmission list" → không thêm vào linkStateRetransmissionList
    r.valid = true;
    return r;
}


// ============================================================
// linkStateUpdateData::sendLSU
// RFC 2328 Section 13 + A.3.5 — Link State Update packet
// ============================================================

void linkStateUpdateData::sendLSU(int ifIndex, const std::vector<LSA>& lsas,
                                   uint32_t routerId, uint32_t areaId,
                                   omnetpp::cSimpleModule* mod)
{
    if (lsas.empty()) return;

    // Tính tổng body size: 4 byte (numberOfLSA) + các LSA (header 20 + links)
    int bodyLen = 4;
    for (const auto& lsa : lsas) {
        // LSA body = 20 byte header + 4 byte flags/zero/numLinks + links
        int lsaBodyLen = 24;  // header(20) + flags(1) + zero(1) + numLinks(2)
        for (const auto& link : lsa.links) {
            lsaBodyLen += 12;  // linkID(4) + linkData(4) + type(1) + numTOS(1) + metric(2)
            lsaBodyLen += (int)link.Data.size() * 4;  // mỗi TOS: tos(1) + zero(1) + metric(2)
        }
        bodyLen += lsaBodyLen;
    }

    std::vector<uint8_t> body(bodyLen);
    int off = 0;
    put32(body.data(), off, (uint32_t)lsas.size());  // numberOfLSA

    for (const auto& lsa : lsas) {
        // LSA header (20 byte)
        put16(body.data(), off, lsa.header.age);
        put8(body.data(), off, lsa.header.options);
        put8(body.data(), off, lsa.header.type);
        put32(body.data(), off, lsa.header.linkStateId);
        put32(body.data(), off, lsa.header.advertisingRouter);
        put32(body.data(), off, (uint32_t)lsa.header.sequenceNumber);
        put16(body.data(), off, lsa.header.checksum);
        put16(body.data(), off, lsa.header.length);
        // LSA body — Router-LSA: flags(1) + zero(1) + numLinks(2) + links
        put8(body.data(), off, lsa.flags);
        put8(body.data(), off, lsa.zero);
        put16(body.data(), off, lsa.numLinks);
        for (const auto& link : lsa.links) {
            put32(body.data(), off, link.linkID);
            put32(body.data(), off, link.linkData);
            put8(body.data(), off, link.type);
            put8(body.data(), off, link.numTOS);
            put16(body.data(), off, link.metric);
            for (const auto& tos : link.Data) {
                put8(body.data(), off, tos.TOSid);
                put8(body.data(), off, tos.zero);
                put16(body.data(), off, tos.metric);
            }
        }
    }

    OspfMess::send(4, body, routerId, areaId, ifIndex, mod);
}


// ============================================================
// linkStateUpdateData::processLSU
// RFC 2328 Section 13 (Flooding Procedure) steps 1-8
// ============================================================

LsuResult linkStateUpdateData::processLSU(const headerOspf& hdr,
                                           const std::vector<uint8_t>& data,
                                           InterfaceData* iface,
                                           AreaData& area,
                                           std::vector<LSAHeader>& requestList)
{
    LsuResult r = {false, false, false, false, {}};
    if (data.size() < 4) return r;

    // Parse LSU body (Section A.3.5)
    int off = 0;
    uint32_t numberOfLSAs = get32(data.data(), off);

    // Parse từng LSA
    std::vector<LSA> lsas;
    for (uint32_t i = 0; i < numberOfLSAs; i++) {
        LSA lsa;
        if ((int)data.size() - off < 20) { r.valid = false; return r; }
        // LSA header (20 byte)
        lsa.header.age              = get16(data.data(), off);
        lsa.header.options          = get8(data.data(), off);
        lsa.header.type             = get8(data.data(), off);
        lsa.header.linkStateId      = get32(data.data(), off);
        lsa.header.advertisingRouter = get32(data.data(), off);
        lsa.header.sequenceNumber   = (int32_t)get32(data.data(), off);
        lsa.header.checksum         = get16(data.data(), off);
        lsa.header.length           = get16(data.data(), off);

        // Router-LSA body (24 byte header + links)
        if (lsa.header.type == 1) {  // Router-LSA
            if ((int)data.size() - off < 4) { r.valid = false; return r; }
            lsa.flags    = get8(data.data(), off);
            lsa.zero     = get8(data.data(), off);
            lsa.numLinks = get16(data.data(), off);
            for (uint16_t j = 0; j < lsa.numLinks; j++) {
                LSALink link;
                if ((int)data.size() - off < 12) { r.valid = false; return r; }
                link.linkID   = get32(data.data(), off);
                link.linkData = get32(data.data(), off);
                link.type     = get8(data.data(), off);
                link.numTOS   = get8(data.data(), off);
                link.metric   = get16(data.data(), off);
                for (uint8_t t = 0; t < link.numTOS; t++) {
                    TOSData tos;
                    if ((int)data.size() - off < 4) { r.valid = false; return r; }
                    tos.TOSid = get8(data.data(), off);
                    tos.zero  = get8(data.data(), off);
                    tos.metric = get16(data.data(), off);
                    link.Data.push_back(tos);
                }
                lsa.links.push_back(link);
            }
        }
        // Các type khác (dự án bỏ qua) — chỉ parse length byte
        else {
            if (lsa.header.length < 20) { r.valid = false; return r; }
            int skip = lsa.header.length - 20;
            if ((int)data.size() - off < skip) { r.valid = false; return r; }
            off += skip;
        }
        lsas.push_back(lsa);
    }

    // ============================================================
    // Section 13: xử lý từng LSA
    // ============================================================
    for (auto& recvLsa : lsas) {
        // (1) Validate checksum — dự án bỏ qua
        // (2) LS type unknown?
        if (recvLsa.header.type < 1 || recvLsa.header.type > 5) continue;
        // (3) AS-external trong stub? — backbone non-stub → pass
        // (4) MaxAge + no DB copy + no neighbor Exchange/Loading?
        //     LSA mới, age=0 → không rơi vào case này

        // Tra LSDB: tìm bản copy hiện tại
        bool dbHasIt = false;
        for (auto& dbLsa : area.routerLSAs) {
            if (dbLsa.header.type == recvLsa.header.type
                && dbLsa.header.linkStateId == recvLsa.header.linkStateId
                && dbLsa.header.advertisingRouter == recvLsa.header.advertisingRouter)
            {
                dbHasIt = true;

                // (5) received newer?
                if (isNewerLSA(recvLsa.header, dbLsa.header)) {
                    // (5a) MinLSArrival — bỏ qua
                    // (5b) Flood — placeholder (1c)
                    // (5c) Xóa DB copy cũ khỏi retransmission lists
                    // (5d) Install: replace DB copy
                    dbLsa = recvLsa;
                    r.scheduleSPF = true;
                    // (5e) ACK
                    r.ackHeaders.push_back(recvLsa.header);
                    // Loading specific: xóa khỏi linkStateRequestList nếu có
                    for (auto it = requestList.begin(); it != requestList.end(); ) {
                        if (it->type == recvLsa.header.type
                            && it->linkStateId == recvLsa.header.linkStateId
                            && it->advertisingRouter == recvLsa.header.advertisingRouter)
                        {
                            it = requestList.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
                // (6) received không mới hơn + trong request list của sending neighbor?
                //     → BadLSReq. Trong 1b2 Loading: neighbor không có LSA của ta
                //     trong request list của nó. Case này chủ yếu cho Flooding (1c).
                else if (false) {  // không xảy ra trong 1b2 — để dành cho 1c
                    r.badLSReq = true;
                    r.valid = true;
                    return r;
                }
                // (7) Same instance (duplicate)?
                else if (!isNewerLSA(recvLsa.header, dbLsa.header)
                         && !isNewerLSA(dbLsa.header, recvLsa.header))
                {
                    // (7a) Implicit ACK nếu trong retransmission list
                    // (7b) ACK
                    r.ackHeaders.push_back(recvLsa.header);
                }
                // (8) DB copy mới hơn
                else {
                    // Gửi DB copy về neighbor — 1c sẽ xử lý
                }

                dbHasIt = true;
                break;
            }
        }

        // Không có DB copy (LSA mới)
        if (!dbHasIt) {
            // (5) Install new LSA into LSDB
            area.routerLSAs.push_back(recvLsa);
            r.scheduleSPF = true;
            // (5e) ACK
            r.ackHeaders.push_back(recvLsa.header);
            // Loading specific: xóa khỏi request list
            for (auto it = requestList.begin(); it != requestList.end(); ) {
                if (it->type == recvLsa.header.type
                    && it->linkStateId == recvLsa.header.linkStateId
                    && it->advertisingRouter == recvLsa.header.advertisingRouter)
                {
                    it = requestList.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    // LoadingDone: request list rỗng (Section 10.9 + 10.3)
    r.loadingDone = requestList.empty();
    r.valid = true;
    return r;
}


// ============================================================
// linkStateAcknowledgementData::sendLSAck
// RFC 2328 Section 13.5 + A.3.6 — Link State Acknowledgment packet
// ============================================================

void linkStateAcknowledgementData::sendLSAck(int ifIndex,
                                              const std::vector<LSAHeader>& headers,
                                              uint32_t routerId, uint32_t areaId,
                                              omnetpp::cSimpleModule* mod)
{
    if (headers.empty()) return;

    // Body = list of LSA headers (mỗi header 20 byte)
    int bodyLen = (int)headers.size() * 20;
    std::vector<uint8_t> body(bodyLen);
    int off = 0;
    for (const auto& h : headers) {
        put16(body.data(), off, h.age);
        put8(body.data(), off, h.options);
        put8(body.data(), off, h.type);
        put32(body.data(), off, h.linkStateId);
        put32(body.data(), off, h.advertisingRouter);
        put32(body.data(), off, (uint32_t)h.sequenceNumber);
        put16(body.data(), off, h.checksum);
        put16(body.data(), off, h.length);
    }

    OspfMess::send(5, body, routerId, areaId, ifIndex, mod);
}