# Hướng dẫn sử dụng các class OSPF message

File `ospf.msg` → `opp_msgtool` sinh ra `ospf_m.h` / `ospf_m.cc` (~4k dòng).
File này không cần đọc — chỉ cần biết các pattern sau là dùng được.

---

## 1. Danh sách class

Tất cả đều kế thừa `omnetpp::cMessage`, nên gửi qua `send()` như message OMNeT++ bình thường.

```
cMessage
├── headerOspf
├── helloOspf
├── LSAHeaderOspf
├── databaseDescriptionOspf
├── linkStateBodyOspf
├── linkStateRequestOspf
├── linkStateUpdateOspf
└── linkStateAcknowledgementOspf
```

---

## 2. Khởi tạo

| Cách | Ví dụ |
|---|---|
| Chỉ tên | `new helloOspf("Hello-1")` |
| Tên + kind | `new linkStateUpdateOspf("LSU-1", 4)` |
| Mặc định | `new databaseDescriptionOspf` (name = `nullptr`) |

---

## 3. Field kiểu số (uint8, uint16, uint32)

```cpp
auto *h = new headerOspf;
h->setVersion(2);
h->setType(1);        // 1=Hello, 2=DBD, 3=LSR, 4=LSU, 5=LSAck
h->setRouterId(0x01010101);
h->setAreaId(0x00000000);
h->setLength(24);

uint8_t v  = h->getVersion();
uint32_t r = h->getRouterId();
```

---

## 4. Field là object lồng nhau (ví dụ `header` trong `helloOspf`)

**Cách 1 — set cả object (nhiều copy):**
```cpp
headerOspf h;
h.setVersion(2);
h.setType(1);
hello->setHeader(h);
```

**Cách 2 — getForUpdate (khuyên dùng, 0 copy):**
```cpp
hello->getHeaderForUpdate().setVersion(2);
hello->getHeaderForUpdate().setType(1);
hello->getHeaderForUpdate().setRouterId(0x01010101);
```

**Đọc thì chỉ có const:**
```cpp
uint32_t rid = hello->getHeader().getRouterId();
```

---

## 5. Field là mảng động (`[]` trong .msg)

### 5a. Mảng số — `neighborId[]` trong `helloOspf`

```cpp
auto *hello = new helloOspf;

// Thêm 3 neighbor
hello->setNeighborIdArraySize(3);
hello->setNeighborId(0, 0x02020202);
hello->setNeighborId(1, 0x03030303);

// Append (thêm cuối)
hello->appendNeighborId(0x04040404);          // mảng giờ có 4 phần tử

// Insert (chèn vị trí bất kỳ)
hello->insertNeighborId(0, 0x01010101);       // chèn đầu

// Đọc
size_t n = hello->getNeighborIdArraySize();   // → 5
uint32_t id = hello->getNeighborId(2);

// Xóa
hello->eraseNeighborId(1);
```

### 5b. Mảng object — `data[]` trong `linkStateRequestOspf` / `linkStateUpdateOspf` / `linkStateAcknowledgementOspf`

```cpp
auto *lsu = new linkStateUpdateOspf("LSU-1");

// Cấp phát 2 phần tử
lsu->setDataArraySize(2);

// Gán bằng getForUpdate
lsu->getDataForUpdate(0).setLSType(1);
lsu->getDataForUpdate(0).setLinkStateId(0x01010101);
lsu->getDataForUpdate(0).setAdvertisingRouter(0x02020202);

lsu->getDataForUpdate(1).setLSType(3);

// Append cả object
linkStateBodyOspf body;
body.setLSType(5);
lsu->appendData(body);

// Insert
linkStateBodyOspf extra;
extra.setLSType(2);
lsu->insertData(0, extra);

// Xóa
lsu->eraseData(2);
```

---

## 6. Gửi / Nhận

```cpp
// Gửi
helloOspf *hello = new helloOspf("Hello-1");
hello->getHeaderForUpdate().setType(1);
send(hello, "eth$o");

// Nhận
void YourModule::handleMessage(cMessage *msg) {
    if (auto *hello = dynamic_cast<helloOspf *>(msg)) {
        EV << "Nhận Hello từ R" << hello->getHeader().getRouterId() << endl;
        delete hello;
    }
    else if (auto *lsu = dynamic_cast<linkStateUpdateOspf *>(msg)) {
        EV << "Nhận LSU với " << lsu->getDataArraySize() << " LSA" << endl;
        delete lsu;
    }
}
```

---

## 7. Clone message

```cpp
helloOspf *copy = hello->dup();    // tạo bản sao độc lập
```

---

## 8. Cheat sheet nhanh

| Trong .msg | Getter | Setter | Array methods |
|---|---|---|---|
| `uint8_t x` | `getX()` | `setX(v)` | — |
| `uint16_t x` | `getX()` | `setX(v)` | — |
| `uint32_t x` | `getX()` | `setX(v)` | — |
| `headerOspf header` | `getHeader()` (const) | `setHeader(obj)` | — |
| | `getHeaderForUpdate()` (non-const) | | |
| `uint32_t neighborId[]` | `getNeighborId(k)` | `setNeighborId(k, v)` | `setNeighborIdArraySize(n)`, `appendNeighborId(v)`, `insertNeighborId(k, v)`, `eraseNeighborId(k)` |
| `LSAHeaderOspf data` | `getData()` (const) | `setData(obj)` | — |
| | `getDataForUpdate()` (non-const) | | |
| `linkStateBodyOspf data[]` | `getData(k)` (const) | `setData(k, obj)` | `setDataArraySize(n)`, `appendData(obj)`, `insertData(k, obj)`, `eraseData(k)` |
| | `getDataForUpdate(k)` (non-const) | | |
| `uint32_t numberOfLSAs` | `getNumberOfLSAs()` | `setNumberOfLSAs(v)` | — |

> **Mẹo:** Cứ `get<Field>ForUpdate()` khi muốn sửa field là object — vừa nhanh vừa sạch.
> Với mảng thì `append...()` là tiện nhất, không cần quan tâm kích thước.
