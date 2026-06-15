#ifndef __OSPF_STRUCT_H
#define __OSPF_STRUCT_H

#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <filesystem>
#include <omnetpp.h>
#include "ospf_m.h"

using namespace omnetpp;

class OspfRouterState;

//////////////////////////////////////////////////////////////////
// các cấu trúc dùng chung
//////////////////////////////////////////////////////////////////

//cấu trúc mess

struct headerOspf {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t routerId;
    uint32_t areaId;
    uint16_t checksum;
    uint16_t authType;
    uint32_t authData1;
    uint32_t authData2;
};

struct LSAHeader
{
    uint16_t age;
    uint8_t options;
    uint8_t type;
    uint32_t linkStateId;
    uint32_t advertisingRouter;
    uint32_t sequenceNumber;
    uint16_t checksum;
    uint16_t length;
};

struct LSARequest
{
    uint32_t LSType;
    uint32_t linkStateId;
    uint32_t advertisingRouter;
};

struct TOSData
{
    uint8_t type;
    uint8_t TOS;
    uint16_t metric;
};

struct LSALink
{
    uint32_t linkID;    
    uint32_t linkData;
    std::vector<TOSData> Data; // danh sách TOS entries, nếu numTOS > 0
};

struct LSA
{
    LSAHeader header;
    uint8_t flags;
    uint8_t zero;
    uint16_t numLinks;
    std::vector<LSALink> links; // nội dung LSA, định dạng tùy theo type
};

//cấu trúc trạng thái router

enum OspfNeighborState : uint8_t {
    NBR_DOWN     = 0,
    NBR_ATTEMPT  = 1,
    NBR_INIT     = 2,
    NBR_TWOWAY   = 3,
    NBR_EXSTART  = 4,
    NBR_EXCHANGE = 5,
    NBR_LOADING  = 6,
    NBR_FULL     = 7
};

enum OspfInterfaceState : uint8_t {
    IF_DOWN         = 0,
    IF_LOOPBACK     = 1,
    IF_WAITING      = 2,
    IF_POINTTOPOINT = 3,
    IF_DROTHER      = 4,
    IF_BACKUP       = 5,
    IF_DR           = 6
};

enum OspfRouterLinkType : uint8_t {
    LINK_P2P     = 1,
    LINK_TRANSIT = 2,
    LINK_STUB    = 3,
    LINK_VIRTUAL = 4
};

enum OspfPathType : uint8_t {
    PATH_INTRA_AREA = 1,
    PATH_INTER_AREA = 2,
    PATH_TYPE1_EXT  = 3,
    PATH_TYPE2_EXT  = 4
};

struct InterfaceData {
    unsigned int type;           // loại kết nối (P2P, broadcast...)
    unsigned int state;          // trạng thái máy trạng thái interface
    uint32_t ipAddress;          // địa chỉ IP của interface
    uint32_t mask;               // subnet mask
    uint32_t areaID;             // area chứa interface
    uint16_t helloInterval;      // chu kỳ gửi Hello
    uint32_t routerDeadInterval; // thời gian chờ neighbor chết
    unsigned int infTransDelay;  // độ trễ truyền ước tính (giây)
    uint8_t routerPriority;      // độ ưu tiên bầu DR
    uint16_t cost;               // chi phí gửi gói qua interface
    unsigned int rxmtInterval;   // chu kỳ retransmit LSA (giây)
    struct NeighborData* neighbor; // neighbor P2P tương ứng
};

struct NeighborData {
    uint32_t neighborID;         // Router ID của neighbor
    unsigned int state;          // trạng thái máy trạng thái neighbor
    cMessage* inactivityTimer;   // timer theo dõi thời gian sống
    unsigned int isMaster;       // vai trò master/slave trong DD exchange
    uint32_t ddSequenceNumber;   // số thứ tự DD đang trao đổi
    uint8_t lastDdOptions;       // Options từ DD packet cuối
    unsigned int lastDdIMs;      // I+M+MS bits từ DD packet cuối
    uint8_t priority;            // Router Priority của neighbor
};

