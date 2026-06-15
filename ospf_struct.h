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
    uint8_t TOSid;
    uint8_t zero;
    uint16_t metric;
};

struct LSALink
{
    uint32_t linkID;    
    uint32_t linkData;
    uint8_t type;
    uint8_t numTOS;
    uint16_t metric;
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

// RFC A.2 — Options field bits
enum OspfOptionsBit : uint8_t {
    OPT_E    = 0x02,  // ExternalRoutingCapability (bit 1)
    OPT_MC   = 0x04,  // Multicast (bit 2)
    OPT_NP   = 0x08,  // NSSA/Propagate (bit 3)
    OPT_EA   = 0x10,  // External-Attributes (bit 4)
    OPT_DC   = 0x20,  // Demand Circuits (bit 5)
};

// RFC A.3.3 — DD flags: |0|0|0|0|0|I|M|MS
enum OspfDDFlagBit : uint8_t {
    DD_MS = 0x01,  // Master/Slave (bit 0)
    DD_M  = 0x02,  // More (bit 1)
    DD_I  = 0x04,  // Init (bit 2)
};

// RFC A.4.2 — Router-LSA flags: |0|0|0|0|0|V|E|B
enum OspfRouterLSAFlagBit : uint8_t {
    LSA_FLAG_B = 0x01,  // Border router (bit 0)
    LSA_FLAG_E = 0x02,  // External router (bit 1)
    LSA_FLAG_V = 0x04,  // Virtual link endpoint (bit 2)
};

//
// Section 9 — Cấu trúc dữ liệu giao diện (page 63–66)
//
struct InterfaceData {
    unsigned int type;                               // loại giao diện (P2P, broadcast, NBMA...)
    unsigned int state;                              // trạng thái máy trạng thái interface (Down, P2P, Loopback...)
    uint32_t ipAddress;                              // địa chỉ IP nguồn trong tất cả gói tin gửi qua interface này
    uint32_t mask;                                   // subnet mask. P2P unnumbered → không định nghĩa (RFC: "not defined")
    uint32_t areaID;                                 // Area ID của vùng chứa mạng kết nối
    uint16_t helloInterval;                          // chu kỳ gửi Hello (giây), quảng bá trong gói Hello
    uint32_t routerDeadInterval;                     // thời gian chờ trước khi neighbor tuyên bố router này chết (giây)
    unsigned int infTransDelay;                      // trễ truyền ước tính (giây), cộng vào tuổi LSA trước khi gửi
    uint8_t routerPriority;                          // độ ưu tiên bầu DR (0-255). P2P → luôn 0, không tham gia bầu DR
// Hello Timer — timer định kỳ kích hoạt gửi Hello. Đang dùng chung helloTimer toàn cục (xem ospf.cc)
// Wait Timer — single-shot, dùng để thoát trạng thái Waiting + bầu DR. P2P không có DR/BDR → không cần.
// List of neighboring routers — P2P chỉ có tối đa 1 neighbor, dùng NeighborData* bên dưới.
// Designated Router — P2P không bầu DR → không cần.
// Backup Designated Router — P2P không bầu BDR → không cần.
    uint16_t cost;                                   // chi phí gửi gói dữ liệu qua interface, quảng bá trong router-LSA
    unsigned int rxmtInterval;                       // chu kỳ retransmit LSA (giây), dùng cho DD/LSR/LSU
// AuType — loại xác thực. Dự án không làm xác thực → không cần.
// Authentication key — khóa xác thực. Dự án không làm xác thực → không cần.
    struct NeighborData* neighbor;                   // neighbor P2P duy nhất (P2P chỉ có 1 neighbor)
};

struct NeighborData {
    
    unsigned int state;          // trạng thái máy trạng thái neighbor
    
