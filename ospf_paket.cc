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
    mod->send(msg, "gate", ifIndex);
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
