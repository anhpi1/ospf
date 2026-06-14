# Task Spec: T-11 — Client Module

> Simple module gửi 1 data packet qua OspfRouter.

---

## 1. Overview
Client kết nối với OspfRouter qua local gate. Gửi 1 data packet từ R6 đến R10.

## 2. Requirements
- cSimpleModule với 1 gate nối đến OspfRouter
- Gửi 1 DataPacket duy nhất tại sendTime

## 3. Input
- `sendTime` parameter
- `srcRouterID`, `dstRouterID` parameters
- DataPacket class từ T-03

## 4. Process

```cpp
class Client : public cSimpleModule {
private:
    int srcRouterID;
    int dstRouterID;
    simtime_t sendTime;
    bool sent = false;
    
protected:
    void initialize() override {
        srcRouterID = par("srcRouterID");
        dstRouterID = par("dstRouterID");
        sendTime = par("sendTime");
    }
    
    void handleMessage(cMessage* msg) override {
        if (msg->isSelfMessage()) {
            // Send data packet
            auto* data = new DataPacket("DATA");
            data->setSrcRouterID(srcRouterID);
            data->setDstRouterID(dstRouterID);
            data->setSeqNumber(1);
            send(data, "localOut");
            sent = true;
            EV << "Client " << srcRouterID << " sent data to " << dstRouterID << endl;
        } else {
            // Received data packet (we are destination)
            DataPacket* data = check_and_cast<DataPacket*>(msg);
            EV << "Client " << srcRouterID << " received data from " 
               << data->getSrcRouterID() << endl;
            delete msg;
        }
    }
};
```

## 5. Output
- DataPacket gửi đến OspfRouter

## 6. Acceptance Criteria
- Client gửi 1 DataPacket tại T=sendTime
- Packet có đúng src + dst

## 7. Related Tasks
- T-03 (.msg): DataPacket
- T-04 (Skeleton): OspfRouter base
- T-12 (Data Forwarding): forward packet đến destination
- T-02 (NED): thêm Client submodule vào OspfNetwork

## 8. Notes
- R6: srcRouterID=6, dstRouterID=10, sendTime=5s (sau khi OSPF hội tụ)
- R10: srcRouterID=10, dstRouterID=0 (không gửi, chỉ nhận)
- Các client khác: no-op (không gửi, không nhận)