    unsigned int isMaster;       // vai trò master/slave trong DD exchange
    uint32_t ddSequenceNumber;   // số thứ tự DD đang trao đổi
    uint8_t lastDdOptions;       // Options từ DD packet cuối
    uint8_t lastDdIMs;           // I+M+MS bits từ DD packet cuối, suy luận từ databaseDescriptionData
    uint32_t IDNeighbor;         // Router ID của neighbor
    uint8_t priorityNeighbor;    // Router Priority của neighbor
    uint32_t IPNeighbor;         // Router IP của neighbor
    uint8_t optionsNeighbor;     // Options từ Hello packet cuối
//  Neighbor's Designated Router 
//  Neighbor's Backup Designated Router
// trong mạng P2P không cần thiết, nên tạm bỏ qua
// ba danh sách này nằm trong NeighborData, chứa bản sao dữ liệu thật (LSAHeader /LSARequest), độc lập với AreaData. Không có chuyện "trỏ đến Area".
    std::vector<LSAHeader> databaseSummaryList;  // LSA đã flood nhưng chưa được ACK, sẽ retransmit định kỳ
    std::vector<LSAHeader> linkStateRequestList ;  // Toàn bộ LSAHeader của LSDB lúc vào Exchange, gửi cho neighbor qua DD
    std::vector<LSARequest> linkStateRetransmissionList; // LSA còn thiếu so với neighbor, gửi qua LS Request
// một số biến timer tự định nghĩa để hoạt động trong omnetpp, không phải là timer OSPF gốc nhưng giúp mô phỏng dễ dàng hơn
    cMessage* rxmtTimer;         // timer retransmit DD/LS Request
    cMessage* inactivityTimer;   // timer theo dõi thời gian sống
    
};

// Cấu trúc 1 đỉnh trong danh sách kề — biểu diễn kết quả SPF (đệ quy con trỏ)
struct SpfVertex {
    uint32_t vertexId;                           // Router ID
    // Một LSA — router-LSA hoặc network-LSA gắn với đỉnh. P2P → luôn là router-LSA
    std::vector<SpfVertex*> neighbors;           // danh sách kề: con trỏ tới đỉnh con
    SpfVertex* parent;                           // con trỏ tới đỉnh cha (nullptr nếu là gốc)
    unsigned int nextHop;                        // index vào state->interfaces[], interface ra để tới đỉnh này
    uint16_t distance;                           // tổng chi phí từ gốc đến đỉnh này
};

//
// Section 6 — Cấu trúc dữ liệu vùng (page 49–51)
//
struct AreaData {
    uint32_t areaID;                                 // định danh vùng (32-bit), backbone = 0.0.0.0
// List of area address range
//List of area address ranges chỉ dùng khi router là ABR (Area Border Router) - router nói giữa nhiều area, cần tổng hợp route để quảng bá sang area khác.
// Mỗi range là một cặp [address, mask] + trạng thái Advertise / DoNotAdvertise.
// Dự án mình chỉ có 1 area duy nhất (backbone 0.0.0.0), không có ABR, nên danh sách này luôn rỗng – không cần khai báo.
    std::vector<unsigned int> interfaceIndices;      // index vào state->interfaces[], các interface thuộc vùng này
    std::vector<LSA> routerLSAs;                     // LSDB: Router-LSA từ mỗi router trong vùng
// network-LSA: chỉ dùng cho mạng broadcast/NBMA có DR — P2P không cần.
// summary-LSA: chỉ ABR tạo ra để quảng bá liên vùng — single-area không cần.
    std::vector<SpfVertex> spfVertices;              // danh sách kề kết quả SPF, các đỉnh + neighbors = cây
    bool transitCapability;                          // TRUE nếu có virtual link dùng vùng này làm transit area
    bool externalRoutingCapability;                  // TRUE = cho AS-external-LSA vào vùng; FALSE = stub area
    uint32_t stubDefaultCost;                        // cost default route nếu là stub + ABR, không stub → 0
};

//
// Section 11 — Cấu trúc Bảng Định tuyến (page 107–110)
//
struct RoutingTableEntry {
    uint8_t destinationType;                         // N=network (forward gói IP) hoặc R=router (trung gian)
    uint32_t destinationId;                          // địa chỉ IP mạng đích (type=network) hoặc Router ID (type=router)
    uint32_t addressMask;                            // subnet mask, chỉ cho network; host route = 0xffffffff
// Optional Capabilities — chỉ khi đích là router (ABR/ASBR). Single-area → không cần.
    uint32_t area;                                   // vùng liên kết của entry; AS-external → không định nghĩa
    uint8_t pathType;                                // intra(1)/inter(2)/type1(3)/type2(4)
    uint32_t cost;                                   // tổng chi phí
// Type 2 Cost — chỉ cho type2 external. Single-area → không cần.
// Link State Origin — LSA tham chiếu đến đích, chỉ MOSPF dùng.
// Advertising Router — chỉ inter-area và AS-external. Single-area → không cần.
    uint32_t nextHop;                                // Router ID của hop kế tiếp
};

//
// RoutingTable — bảng định tuyến duy nhất của router (RFC Section 11)
//



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
                                 InterfaceData* iface, int ifIndex, uint32_t myRouterId,
                                 omnetpp::cSimpleModule* mod);
};
class databaseDescriptionData
{
    public:
        uint16_t interfaceMTU;
        uint8_t options;
        uint8_t flags;
        uint32_t ddSequenceNumber;
        std::vector<LSAHeader> lsaHeaders;

