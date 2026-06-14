# Task Spec: T-10a — SPF Calculator (Dijkstra)

> Implement Dijkstra Shortest Path First algorithm từ LSDB.

---

## 1. Overview
Từ LSDB, xây dựng đồ thị và tính shortest path tree với root = self.

## 2. Requirements
- Đọc LSDB → xây graph (vertices = routerIDs, edges = links)
- Dijkstra với priority queue
- Hỗ trợ ECMP: track multiple predecessors

## 3. Input
- LSDB (từ T-08): tất cả RouterLSAs với links
- self routerID

## 4. Process

```cpp
struct SPFResult {
    int destination;
    int cost;
    std::vector<int> predecessors;  // ECMP: nhiều predecessors
};

std::vector<SPFResult> OspfSpf::calculate(const OspfLsdb& lsdb, int selfID) {
    // 1. Build vertex set from LSDB
    std::set<int> vertices;
    for (auto* lsa : lsdb.getAllLSAs()) {
        vertices.insert(lsa->advertisingRouter);
        for (auto& link : lsa->links) {
            vertices.insert(link.linkID);
        }
    }
    
    // 2. Initialize
    std::map<int, int> dist;       // routerID → distance
    std::map<int, std::vector<int>> prev;  // routerID → [predecessors]
    for (int v : vertices) {
        dist[v] = INT_MAX;
        prev[v] = {};
    }
    dist[selfID] = 0;
    
    // 3. Priority queue: (distance, routerID)
    using P = std::pair<int, int>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
    pq.push({0, selfID});
    
    // 4. Dijkstra loop
    while (!pq.empty()) {
        auto [d, u] = pq.top(); pq.pop();
        if (d > dist[u]) continue;
        
        // Get u's links from LSDB
        auto* lsa = lsdb.lookupLSA(u);
        if (!lsa) continue;
        
        for (auto& link : lsa->links) {
            int v = link.linkID;
            int newDist = dist[u] + link.metric;
            
            if (newDist < dist[v]) {
                dist[v] = newDist;
                prev[v] = {u};  // single predecessor
                pq.push({newDist, v});
            } else if (newDist == dist[v]) {
                prev[v].push_back(u);  // ECMP: additional predecessor
            }
        }
    }
    
    // 5. Build result
    std::vector<SPFResult> results;
    for (int v : vertices) {
        if (v != selfID && dist[v] < INT_MAX) {
            results.push_back({v, dist[v], prev[v]});
        }
    }
    return results;
}
```

## 5. Output
- `vector<SPFResult>`: destination, cost, predecessors list

## 6. Acceptance Criteria
- Tính đúng shortest path cho topology đã biết
- ECMP: predecessor với nhiều entry khi có đường bằng cost
- Self → self: cost=0
- Unreachable: bỏ qua

## 7. Related Tasks
- T-08 (LSDB): cung cấp dữ liệu
- T-09 (Flooding): schedule SPF sau flooding
- T-10b (Routing Table): convert SPF result → routing entries
- T-14 (Dump): hiển thị kết quả SPF

## 8. Notes
- Complexity: O((V+E)logV) với V ≤ 10 → rất nhanh
- Dùng `std::priority_queue` cho simplicity
- SPF chạy khi: (a) LSA mới installed, (b) LSA hết hạn
