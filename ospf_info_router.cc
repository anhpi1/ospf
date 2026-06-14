#include "ospf.h"

OspfRouterState::OspfRouterState(uint32_t routerId, int numInterfaces)
{
    routerID = routerId;
    externalRoutes.clear();                           // không dùng (chỉ Router-LSA)

    // ===== Interface configuration =====
    // Mỗi gate tương ứng một interface OSPF P2P (Section 9).
    // Các giá trị dưới đây dùng sample values cho mạng LAN từ Appendix C.3.
    interfaces.resize(numInterfaces);
    for (int i = 0; i < numInterfaces; i++)
    {
        InterfaceData* iface = &interfaces[i];

        // --- Cấu hình interface (Section 9, Appendix C.3) ---
        iface->type = 1;                     // Kiểu link: P2P (LINK_P2P = 1 trong Router-LSA, Appendix A.4.2)
        iface->state = IF_DOWN;              // Khởi tạo ở trạng thái Down, chờ Hello đưa lên PointToPoint
        iface->ipAddress = routerId;           // Unnumbered P2P: dùng Router ID làm địa chỉ interface
        iface->mask = 0;                     // Dùng unnumbered point-to-point (không cần subnet mask)
        iface->areaID = 0;                   // Thuộc backbone area 0.0.0.0 (single-area)
        iface->helloInterval = 10;           // Chu kỳ gửi Hello: 10 giây (sample value cho LAN, Appendix C.3)
        iface->routerDeadInterval = 40;      // Ngưỡng coi neighbor chết: 4 × HelloInterval (khuyến nghị Appendix C.3)
        iface->infTransDelay = 1;            // Độ trễ truyền ước tính: 1 giây (sample value cho LAN)
        iface->routerPriority = 0;           // P2P không cần DR/BDR nên để 0 (không đủ điều kiện bầu DR)
        iface->cost = 1;                     // Chi phí gửi gói qua interface, quảng bá trong Router-LSA. Phải > 0.
        iface->rxmtInterval = 5;             // Khoảng retransmit LSA: 5 giây (sample value cho LAN), phải lớn hơn RTT

        // --- Khởi tạo neighbor (Section 10.1) ---
        // P2P: mỗi interface có đúng 1 neighbor, cấp phát tĩnh từ initState.
        // Các trường chưa biết sẽ được điền khi nhận Hello / trong quá trình adjacency.
        iface->neighbor = new NeighborData;
        iface->neighbor->neighborID = 0;       // Chưa biết, học từ Router ID trong Hello của đối phương
        iface->neighbor->state = NBR_DOWN;     // Trạng thái khởi tạo: chưa có liên lạc với neighbor
        iface->neighbor->inactivityTimer = nullptr; // Timer theo dõi neighbor, reset mỗi khi nhận Hello
        iface->neighbor->isMaster = false;     // Vai trò master/slave sẽ xác định trong ExStart (Section 10.8)
        iface->neighbor->ddSequenceNumber = 0; // Số tuần tự DD, khởi tạo trong ExStart (Section 10.8)
        iface->neighbor->lastDdOptions = 0;    // Options field từ DD cuối cùng nhận được (Section 10.6)
        iface->neighbor->lastDdIMs = 0;        // Interface MTU từ DD cuối cùng nhận được (Section 10.6)
        iface->neighbor->priority = 0;         // Router Priority của neighbor, học từ Hello của đối phương
    }

    // ===== Area cấu hình (Section 6, Appendix C.2) =====
    // Single-area: toàn bộ interface thuộc backbone area 0.0.0.0.
    area.areaID = 0;
    area.transitCapability = false;
            // Mặc định false vì single-area không cần chuyển tiếp inter-area traffic.
            // Nếu có virtual link hoặc multi-area thì phải set true cho backbone.
    area.externalRoutingCapability = true;
            // Area 0 (backbone) cho phép flood AS-external-LSA (Appendix C.2).
            // Nếu là stub area thì phải set false.
    area.stubDefaultCost = 1;
            // Chi phí của default summary-LSA vào stub area. Chỉ dùng nếu area là stub.
            // Trong single-area backbone, giá trị này không ảnh hưởng.
    area.interfaceIndices.clear();
    for (int i = 0; i < numInterfaces; i++)
        area.interfaceIndices.push_back(i);
            // Ánh xạ: mọi interface đều thuộc area này.
    area.routerLSAs.clear();  // LSDB khởi tạo rỗng, sẽ được điền khi nhận LSA
    area.spfTree.clear();     // SPF tree tính sau khi LSDB đã có dữ liệu

    // ===== Routing table =====
    // Khởi tạo rỗng, sẽ được điền sau khi chạy thuật toán Dijkstra (Section 11).
    routingTable.clear();

}

