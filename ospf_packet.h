#include <cstdint>
#include <vector>
#include <omnetpp.h>


//type 1
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
        
};

//type 2
struct LSAHeader
{
    uint16_t age;
    uint8_t options;
    uint8_t type;
    uint32_t linkStateId;
    uint32_t advertisingRouter;
    uint16_t sequenceNumber;
    uint16_t checksum;
    uint16_t length;
};

class databaseDescriptionData
{
    public:
        uint16_t interfaceMTU;
        uint8_t options;
        uint8_t flags;
        uint32_t ddSequenceNumber;
        LSAHeader data;
};

//type 3 4 5
struct linkState
{

    uint32_t LSType;
    uint32_t linkStateId;
    uint32_t advertisingRouter;
};

class linkStateRequestData
{
    public:
        std::vector<linkState> data;
};

class linkStateUpdateData
{
    public:
        uint32_t numberOfLSAs;
        std::vector<linkState> data;
};

class linkStateAcknowledgementData
{
    public:
        uint32_t numberOfLSAs;
        std::vector<linkState> data;
};

//header OSPF packet

// 24-byte OSPF header (RFC A.3.1)
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

        static void put32(uint8_t* buf, int& off, uint32_t v);
        static void put16(uint8_t* buf, int& off, uint16_t v);
        static void put8(uint8_t* buf, int& off, uint8_t v);

        // Tách msg thành header + data thô, trả về type (0 nếu fail)
        static uint8_t parsePacket(const OspfMess* msg, const InterfaceData* iface,
                                   headerOspf& hdr, std::vector<uint8_t>& data);
};

