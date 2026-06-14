



message OspfMess
{
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t routerId;
    uint32_t areaId;
    uint16_t checksum;
    uint16_t authType;
    uint32_t authData1;
    uint32_t authData2;
    uint8_t payload[]; // Dữ liệu gói tin OSPF
}