OspfRouterState::~OspfRouterState()
{
    for (auto& iface : interfaces)
        delete iface.neighbor;
}

// Hàm chuyển enum thành tên
static const char* nbrStateName(int s) {
    switch (s) {
        case NBR_DOWN:     return "Down";
        case NBR_ATTEMPT:  return "Attempt";
        case NBR_INIT:     return "Init";
        case NBR_TWOWAY:   return "2Way";
        case NBR_EXSTART:  return "ExStart";
        case NBR_EXCHANGE: return "Exchange";
        case NBR_LOADING:  return "Loading";
        case NBR_FULL:     return "Full";
        default:           return "?";
    }
}
static const char* ifStateName(int s) {
    switch (s) {
        case IF_DOWN:         return "Down";
        case IF_LOOPBACK:     return "Loopback";
        case IF_WAITING:      return "Waiting";
        case IF_POINTTOPOINT: return "PointToPoint";
        case IF_DROTHER:      return "DROther";
        case IF_BACKUP:       return "Backup";
        case IF_DR:           return "DR";
        default:              return "?";
    }
}

void OspfRouterState::printState()
{
    namespace fs = std::filesystem;
    fs::create_directories("state_dump");

    // Tìm số file lớn nhất trong state_dump/
    int maxNum = 0;
    for (const auto& entry : fs::directory_iterator("state_dump")) {
        std::string name = entry.path().stem().string();
        try { int n = std::stoi(name); if (n > maxNum) maxNum = n; }
        catch (...) {}
    }
    int fileNumber = maxNum + 1;

    // Mở file state_dump/<số>.log
    std::string filename = "state_dump/" + std::to_string(fileNumber) + ".log";
    std::ofstream f(filename);
    if (!f.is_open()) return;

    f << "OSPF State dump #" << fileNumber << "\n";
    f << "Router ID: " << routerID << "\n\n";

    // --- Interfaces ---
    f << "--- Interfaces (" << interfaces.size() << ") ---\n";
    for (size_t i = 0; i < interfaces.size(); i++) {
        const InterfaceData& iface = interfaces[i];
        f << "  [" << i << "] type=" << iface.type
          << " state=" << ifStateName(iface.state)
          << " area=0x" << std::hex << iface.areaID << std::dec
          << " cost=" << iface.cost
          << " helloInt=" << iface.helloInterval
          << " deadInt=" << iface.routerDeadInterval << "\n";
        f << "       ip=0x" << std::hex << iface.ipAddress << std::dec
          << " mask=0x" << std::hex << iface.mask << std::dec
          << " priority=" << (int)iface.routerPriority << "\n";

        // Neighbor đi kèm
        if (iface.neighbor) {
            const NeighborData& nbr = *iface.neighbor;
            f << "       nbr: id=" << nbr.neighborID
              << " state=" << nbrStateName(nbr.state)
              << " priority=" << (int)nbr.priority
              << " master=" << nbr.isMaster
              << " ddSeq=" << nbr.ddSequenceNumber << "\n";
        }
    }

    // --- Area ---
    f << "\n--- Area ---\n";
    f << "  areaID=0x" << std::hex << area.areaID << std::dec
      << " transit=" << area.transitCapability
      << " extRouting=" << area.externalRoutingCapability
      << " stubCost=" << area.stubDefaultCost << "\n";
    f << "  interfaces: ";
    for (int idx : area.interfaceIndices) f << idx << " ";
    f << "\n";
    f << "  LSDB: " << area.routerLSAs.size() << " Router-LSAs\n";
    f << "  SPF tree: " << area.spfTree.size() << " entries\n";

    // --- Routing Table ---
    f << "\n--- Routing Table (" << routingTable.size() << ") ---\n";
    for (size_t i = 0; i < routingTable.size(); i++) {
        const RoutingTableEntry& rte = routingTable[i];
        f << "  [" << i << "] dest=0x" << std::hex << rte.destinationId << std::dec
          << " mask=0x" << std::hex << rte.addressMask << std::dec
          << " pathType=" << (int)rte.pathType
          << " cost=" << rte.cost
          << " nextHopIf=" << rte.nextHopInterfaceIndex
          << " nextHopRtr=0x" << std::hex << rte.nextHopRouterID << std::dec << "\n";
    }

    f.close();
}



