# Qtenv Visualization — Mô phỏng OSPF trực quan

**Ngày:** 2026-06-21
**Phạm vi:** Cải thiện hiển thị Qtenv khi chạy mô phỏng OSPF

## Mục tiêu

Khi chạy mô phỏng trong Qtenv, người xem có thể **nhìn thấy trực tiếp trên sơ đồ topology**:
- Trạng thái neighbor của từng link
- Gói tin đang được trao đổi
- Tiến độ LSDB và SPF của từng router

Không dùng web tool bên ngoài, chỉ dùng API có sẵn của OMNeT++.

## 1. Đổi màu link theo neighbor state

Mỗi link P2P đổi màu dựa trên trạng thái của neighbor state machine tại interface đó.

| Neighbor State | Màu | Ý nghĩa |
|----------------|-----|---------|
| NBR_DOWN | `grey` | Chưa biết neighbor |
| NBR_INIT | `yellow` | Đã thấy neighbor, chưa bidirectional |
| NBR_TWOWAY | `gold` | Bidirectional, chuẩn bị adjacency |
| NBR_EXSTART | `orange` | Đang thương lượng Master/Slave |
| NBR_EXCHANGE | `cyan` | Đang trao đổi DD |
| NBR_LOADING | `blue` | Đang kéo LSA về |
| NBR_FULL | `green` | Adjacency hoàn tất |

**Implement:** Dùng `cGate::getTransmissionChannel()->getDisplayString().setTagArg("ls", 0, color)`.

## 2. Bubble thông báo sự kiện

Hiện `bubble()` trên module router khi có sự kiện quan trọng.

| Sự kiện | Bubble |
|---------|--------|
| Nhận gói tin | `"← Hello (R2)"`, `"← LSU (R4)"`, `"← DD (R3)"`, `"← LSR (R5)"`, `"← LSAck (R8)"` |
| Neighbor state transition | `"R2: Init → 2Way"`, `"R4: ExStart → Exchange"`, `"R5: Full ✓"` |
| SPF hoàn tất | `"SPF: N vertices, M routes"` |
| Link flap | `"Link↓ R3 (blocked)"`, `"Link↑ R3 (restored)"` |

## 3. Text overlay trên module

Dùng `getDisplayString().setTagArg("t", 0, text)` hiển thị text ngay dưới tên router.

```
R1 [2/4]
LSDB:8 SPF:✓
```

- `[full/total]` — số neighbor Full trên tổng số interface
- `LSDB:N` — số LSA trong LSDB
- `SPF:✓` / `SPF:✗` — đã tính SPF hay chưa

## 4. Hàm updateDisplay()

Tất cả logic display gom vào 1 hàm:

```cpp
void routerOspf::updateDisplay() {
    // (a) Cập nhật màu link
    for (size_t i = 0; i < state->interfaces.size(); i++) {
        const char* color = nbrStateColor(state->interfaces[i].neighbor->state);
        cGate *g = gate("gate$o", i);
        if (g->isConnected())
            g->getTransmissionChannel()->getDisplayString().setTagArg("ls", 0, color);
    }
    // (b) Cập nhật text overlay
    char buf[64];
    snprintf(buf, sizeof(buf), "%s [%d/%d]\nLSDB:%d SPF:%s",
             getName(), fullCount, totalInterfaces,
             lsdbCount, spfDone ? "Y" : "N");
    getDisplayString().setTagArg("t", 0, buf);
}
```

Gọi `updateDisplay()` ở:
- Cuối `handleMessage()` (sau khi xử lý xong gói tin)
- Sau mỗi state transition trong `handleMessage()` (kèm bubble)
- Trong `finish()` (trạng thái cuối cùng)

## 5. Bubble gói tin — vị trí đặt

Tất cả bubble đặt trong `handleMessage()`, ngay sau khi parse thành công loại gói tin:

| Packet type | Vị trí | Bubble |
|------------|--------|--------|
| type=1 (Hello) | Sau `processHello()` | `"← Hello (R{routerId})"` |
| type=2 (DD) | Sau `processDD()` | `"← DD (R{routerId})"` |
| type=3 (LSR) | Trước `processLSR()` | `"← LSR (R{routerId})"` |
| type=4 (LSU) | Trước `processLSU()` | `"← LSU (R{routerId})"` |
| type=5 (LSAck) | Trước xử lý LSAck | `"← LSAck (R{routerId})"` |
| SPF done | Sau `calculateSpf()` trong spfTimer | `"SPF: {V} vertices, {R} routes"` |
| Link flap | Trong flapTimer | `"Link↓ R{target}"` / `"Link↑ R{target}"` |
| Inactivity timer | Trong inactivityTimer handler | `"R{nbrId}: Dead"` |

## 6. Hàm helper

- `nbrStateColor(state)` — enum NBR_XXX → string màu
- `countFullNeighbors()` — đếm số neighbor có state == NBR_FULL
- `pktTypeName(type)` — 1→"Hello", 2→"DD", 3→"LSR", 4→"LSU", 5→"LSAck"

## 7. Kiến trúc thay đổi

Chỉ sửa 1 file: `ospf.cc`
- Thêm hàm `updateDisplay()`, `nbrStateColor()`, `countFullNeighbors()`, `pktTypeName()`
- Thêm `#include "ospf.h"` đã có sẵn
- Gọi `updateDisplay()` + `bubble()` tại các vị trí đã liệt kê
- Thêm `updateDisplay()` vào `finish()` (sau khi dọn dẹp timer)

Không sửa struct, không sửa NED, không sửa message classes.
