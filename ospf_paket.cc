#include "ospf.h"

void helloData::sendHello(int ifIndex, OspfRouterState& state, uint32_t routerId, omnetpp::cSimpleModule* mod)
{
    InterfaceData* iface = &state.interfaces[ifIndex];
    NeighborData* nbr = iface->neighbor;

    OspfMess* msg = new OspfMess("OSPF-Hello");

    // Header
    msg->version = 2;
    msg->type = 1;                      // 1=Hello
    msg->routerId = routerId;
    msg->areaId = iface->areaID;
    msg->authType = 0;
    msg->authData1 = 0;
    msg->authData2 = 0;

    // Encode hello body vào payload (big-endian)
    int nNbr = (nbr->neighborID != 0) ? 1 : 0;
    int bodyLen = 20 + nNbr * 4;
    std::vector<uint8_t> buf(bodyLen);
    int off = 0;
    OspfMess::put32(buf.data(), off, iface->mask);               // networkMask
    OspfMess::put16(buf.data(), off, iface->helloInterval);      // helloInterval
    OspfMess::put8(buf.data(), off, 0x02);                       // options (E-bit)
    OspfMess::put8(buf.data(), off, iface->routerPriority);      // routerPriority
    OspfMess::put32(buf.data(), off, iface->routerDeadInterval); // routerDeadInterval
    OspfMess::put32(buf.data(), off, 0);                         // designatedRouter (P2P=0)
    OspfMess::put32(buf.data(), off, 0);                         // backupDesignatedRouter (P2P=0)
    if (nNbr)
        OspfMess::put32(buf.data(), off, nbr->neighborID);       // neighborId[0]

    msg->payload.resize(bodyLen);
    for (int i = 0; i < bodyLen; i++)
        msg->payload[i] = buf[i];

    msg->length = 24 + bodyLen;               // tổng bytes: header 24 + body
    mod->send(msg, "gate$o", ifIndex);
}

// ===== OspfMess static methods =====

void OspfMess::put32(uint8_t* buf, int& off, uint32_t v) {
    buf[off++] = (v >> 24) & 0xFF;
    buf[off++] = (v >> 16) & 0xFF;
    buf[off++] = (v >> 8) & 0xFF;
    buf[off++] = v & 0xFF;
}
void OspfMess::put16(uint8_t* buf, int& off, uint16_t v) {
    buf[off++] = (v >> 8) & 0xFF;
    buf[off++] = v & 0xFF;
}
void OspfMess::put8(uint8_t* buf, int& off, uint8_t v) {
    buf[off++] = v;
}

// Tách msg thành header (24 byte) + data thô (payload),
// trả về type (1=Hello,2=DD,3=LSR,4=LSU,5=LSAck) nếu hợp lệ, 0 nếu fail
uint8_t OspfMess::parsePacket(const OspfMess* msg, const InterfaceData* iface,
                               headerOspf& hdr, std::vector<uint8_t>& data)
{
    if (msg->version != 2)
        return 0;

    if (msg->areaId != iface->areaID)
        return 0;

    if (msg->authType != 0)
        return 0;

    // Tách header
    hdr.version   = msg->version;
    hdr.type      = msg->type;
    hdr.length    = msg->length;
    hdr.routerId  = msg->routerId;
    hdr.areaId    = msg->areaId;
    hdr.checksum  = msg->checksum;
    hdr.authType  = msg->authType;
    hdr.authData1 = msg->authData1;
    hdr.authData2 = msg->authData2;

    // Data thô chưa xử lý
    data = msg->payload;

    return msg->type;
}

// Đọc big-endian từ buffer
uint32_t OspfMess::get32(const uint8_t* buf, int& off) {
    uint32_t v = ((uint32_t)buf[off] << 24) | ((uint32_t)buf[off+1] << 16)
               | ((uint32_t)buf[off+2] << 8) | buf[off+3];
    off += 4;
    return v;
}
uint16_t OspfMess::get16(const uint8_t* buf, int& off) {
    uint16_t v = ((uint16_t)buf[off] << 8) | buf[off+1];
    off += 2;
    return v;
}
uint8_t OspfMess::get8(const uint8_t* buf, int& off) {
    return buf[off++];
}

// Xử lý gói Hello nhận được (RFC 2328 Section 10.5, P2P)
void helloData::processHello(const headerOspf& hdr, const std::vector<uint8_t>& data,
                              InterfaceData* iface, uint32_t myRouterId)
{
    if (data.size() < 20) return;  // tối thiểu 20 byte (không neighbor)

    // Bước 2: parse Hello body vào helloData (RFC A.3.2)
    helloData hello;
    int off = 0;
    hello.networkMask         = OspfMess::get32(data.data(), off);
    hello.helloInterval       = OspfMess::get16(data.data(), off);
    hello.options             = OspfMess::get8(data.data(), off);
    hello.routerPriority      = OspfMess::get8(data.data(), off);
    hello.routerDeadInterval  = OspfMess::get32(data.data(), off);
    hello.designatedRouter    = OspfMess::get32(data.data(), off);
    hello.backupDesignatedRouter = OspfMess::get32(data.data(), off);

    // Danh sách neighbor (nếu có)
    int nbrCount = ((int)data.size() - 20) / 4;
    for (int i = 0; i < nbrCount; i++)
        hello.neighborId.push_back(OspfMess::get32(data.data(), off));

    // Kiểm tra tham số (P2P: bỏ qua NetworkMask)
    if (hello.helloInterval != iface->helloInterval)       return;
    if (hello.routerDeadInterval != iface->routerDeadInterval) return;

    // Bước 3: kiểm tra E-bit
    bool eBit = (hello.options & 0x02);
    if (iface->areaID == 0 && !eBit)            return;  // backbone phải có E-bit

    // Cập nhật dữ liệu từ hello vào neighbor
    NeighborData* nbr = iface->neighbor;
    if (nbr->neighborID == 0)
        nbr->neighborID = hdr.routerId;
    nbr->priority = hello.routerPriority;

    // Bước 5: HelloReceived
    if (nbr->state == NBR_DOWN)
        nbr->state = NBR_INIT;

    // Bước 6: kiểm tra neighbor list — có RouterID mình không?
    bool seen = false;
    for (uint32_t nid : hello.neighborId) {
        if (nid == myRouterId) { seen = true; break; }
    }

    if (seen && nbr->state < NBR_TWOWAY)
        nbr->state = NBR_TWOWAY;
    else if (!seen && nbr->state >= NBR_TWOWAY)
        nbr->state = NBR_INIT;
}
