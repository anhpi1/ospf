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
    int32_t sequenceNumber;      // signed 32-bit (Section 12.1.6, RFC 2328)
    uint16_t checksum;
    uint16_t length;

    void print(std::ostream& f) const;
};

struct LSARequest
{
    uint32_t LSType;
    uint32_t linkStateId;
    uint32_t advertisingRouter;

    void print(std::ostream& f) const;
};

struct TOSData
{
    uint8_t TOSid;
    uint8_t zero;
    uint16_t metric;

    void print(std::ostream& f) const;
};

struct LSALink
{
    uint32_t linkID;
    uint32_t linkData;
    uint8_t type;
    uint8_t numTOS;
    uint16_t metric;
    std::vector<TOSData> Data; // danh sách TOS entries, nếu numTOS > 0

    void print(std::ostream& f) const;
};

struct LSA
{
    LSAHeader header;
    uint8_t flags;
    uint8_t zero;
    uint16_t numLinks;
    std::vector<LSALink> links; // nội dung LSA, định dạng tùy theo type

    void print(std::ostream& f) const;
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

    void print(std::ostream& f, int index) const;
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

    void print(std::ostream& f) const;
};

// Cấu trúc 1 đỉnh trong danh sách kề — biểu diễn kết quả SPF (đệ quy con trỏ)
struct SpfVertex {
    uint32_t vertexId;                           // Router ID
    // Một LSA — router-LSA hoặc network-LSA gắn với đỉnh. P2P → luôn là router-LSA
    std::vector<SpfVertex*> neighbors;           // danh sách kề: con trỏ tới đỉnh con
    SpfVertex* parent;                           // con trỏ tới đỉnh cha (nullptr nếu là gốc)
    unsigned int nextHop;                        // index vào state->interfaces[], interface ra để tới đỉnh này
    uint16_t distance;                           // tổng chi phí từ gốc đến đỉnh này

    void print(std::ostream& f, int depth = 0) const;
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

    void print(std::ostream& f) const;
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

    void print(std::ostream& f, int index) const;
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
        // Chỉ parse + cập nhật state. KHÔNG đụng timer — handleMessage lo.
        // Trả về true nếu Hello hợp lệ (đã pass validation).
        static bool processHello(const headerOspf& hdr, const std::vector<uint8_t>& data,
                                 InterfaceData* iface, int ifIndex, uint32_t myRouterId);
};
// Struct kết quả processDD (Rule 8: XxxResult pattern)
struct DdResult {
    bool valid;                // false → drop gói
    bool negotiationDone;      // NegotiationDone event → chuyển Exchange
    bool exchangeDone;         // ExchangeDone event → chuyển Loading/Full
    bool shouldSendDD;         // Slave cần reply DD ngay
    bool neighborStateChanged; // state changed → cần xử lý timer
};

class databaseDescriptionData
{
    public:
        uint16_t interfaceMTU;
        uint8_t options;
        uint8_t flags;
        uint32_t ddSequenceNumber;
        std::vector<LSAHeader> lsaHeaders;

        // Gửi DD packet (RFC 2328 Section 10.8 + A.3.3)
        static void sendDD(int ifIndex, OspfRouterState& state,
                           uint32_t routerId, omnetpp::cSimpleModule* mod);

        // Xử lý DD packet nhận (RFC 2328 Section 10.6)
        // Chỉ parse + cập nhật state. KHÔNG đụng timer.
        // Trả về DdResult chứa kết quả xử lý.
        static DdResult processDD(const headerOspf& hdr,
                                  const std::vector<uint8_t>& data,
                                  InterfaceData* iface, uint32_t myRouterId,
                                  const std::vector<LSA>& routerLSAs);
};

// Struct kết quả processLSR (Rule 8: XxxResult pattern)
struct LsrResult {
    bool valid;                // false → BadLSReq / lỗi
    bool badLSReq;             // true → handleMessage chuyển ExStart
    std::vector<LSA> lsus;     // LSAs cần gửi về trong LSU
};

// Struct kết quả processLSU (Rule 8 pattern)
struct LsuResult {
    bool valid;                // false → drop gói
    bool loadingDone;          // true → state = Full
    bool scheduleSPF;          // true → schedule SPF
    bool badLSReq;             // true → BadLSReq (Section 13 step 6)
    std::vector<LSAHeader> ackHeaders;  // LSA headers cần ACK
    std::vector<LSAHeader> newLsas;     // LSA headers mới cài vào LSDB (1c: flood tiếp)
};

class linkStateRequestData
{
    public:
        std::vector<LSARequest> requests;

        // Gửi LSR packet (RFC 2328 Section 10.9 + A.3.4)
        // Lấy đầu linkStateRequestList, KHÔNG xóa khỏi list (giữ lại để retransmit)
        static void sendLSR(int ifIndex, OspfRouterState& state,
                            uint32_t routerId, omnetpp::cSimpleModule* mod);

        // Xử lý LSR packet nhận (RFC 2328 Section 10.7)
        // Tra LSDB → copy LSA vào LSU body. Nếu không tìm thấy → BadLSReq.
        // KHÔNG đụng timer. Trả về LsrResult.
        static LsrResult processLSR(const headerOspf& hdr,
                                     const std::vector<uint8_t>& data,
                                     InterfaceData* iface,
                                     const std::vector<LSA>& routerLSAs);
};

class linkStateUpdateData
{
    public:
        uint32_t numberOfLSA;
        std::vector<LSA> LSAs;

        // Gửi LSU packet (RFC 2328 Section 13 + A.3.5)
        static void sendLSU(int ifIndex, const std::vector<LSA>& lsas,
                            uint32_t routerId, uint32_t areaId,
                            omnetpp::cSimpleModule* mod);

        // Xử lý LSU packet nhận (RFC 2328 Section 13 steps 1-8)
        // Validate từng LSA → so sánh với LSDB → cài / ACK / discard.
        // Truncate linkStateRequestList nếu LSA là response cho LSR.
        // KHÔNG đụng timer. Trả về LsuResult.
        static LsuResult processLSU(const headerOspf& hdr,
                                     const std::vector<uint8_t>& data,
                                     InterfaceData* iface,
                                     AreaData& area,
                                     std::vector<LSAHeader>& requestList);

        // Flood LSA ra tất cả interface (RFC 2328 Section 13.3)
        // incomingIfIndex = -1 nếu self-originated (flood ra tất cả)
        // incomingIfIndex >= 0 nếu nhận từ neighbor (không flood ngược lại)
        static void floodLSA(const LSA& lsa, int incomingIfIndex,
                              OspfRouterState& state, uint32_t routerId,
                              omnetpp::cSimpleModule* mod);
};

//type 5
class linkStateAcknowledgementData
{
    public:
        std::vector<LSAHeader> data;

        // Gửi LSAck packet (RFC 2328 Section 13.5 + A.3.6)
        static void sendLSAck(int ifIndex, const std::vector<LSAHeader>& headers,
                              uint32_t routerId, uint32_t areaId,
                              omnetpp::cSimpleModule* mod);
};
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

        // Log state sau transition, ghi vào state_dump/<subphase>/ (gọi từ handleMessage)
        void logTransition(const char* subphase, const char* event,
                           double simtime, int ifIndex);

        void originateRouterLSA();
};

#endif

