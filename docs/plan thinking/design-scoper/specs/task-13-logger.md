# Task Spec: T-13 — Logger (Transaction + Content)

> Ghi 2 file log: packet transaction log (có mã số) + message content log.

---

## 1. Overview
Logger ghi lại mọi hành động gửi/nhận packet với mã số duy nhất, và nội dung chi tiết của mỗi message.

## 2. Requirements
- Transaction log: mỗi action có mã số TX-001, TX-002, ...
- Content log: dump fields của mỗi message
- Ghi vào 2 file riêng biệt

## 3. Input
- Các action từ OspfRouter (gửi/nhận/timeout)
- Packet contents

## 4. Process

```cpp
class Logger {
private:
    std::ofstream txFile;
    std::ofstream contentFile;
    int transactionID = 0;
    
public:
    Logger() {
        txFile.open("results/transaction.log");
        contentFile.open("results/content.log");
    }
    
    ~Logger() {
        txFile.close();
        contentFile.close();
    }
    
    // Transaction log
    void logTx(int src, int dst, const std::string& type, const std::string& detail) {
        transactionID++;
        txFile << "[TX-" << std::setw(3) << std::setfill('0') << transactionID << "] "
               << "T=" << simTime() << " "
               << ipFormat(src) << "->" << ipFormat(dst) << " "
               << type << " "
               << detail << std::endl;
    }
    
    // Content log
    void logContent(const OspfHello* hello) {
        contentFile << "[CONTENT] T=" << simTime() << " "
                    << "TYPE=HELLO "
                    << "routerID=" << ipFormat(hello->getRouterID()) << " "
                    << "helloInt=" << hello->getHelloInterval() << " "
                    << "deadInt=" << hello->getDeadInterval() << " "
                    << "neighbors=[";
        for (int i = 0; i < hello->getNeighborIDsArraySize(); i++) {
            if (i > 0) contentFile << ",";
            contentFile << hello->getNeighborIDs(i);
        }
        contentFile << "]" << std::endl;
    }
    
    void logContent(const OspfDD* dd) {
        contentFile << "[CONTENT] T=" << simTime() << " "
                    << "TYPE=DD "
                    << "i=" << dd->getIBit() << " "
                    << "m=" << dd->getMBit() << " "
                    << "ms=" << dd->getMsBit() << " "
                    << "seq=" << dd->getDdSequenceNumber() << " "
                    << "lsaHeaders=[" << dd->getLsaHeadersArraySize() << "]" << std::endl;
    }
    
    void logContent(const DataPacket* data) {
        contentFile << "[CONTENT] T=" << simTime() << " "
                    << "TYPE=DATA "
                    << "src=" << ipFormat(data->getSrcRouterID()) << " "
                    << "dst=" << ipFormat(data->getDstRouterID()) << " "
                    << "seq=" << data->getSeqNumber() << std::endl;
    }
    
private:
    std::string ipFormat(int id) {
        return "10.0.0." + std::to_string(id);
    }
};
```

**Integration với OspfRouter:**
```cpp
// Trong OspfRouter, khi gửi:
logTx(routerID, nbrID, "HELLO", "neighbors=[" + neighborList + "]");
logContent(hello);

// Khi nhận:
logTx(nbrID, routerID, "HELLO", "neighbors=[" + neighborList + "]");
logContent(received);
```

## 5. Output
- `results/transaction.log`
- `results/content.log`

## 6. Acceptance Criteria
- transaction.log: mỗi dòng bắt đầu `[TX-001]` format
- content.log: mỗi dòng bắt đầu `[CONTENT]`
- Mỗi action gửi/nhận đều có TX ID
- Fields dump chính xác

## 7. Related Tasks
- T-04 (Skeleton): OspfRouter calls Logger
- T-05→T-09 (Protocol): mỗi packet gửi/nhận đều log
- T-12 (Forwarding): log forwarding actions
- T-14 (Dump): convergence log

## 8. Notes
- Logger là singleton (dùng file static hoặc được OspfRouter tạo)
- File log nằm trong `results/` directory
- OSPF protocol fields: ospfType, routerID, cost, sequence numbers
