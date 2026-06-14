# Task Spec: T-03 — OSPF Packet Definitions (.msg)

> Định nghĩa 5 OSPF packet types + RouterLSA trong OMNeT++ .msg file.

---

## 1. Overview
Tạo file `OspfPacket.msg` định nghĩa các packet classes dùng cho OSPF protocol.

## 2. Requirements
- Kế thừa từ `cPacket` (OMNeT++ base packet class)
- Đủ field cho OSPF routing (single-area, P2P, không authentication)
- Generate được `_m.h` / `_m.cc` qua `opp_msgc`

## 3. Input
- RFC 2328 §A.3 (OSPF packet formats), §A.4 (LSA formats)
- OMNeT++ .msg syntax

## 4. Process

**Base OSPF Packet:**
```
packet OspfPacket {
    int ospfType;       // 1=Hello, 2=DD, 3=LSR, 4=LSU, 5=LSAck
    int routerID;       // 10.0.0.x → integer
    int areaID = 0;     // always 0 (single area)
}
```

**Hello:**
```
packet OspfHello extends OspfPacket {
    int helloInterval = 1;
    int deadInterval = 4;
    int neighborIDs[];  // list of seen neighbors
    setOspfType(1);
}
```

**Database Description:**
```
packet OspfDD extends OspfPacket {
    bool iBit = true;
    bool mBit = true;
    bool msBit = true;
    int ddSequenceNumber;
    int lsaHeaders[];   // compact: advertisingRouter + lsSequenceNumber
    setOspfType(2);
}
```

**Link State Request:**
```
packet OspfLSR extends OspfPacket {
    int requestTypes[];     // lsType
    int requestIDs[];       // lsID
    int requestRouters[];   // advertisingRouter
    setOspfType(3);
}
```

**Link State Update:**
```
packet OspfLSU extends OspfPacket {
    int lsaRouters[];       // advertisingRouter of each LSA
    int lsaSeqNums[];       // sequence number of each LSA
    int lsaLinkCounts[];    // link count of each LSA
    // Chứa N RouterLSAs, fields flatten thành arrays
    setOspfType(4);
}
```

**Link State Acknowledgment:**
```
packet OspfLSAck extends OspfPacket {
    int ackedRouters[];     // list of advertising routers acked
    setOspfType(5);
}
```

**Data Packet (cho client traffic):**
```
packet DataPacket {
    int srcRouterID;
    int dstRouterID;
    int seqNumber;
    // payload không cần thiết, chỉ cần header
}
```

## 5. Output
- `src/OspfPacket.msg`
- `src/OspfPacket_m.h` (generated)
- `src/OspfPacket_m.cc` (generated)

## 6. Acceptance Criteria
- `opp_msgc src/OspfPacket.msg` sinh file _m.h/_m.cc không lỗi
- Tất cả packet classes có sẵn trong C++ code

## 7. Related Tasks
- T-04 (Skeleton): cần OspfPacket classes
- T-05 (Hello): cần OspfHello
- T-07a (DD): cần OspfDD
- T-07b (LSR/LSU): cần OspfLSR, OspfLSU, OspfLSAck
- T-11 (Client): cần DataPacket
- T-08 (LSDB): cần RouterLSA fields

## 8. Notes
- Flatten LSA fields thành arrays trong LSU (không dùng nested packet do complexity)
- DataPacket không phải OSPF — dùng riêng, không extends OspfPacket
- fields dùng `int` thay vì IP address string để dễ xử lý