struct AreaData {
    uint32_t areaID;             // định danh area
    unsigned int transitCapability; // có virtual link không
    unsigned int externalRoutingCapability; // stub flag
    unsigned int stubDefaultCost; // default cost nếu là stub
    std::vector<int> interfaceIndices; // danh sách interface trong area
    std::vector<LSA> routerLSAs; // LSDB chứa Router-LSA
    struct SpfResult {
        uint32_t destination;    // đích đến (Router ID)
        uint16_t cost;           // tổng chi phí đến đích
        std::vector<int> predecessors; // danh sách node trước trên đường đi
    };
    std::vector<SpfResult> spfTree; // cây SPF từ Dijkstra
};

struct RoutingTableEntry {
    uint32_t destinationId;      // đích đến (IP / Router ID)
    uint32_t addressMask;        // subnet mask
    uint8_t pathType;            // loại đường đi
    uint32_t cost;               // chi phí đường đi
    unsigned int nextHopInterfaceIndex; // interface đi tiếp
    uint32_t nextHopRouterID;    // Router ID hop tiếp theo
};


//////////////////////////////////////////////////////////////////
// phương thức tin nhắn
//////////////////////////////////////////////////////////////////

class helloData
{
    public:
        uint32_t networkMask;
        uint16_t helloInterval;
        uint8_t options;
        uint8_t routerPriority;
        uint32_t routerDeadInterval;
        uint32_t designatedRouter;
        uint32_t backupDesignatedRouter;
        std::vector<uint32_t> neighborId;

        static void sendHello(int ifIndex, OspfRouterState& state, uint32_t routerId, omnetpp::cSimpleModule* mod);

        // Xử lý gói Hello nhận được (RFC 2328 Section 10.5)
        static void processHello(const headerOspf& hdr, const std::vector<uint8_t>& data,
                                 InterfaceData* iface, uint32_t myRouterId);
};
class databaseDescriptionData
{
    public:
        uint16_t interfaceMTU;
        uint8_t options;
        uint8_t flags;
        uint32_t ddSequenceNumber;
        std::vector<LSAHeader> lsaHeaders;
};
class linkStateRequestData
{
    public:
        std::vector<LSARequest> requests;
};

class linkStateUpdateData
{
    public:
        uint32_t numberOfLSA;
        std::vector<LSA> LSAs;
};


//type 5 
class linkStateAcknowledgementData
{
    public:
        std::vector<LSAHeader> data;
};

//header OSPF packet

class OspfMess : public omnetpp::cMessage
{
    public:
        uint8_t version;
        uint8_t type;
        uint16_t length;
        uint32_t routerId;
        uint32_t areaId;
        uint16_t checksum;
        uint16_t authType;
        uint32_t authData1;
        uint32_t authData2;
        std::vector<uint8_t> payload;

        OspfMess(const char* name) : omnetpp::cMessage(name) {}

        // Gửi: đóng OSPF header + body → tạo Mess → send()
        static void send(uint8_t type, const std::vector<uint8_t>& body,
                        uint32_t routerId, uint32_t areaId,
                        int ifIndex, omnetpp::cSimpleModule* mod);

        // Nhận: tách Mess → OSPF header + body, trả về type (0 nếu fail)
        static uint8_t parse(const Mess* msg, const InterfaceData* iface,
                            headerOspf& hdr, std::vector<uint8_t>& data);
};


//////////////////////////////////////////////////////////////////
// phương thức router router
//////////////////////////////////////////////////////////////////

class OspfRouterState {
    public:
        uint32_t routerID;               // Router ID của router này
        std::vector<InterfaceData> interfaces;   // danh sách interface
        AreaData area;                   // area duy nhất
        std::vector<RoutingTableEntry> routingTable; // bảng định tuyến
        std::map<uint32_t, uint32_t> externalRoutes; // route AS-external

        OspfRouterState(uint32_t routerId, int numInterfaces);
        ~OspfRouterState();

        void printState();
};

#endif

