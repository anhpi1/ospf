# Task Spec: T-08 — LSDB + LSA Definitions

> Link State Database — lưu trữ và quản lý Router-LSAs.

---

## 1. Overview
Class OspfLsdb quản lý collection các RouterLSA, hỗ trợ install, lookup, duplicate detection.

## 2. Requirements
- Lưu trữ RouterLSAs với key = (advertisingRouter, lsType, lsID)
- Sequence number comparison để detect newer/older
- Retrieve all LSAs cho SPF calculation

## 3. Input
- RouterLSA fields từ T-03 LSU packets
- OSPF sequence number rules (§13.1)

## 4. Process

**Cấu trúc dữ liệu:**
```cpp
struct RouterLSA {
    int advertisingRouter;  // 10.0.0.x
    int lsSequenceNumber;   // bắt đầu từ 0x80000001, increment mỗi lần
    simtime_t lsAge;        // thời gian tồn tại
    // Links
    struct Link {
        int linkID;         // neighbor routerID
        int linkData;       // interface IP (last octet)
        int type;           // 1 = P2P
        int metric;         // cost (1-30)
    };
    std::vector<Link> links;
};
```

**LSDB Class:**
```cpp
class OspfLsdb {
private:
    // Key: (advertisingRouter, 1, advertisingRouter) — type=1 RouterLSA
    std::map<int, RouterLSA> lsas;  // key = advertisingRouter
    
public:
    bool installLSA(const RouterLSA& lsa);  // returns true if newer
    const RouterLSA* lookupLSA(int routerID) const;
    std::vector<const RouterLSA*> getAllLSAs() const;
    int getNextSequenceNumber(int routerID) const;
};
```

**installLSA logic:**
1. Nếu chưa có → add new
2. Nếu có + seq# cao hơn → update
3. Bằng seq# → không làm gì (duplicate)
4. Seq# thấp hơn → drop (stale)

**Sequence number:** bắt đầu 0x80000001, increment 1 mỗi lần originate

## 5. Output
- `src/OspfLsdb.h`
- `src/OspfLsdb.cc`
- LSDB operations available cho T-07b, T-10a

## 6. Acceptance Criteria
- install và lookup hoạt động
- install LSA với seq# cao hơn → replace
- install LSA với seq# bằng → keep existing
- getAllLSAs trả về tất cả

## 7. Related Tasks
- T-03 (.msg): RouterLSA fields
- T-07b (LSR/LSU): install LSAs từ LSU
- T-09 (Flooding): đọc LSDB để flood
- T-10a (SPF): đọc LSDB để tính toán

## 8. Notes
- Dùng `std::map` với key đơn giản (advertisingRouter) vì chỉ có Router-LSAs
- LSDB sync check: so sánh size + seq# giữa các router sau khi exchange
