# Task Spec: T-04 — OspfRouter Module Skeleton

> Tạo cSimpleModule OspfRouter với cấu trúc cơ bản.

---

## 1. Overview
Class OspfRouter kế thừa cSimpleModule, định nghĩa initialize(), handleMessage(), finish().

## 2. Requirements
- Kế thừa `cSimpleModule`
- Vector gate `ppg[]` cho P2P interfaces
- Các parameters: helloInterval, routerDeadInterval, rxmtInterval, spfDelay, routerID

## 3. Input
- OMNeT++ cSimpleModule API
- OspfPacket classes từ T-03

## 4. Process

**Header (OspfRouter.h):**
```cpp
class OspfRouter : public cSimpleModule {
private:
    // Parameters
    double helloInterval;
    double routerDeadInterval;
    double rxmtInterval;
    double spfDelay;
    int routerID;  // convert from string param
    
    // Components (forward declarations)
    class NeighborState;
    struct InterfaceData;
    OspfLsdb* lsdb;
    OspfRoutingTable* routingTable;
    
    // Timers
    cMessage* helloTimer;
    
protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage* msg) override;
    virtual void finish() override;
    
    // Dispatch
    void handleTimer(cMessage* msg);
    void handleOSPF(cMessage* msg);
    void handleData(cMessage* msg);
};
```

**Initialize:** parse parameters → create timers → schedule helloTimer

**handleMessage:** dispatch: self-message → timer, instanceof OspfPacket → handleOSPF, instanceof DataPacket → handleData

**finish:** dump routing table (nếu configured)

## 5. Output
- `src/OspfRouter.h`
- `src/OspfRouter.cc` (skeleton — methods bỏ trống hoặc stub)

## 6. Acceptance Criteria
- Compile thành công
- Module xuất hiện trong Qtenv với gates và parameters

## 7. Related Tasks
- T-03 (.msg): cần packet classes
- T-05→T-09, T-12, T-14: sẽ implement các method trong OspfRouter
- T-11 (Client): module sibling trong compound
- T-13 (Logger): được gọi từ OspfRouter methods

## 8. Notes
- Dùng `std::vector<InterfaceData>` để quản lý interfaces
- routerID convert từ string "10.0.0.1" → int 1 (phần octet cuối)