        // BƯỚC 1 ExStart: gửi DD rỗng (I=1,M=1,MS=1) để khởi động thương lượng Master/Slave
        static void sendExStart(InterfaceData* iface, int ifIndex, uint32_t routerId,
                                omnetpp::cSimpleModule* mod);
        // BƯỚC 2 ExStart: nhận DD, thương lượng Master/Slave (RFC 2328 Section 10.6)
        static void processExStart(const headerOspf& hdr, const std::vector<uint8_t>& data,
                                   InterfaceData* iface, int ifIndex, uint32_t myRouterId,
                                   OspfRouterState& state, omnetpp::cSimpleModule* mod);
        // GIAI ĐOẠN 2 BƯỚC 1: Master gửi DD chứa LSA headers từ LSDB
        static void sendExchange(InterfaceData* iface, int ifIndex, uint32_t routerId,
                                 OspfRouterState& state, omnetpp::cSimpleModule* mod);
        // GIAI ĐOẠN 2 BƯỚC 2: Master nhận ACK từ Slave (RFC 2328 Section 10.6)
        static void processExchangeForMaster(const headerOspf& hdr, const std::vector<uint8_t>& data,
                                             InterfaceData* iface, int ifIndex, uint32_t myRouterId,
                                             OspfRouterState& state, omnetpp::cSimpleModule* mod);
        // GIAI ĐOẠN 2 BƯỚC 2: Slave nhận DD từ Master, so sánh LSDB, gửi ACK
        static void processExchangeForSlave(const headerOspf& hdr, const std::vector<uint8_t>& data,
                                            InterfaceData* iface, int ifIndex, uint32_t myRouterId,
                                            OspfRouterState& state, omnetpp::cSimpleModule* mod);
};
class linkStateRequestData
{
    public:
        std::vector<LSARequest> requests;

        // FLOW A — BƯỚC A1: gửi Link State Request (type 3) (RFC 10.9)
        static void sendLSR(InterfaceData* iface, int ifIndex, uint32_t routerId,
                                const std::vector<LSARequest>& reqs, omnetpp::cSimpleModule* mod);
        // FLOW B: nhận LS Request (type 3), tìm LSA trong LSDB, gửi LSU trả lời (RFC 10.7)
        static void processLSR(const headerOspf& hdr, const std::vector<uint8_t>& data,
                               InterfaceData* iface, int ifIndex, uint32_t myRouterId,
                               OspfRouterState& state, omnetpp::cSimpleModule* mod);
};

class linkStateUpdateData
{
    public:
        uint32_t numberOfLSA;
        std::vector<LSA> LSAs;

        // FLOW A — BƯỚC A2: nhận LSU, cài LSDB, gửi LSAck (RFC 13, 13.2, 13.5)
        static void processLSU(const headerOspf& hdr, const std::vector<uint8_t>& data,
                               InterfaceData* iface, int ifIndex, uint32_t myRouterId,
                               OspfRouterState& state, omnetpp::cSimpleModule* mod);
        // FLOW B: gửi Link State Update (type 4) trả lời LS Request
        static void sendLSU(const std::vector<LSA>& lsas, InterfaceData* iface,
                            int ifIndex, uint32_t routerId, omnetpp::cSimpleModule* mod);
};


//type 5
class linkStateAcknowledgementData
{
    public:
        std::vector<LSAHeader> data;

        // Gửi Link State Acknowledgment (type 5) — danh sách LSAHeader
        static void sendAck(const std::vector<LSAHeader>& headers,
                            InterfaceData* iface, int ifIndex, uint32_t routerId,
                            omnetpp::cSimpleModule* mod);
        // Nhận LSAck (type 5), xóa LSA khỏi retransmission list
        static void processAck(const headerOspf& hdr, const std::vector<uint8_t>& data,
                               InterfaceData* iface, int ifIndex,
                               OspfRouterState& state, omnetpp::cSimpleModule* mod);
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
        std::vector<RoutingTableEntry> RoutingTable;

        std::map<uint32_t, uint32_t> externalRoutes; // route AS-external

        OspfRouterState(uint32_t routerId, int numInterfaces);
        ~OspfRouterState();

        void printState();
};

#endif

