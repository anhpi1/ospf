#ifndef __OSPF_INFO_ROUTER_H
#define __OSPF_INFO_ROUTER_H

#include <cstdint>
#include <vector>
#include <map>
#include <omnetpp.h>

using namespace omnetpp;

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

struct LsaHeader {
    uint16_t lsAge;              // thời gian sống (giây)
    uint8_t options;             // OSPF capabilities
    uint8_t lsType;              // loại LSA (1=Router-LSA)
    uint32_t linkStateID;        // định danh LSA
    uint32_t advRouter;          // router khởi tạo LSA
    int32_t lsSequenceNumber;    // số thứ tự (monotonic)
    uint16_t lsChecksum;         // Fletcher checksum
    uint16_t length;             // tổng bytes của LSA
};

struct RouterLsaLink {
    uint32_t linkID;             // định danh link (Router ID neighbor)
    uint32_t linkData;           // dữ liệu link (IP interface)
    uint8_t linkType;            // loại link (1=P2P, 2=transit...)
    uint8_t numTos;              // số TOS metric (0=chỉ cost)
    uint16_t metric;             // chi phí link
};

struct RouterLsa {
    LsaHeader header;            // header chung LSA
    bool vBit;                   // virtual link flag
    bool eBit;                   // AS-external flag
    bool bBit;                   // border router flag
    uint16_t numLinks;           // số link trong LSA
    std::vector<RouterLsaLink> links; // danh sách link
};

struct AreaData {
    uint32_t areaID;             // định danh area
    unsigned int transitCapability; // có virtual link không
    unsigned int externalRoutingCapability; // stub flag
    unsigned int stubDefaultCost; // default cost nếu là stub
    std::vector<int> interfaceIndices; // danh sách interface trong area
    std::vector<RouterLsa> routerLSAs; // LSDB chứa Router-LSA
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

class OspfRouterState {
    public:
        uint32_t routerID;               // Router ID của router này
        std::vector<InterfaceData> interfaces;   // danh sách interface
        AreaData area;                   // area duy nhất
        std::vector<RoutingTableEntry> routingTable; // bảng định tuyến
        std::map<uint32_t, uint32_t> externalRoutes; // route AS-external

        OspfRouterState(uint32_t routerId, int numInterfaces);
        ~OspfRouterState();
};

#endif
