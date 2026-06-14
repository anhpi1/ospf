# Hướng dẫn sử dụng `cSimpleModule`

## 1. `cSimpleModule` là gì?

Là lớp cơ bản nhất để viết một module trong OMNeT++.
Mọi module đều kế thừa `cSimpleModule`.

```cpp
class MyModule : public cSimpleModule
{
  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
};

Define_Module(MyModule);
```

`Define_Module(MyModule)` đăng ký class với OMNeT++ runtime — **bắt buộc** phải có.

---

## 2. KHAI BÁO TRONG .ned

Trước khi dùng, phải khai báo simple module trong file `.ned`:

```
simple MyModule
{
    parameters:
        int myParam;
        double interval @unit(s) = default(1s);
    gates:
        input in;
        output out;
        inout gate[];
}
```

Các kiểu gate:
- `input` — chỉ nhận
- `output` — chỉ gửi
- `inout` — cả hai (thường dùng cho link full-duplex)
- `gate[]` — mảng gate

---

## 3. BA METHOD CÓ SẴN ĐỂ OVERRIDE

Đây là tất cả các method có sẵn trong `cSimpleModule` mà anh có thể override:

| Method | Chạy khi nào | Dùng để |
|---|---|---|
| `initialize()` | một lần khi mô phỏng bắt đầu | đọc tham số, hẹn timer, khởi tạo biến |
| `handleMessage(cMessage *msg)` | mỗi khi có message đến (timer hoặc gói tin) | xử lý tất cả các loại message |
| `finish()` | khi mô phỏng kết thúc | in thống kê (tùy chọn) |

**Không còn method có sẵn nào khác để override.**

### 3a. `initialize()`

```cpp
void MyModule::initialize()
{
    int x = par("myParam");
    int n = gateSize("gate");
    scheduleAt(simTime() + 1.0, new cMessage("timer1"));
}
```

### 3b. `handleMessage(cMessage *msg)`

Tất cả message (timer và gói tin) đều vào đây. Phân biệt bằng `isSelfMessage()`:

```cpp
void MyModule::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // Là timer do scheduleAt() tạo ra
        if (strcmp(msg->getName(), "timerA") == 0) {
            scheduleAt(simTime() + 1.0, msg);   // tái dùng timer
        }
    } else {
        // Là gói tin từ module khác gửi đến
        EV << "Nhận: " << msg->getName() << endl;
        delete msg;
    }
}
```

- `msg->isSelfMessage() == true` → timer
- `msg->isSelfMessage() == false` → gói tin từ bên ngoài

### 3c. `finish()`

```cpp
void MyModule::finish()
{
    EV << "Mô phỏng kết thúc" << endl;
}
```

---

## 4. GỬI — CÓ SẴN

```cpp
cMessage *msg = new cMessage("ten-goi");
send(msg, "out");                  // gửi ra cổng tên "out"
send(msg, "gate", i);              // gửi ra cổng thứ i của mảng "gate[]"
sendDelayed(msg, 0.5, "out");      // gửi sau 0.5 giây
```

---

## 5. TIMER — CÓ SẴN

```cpp
// Hẹn timer
cMessage *timer = new cMessage("myTimer");
scheduleAt(simTime() + 5.0, timer);

// Hủy timer trước hạn
cancelAndDelete(timer);

// Timer lặp — tái dụng chính msg đó
void MyModule::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        // xử lý ...
        scheduleAt(simTime() + 5.0, msg);   // lặp lại sau 5s
    }
}
```

---

## 6. QUY TẮC delete

| Tình huống | Làm gì |
|---|---|
| Nhận gói tin, xử lý xong | `delete msg;` |
| Nhận gói tin, gửi tiếp | `send(msg, ...);` — không delete |
| Timer cháy, lặp lại | `scheduleAt(t, msg);` — không delete |
| Hủy timer đang chờ | `cancelAndDelete(timer);` |
| Gửi ra N cổng | `for(...) send(msg->dup(), ...);` rồi `delete msg;` |

---

## 7. CÁC UTILITY CÓ SẴN

| Method | Chức năng |
|---|---|
| `par("name")` | Đọc tham số từ NED |
| `gateSize("name")` | Kích thước mảng gate |
| `gate("name", i)` | Lấy gate thứ i |
| `send(msg, "name", i)` | Gửi ra gate thứ i |
| `send(msg, "name")` | Gửi ra gate đơn |
| `scheduleAt(time, msg)` | Hẹn timer |
| `cancelAndDelete(timer)` | Hủy timer |
| `simTime()` | Thời gian hiện tại |
| `EV << text` | Ghi log |
| `bubble(text)` | Hiện bong bóng trong GUI |
| `getParentModule()` | Module cha |
| `getSystemModule()` | Network gốc |

---

## 8. CHƯƠNG TRÌNH NGẮN NHẤT CÓ THỂ

```cpp
class MyModule : public cSimpleModule
{
  protected:
    virtual void initialize() override {
        scheduleAt(simTime() + 1.0, new cMessage("self"));
    }
    virtual void handleMessage(cMessage *msg) override {
        send(new cMessage("chao"), "out");
        delete msg;
    }
};
Define_Module(MyModule);
```

1. `initialize()` — hẹn timer 1 giây
2. `handleMessage()` — timer cháy → gửi "chao" ra cổng out → xóa timer

---

**Tóm lại:** `cSimpleModule` chỉ có 3 thứ để nhớ:
- `initialize()` — set up
- `handleMessage()` — xử lý (phân biệt timer bằng `isSelfMessage()`)
- `finish()` — kết thúc
