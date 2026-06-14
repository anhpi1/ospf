# Task Spec: T-10b — Routing Table + ECMP

> Xây dựng routing table từ SPF result, hỗ trợ ECMP.

---

## 1. Overview
Convert SPF result → routing entries, resolve next-hops từ predecessors chain, hỗ trợ multiple next-hops.

## 2. Requirements
- Mỗi entry: destination, cost, danh sách next-hops
- ECMP: dest có thể có ≥1 next-hop
- resolveNextHop(): từ predecessors chain tìm next-hop đầu tiên
- dump(): in routing table

## 3. Input
- SPFResult từ T-10a
- Interface-to-neighbor mapping

## 4. Process

```cpp
struct NextHop {
    int neighborID;      // next-hop routerID
    int interfaceIndex;  // gate index
};

struct RoutingEntry {
    int destination;
    int cost;
    std::vector<NextHop> nextHops;  // ECMP support
};

class OspfRoutingTable {
private:
    std::vector<RoutingEntry> entries;
    
    // Resolve next-hop từ predecessors chain
    // Nếu dest là neighbor trực tiếp → nextHop = dest
    // Nếu không → trace qua predecessors để tìm neighbor đầu tiên
    std::vector<NextHop> resolveNextHops(int dest, const std::vector<int>& predecessors);

public:
    void update(const std::vector<SPFResult>& spfResults);
    const RoutingEntry* lookup(int destination) const;
    void clear();
    std::string dump() const;
};
```

**resolveNextHop logic (ECMP):**
```
For each predecessor p of dest:
  if p == self → nextHop = dest (directly connected)
  else → recurse: find nextHop for p (first hop)
  
Nếu 2 predecessors dẫn đến 2 next-hops khác nhau → ECMP
```

**dump() output format:**
```
Routing Table for Router 10.0.0.1:
Dest        Cost    NextHops
10.0.0.2    5       [10.0.0.2]
10.0.0.3    12      [10.0.0.2, 10.0.0.4]   ← ECMP
10.0.0.4    3       [10.0.0.4]
...
```

## 5. Output
- `OspfRoutingTable` object trong OspfRouter
- dump() method cho console/file output

## 6. Acceptance Criteria
- Routing table đúng SPF result
- ECMP: nếu SPF result có 2 predecessors → routing entry có 2 next-hops
- lookup(dest) trả về entry chính xác
- dump() in đẹp

## 7. Related Tasks
- T-10a (SPF): cung cấp SPF result
- T-12 (Data Forwarding): dùng routing table
- T-14 (Dump): gọi dump() + convergence

## 8. Notes
- clear() + update() mỗi khi SPF chạy
- ECMP: round-robin hoặc first trong data forwarding
- Entries sorted by destination
