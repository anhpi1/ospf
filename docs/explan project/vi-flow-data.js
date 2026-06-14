// Dịch từ flow-data.js sang tiếng Việt
// Dịch tự động bởi translate_flow_data.py
// Model: deepseek-v4-flash | API: https://api.ai-box.vn/v1

const FLOW_DATA = {
  "doc": {
    "title": "Định tuyến trạng thái liên kết OSPFv2 -- Các cơ chế giao thức cốt lõi",
    "desc": "Một giải thích mang tính giáo dục về giao thức định tuyến Open Shortest Path First (OSPF), tập trung vào các cơ chế cốt lõi cho mạng điểm-điểm một vùng. Bao gồm khám phá láng giềng, đồng bộ cơ sở dữ liệu, tràn ngập trạng thái liên kết, tính toán đường đi ngắn nhất và xây dựng bảng định tuyến với hỗ trợ Equal-Cost Multi-Path (ECMP). Nguồn: RFC 2328 (OSPFv2) -- Phạm vi: một vùng Area 0, chỉ các liên kết P2P, Router-LSA (Loại 1) và Network-LSA (Loại 2), không xác thực, không đa vùng, không liên kết ảo, không bầu chọn DR/BDR."
  },
  "mainFlow": [
    {
      "id": "n1",
      "label": "1. Luồng giao thức OSPF năm pha",
      "nodeType": "leaf"
    },
    {
      "id": "n2",
      "label": "2. Khám phá láng giềng: Giao thức Hello",
      "nodeType": "branch"
    },
    {
      "id": "n6",
      "label": "3. Máy trạng thái láng giềng",
      "nodeType": "branch"
    },
    {
      "id": "n9",
      "label": "4. Quá trình trao đổi cơ sở dữ liệu",
      "nodeType": "branch"
    },
    {
      "id": "n14",
      "label": "5. Cơ sở dữ liệu trạng thái liên kết (LSDB) và định dạng LSA",
      "nodeType": "branch"
    },
    {
      "id": "n19",
      "label": "6. Tràn ngập trạng thái liên kết",
      "nodeType": "branch"
    },
    {
      "id": "n23",
      "label": "7. Tính toán SPF (Thuật toán Dijkstra)",
      "nodeType": "branch"
    },
    {
      "id": "n27",
      "label": "8. Xây dựng bảng định tuyến và chuyển tiếp dữ liệu",
      "nodeType": "branch"
    },
    {
      "id": "n30",
      "label": "9. Hành vi hệ thống trong các điều kiện khác nhau",
      "nodeType": "branch"
    },
    {
      "id": "n35",
      "label": "10. Tóm tắt so sánh: Các quyết định thiết kế OSPF",
      "nodeType": "branch"
    }
  ],
  "sections": {
    "n1": {
      "overview": [
        {
          "type": "pre",
          "value": "  [Neighbor       [Database         [LSDB           [SPF             [Routing\n   Discovery] -->  Exchange]  -->     Sync &          Calc]  -->       Table &\n                    |                Flooding]                         Forwarding]\n                    |                                                    |\n                    v                                                    v\n              Master/Slave                                      ECMP-aware\n              Negotiation                                       next-hops"
        },
        {
          "type": "p",
          "value": "OSPF là một Giao thức cổng nội bộ (IGP) trạng thái liên kết, tính toán các tuyến không vòng lặp bằng cách duy trì một cơ sở dữ liệu trạng thái liên kết được đồng bộ trên tất cả các bộ định tuyến trong một vùng. Giải thích này tuân theo năm pha chính của hoạt động OSPF theo thứ tự chúng thực thi khi một bộ định tuyến tham gia mạng và duy trì trạng thái định tuyến."
        },
        {
          "type": "rfc",
          "value": "Tham khảo: §1.1 (Tổng quan giao thức)"
        }
      ],
      "flow": [],
      "nodes": {}
    },
    "n2": {
      "overview": [],
      "flow": [
        {
          "id": "n3",
          "label": "2.1 Tại sao cần cơ chế khám phá chuyên dụng?",
          "nodeType": "leaf"
        },
        {
          "id": "n4",
          "label": "2.2 Cấu trúc gói tin Hello",
          "nodeType": "leaf"
        },
        {
          "id": "n5",
          "label": "2.3 Tương tác giữa Bộ định thời Hello và Bộ định thời Dead",
          "nodeType": "leaf"
        }
      ],
      "nodes": {
        "n3": {
          "content": [
            {
              "type": "p",
              "value": "Các giao thức trạng thái liên kết yêu cầu mỗi bộ định tuyến phải biết láng giềng nào có thể truy cập trước khi bất kỳ dữ liệu định tuyến nào được trao đổi. OSPF sử dụng Giao thức Hello thay vì dựa vào các chỉ báo lớp dưới bởi vì:"
            },
            {
              "type": "ul",
              "items": [
                "<strong>Kiểm tra hai chiều</strong>: Bộ định tuyến phải xác nhận rằng láng giềng có thể nghe thấy nó (giao tiếp hai chiều), không chỉ là nó có thể nghe thấy láng giềng.",
                "<strong>Đàm phán tham số</strong>: Các gói tin Hello mang các tham số giao thức (bộ định thời, ID vùng, khả năng tùy chọn) phải khớp giữa các láng giềng.",
                "<strong>Duy trì kết nối</strong>: Phát hiện lỗi dựa trên bộ định thời Dead cung cấp hội tụ nhanh mà không cần chờ thời gian chờ lớp liên kết."
              ]
            },
            {
              "type": "p",
              "value": "<strong>So sánh: Các phương pháp khám phá láng giềng</strong>"
            },
            {
              "type": "table",
              "headers": [
                "Phương pháp",
                "Cơ chế",
                "Chi phí",
                "Hội tụ",
                "Kiểm tra hai chiều"
              ],
              "rows": [
                [
                  "Giao thức Hello (OSPF)",
                  "Các gói tin Hello multicast/unicast định kỳ",
                  "Trung bình (HelloInterval mặc định 10s)",
                  "Nhanh (RouterDeadInterval = 4x HelloInterval)",
                  "Vốn có (danh sách láng giềng)"
                ],
                [
                  "Chỉ phát hiện lớp 2",
                  "Tín hiệu trạng thái liên kết (sóng mang, duy trì kết nối)",
                  "Không có",
                  "Phụ thuộc vào môi trường vật lý",
                  "Không được cung cấp"
                ],
                [
                  "Cấu hình tĩnh",
                  "Danh sách láng giềng thủ công",
                  "Không chi phí",
                  "Không áp dụng (phải cấu hình lại)",
                  "Không áp dụng"
                ]
              ]
            },
            {
              "type": "p",
              "value": "Giao thức Hello của OSPF đánh đổi chi phí trung bình để có phát hiện láng giềng hai chiều tự động đáng tin cậy và phát hiện lỗi nhanh."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §7.1 (Giao thức Hello), §9.5 (Gửi gói tin Hello)"
            }
          ]
        },
        "n4": {
          "content": [
            {
              "type": "p",
              "value": "Mỗi gói tin Hello chứa:"
            },
            {
              "type": "pre",
              "value": "OSPF Header          (24 bytes)\n  Version            = 2\n  Type               = 1 (Hello)\n  Router ID          = originating router's 32-bit ID\n  Area ID            = area of the sending interface\n  Checksum           = IP one's complement checksum\n  AuType             = authentication type (omitted in scope)\n\nHello-specific fields:\n  Network Mask       = subnet mask of the sending interface\n  HelloInterval      = seconds between Hello packets (configurable)\n  Options            = optional capabilities (E-bit for external routing)\n  Router Priority    = used for DR/BDR election (not used in P2P scope)\n  RouterDeadInterval = seconds before declaring neighbor dead\n  Designated Router  = DR for this network (0.0.0.0 for P2P)\n  Backup Designated Router = BDR for this network (0.0.0.0 for P2P)\n  Neighbor List      = Router IDs of neighbors from which Hello\n                       was recently received"
            },
            {
              "type": "p",
              "value": "Trên các liên kết điểm-điểm, các gói tin Hello được gửi mỗi HelloInterval giây đến địa chỉ multicast AllSPFRouters. Trường Danh sách Láng giềng cho phép kiểm tra hai chiều: một bộ định tuyến coi giao tiếp là hai chiều khi nó thấy Router ID của chính nó trong gói tin Hello nhận được."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §A.3.2 (Định dạng gói tin Hello), §9.5"
            }
          ]
        },
        "n5": {
          "content": [
            {
              "type": "pre",
              "value": "RouterA                          RouterB\n  |                                |\n  |--- Hello(seq, neighbors=[]) -->|  RouterA starts\n  |                                |  RouterB sees HelloA\n  |                                |  -> transition to Init\n  |--- Hello(seq, neighbors=[A])-> |  RouterB lists A\n  |<-- Hello(seq, neighbors=[B])-- |  RouterA sees self listed\n  |                                |  -> transition to 2-Way\n  |  (bidirectional established)   |\n  |                                |\n  |--- Hello (periodic, 1s) -----> |  Keep-alive\n  |<-- Hello (periodic, 1s) ------ |\n  |                                |\n  |     [link failure at t=10s]    |\n  |                                |\n  |     [Dead Timer expires t=14s] |\n  |                     neighbor -> Down"
            },
            {
              "type": "p",
              "value": "Bộ định thời Không hoạt động (độ dài = RouterDeadInterval) được đặt lại mỗi khi nhận được Hello. Nếu không có Hello nào đến trước khi bộ định thời hết hạn, láng giềng được tuyên bố là chết -- không cần tín hiệu bổ sung."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §10.5 (Nhận gói tin Hello)"
            }
          ]
        }
      },
      "subFlows": {}
    },
    "n6": {
      "overview": [],
      "flow": [
        {
          "id": "n7",
          "label": "3.1 Tại sao cần máy trạng thái chính thức?",
          "nodeType": "leaf"
        },
        {
          "id": "n8",
          "label": "3.2 Máy trạng thái (Đơn giản hóa cho P2P)",
          "nodeType": "leaf"
        }
      ],
      "nodes": {
        "n7": {
          "content": [
            {
              "type": "p",
              "value": "OSPF định nghĩa tám trạng thái láng giềng để quản lý quá trình phức tạp của việc thiết lập, duy trì và hủy bỏ quan hệ kề. Mô hình máy trạng thái hữu hạn (FSM) cung cấp:"
            },
            {
              "type": "ul",
              "items": [
                "<strong>Chuyển tiếp xác định</strong>: Mỗi sự kiện ánh xạ chính xác đến một hành vi trên mỗi trạng thái.",
                "<strong>Xử lý lỗi đáng tin cậy</strong>: Mọi điều kiện bất ngờ (không khớp số thứ tự, chỉ nghe thấy một chiều, hết thời gian chờ) buộc phải quay lại trạng thái an toàn.",
                "<strong>Tiến triển từng bước</strong>: Quá trình trao đổi cơ sở dữ liệu chỉ có thể tiếp tục khi các trạng thái tiên quyết đã đạt được."
              ]
            },
            {
              "type": "p",
              "value": "<strong>So sánh: Máy trạng thái so với Bắt tay đơn giản hóa</strong>"
            },
            {
              "type": "table",
              "headers": [
                "Phương pháp",
                "Độ mạnh mẽ",
                "Độ phức tạp",
                "Phục hồi lỗi",
                "Công sức triển khai"
              ],
              "rows": [
                [
                  "FSM láng giềng OSPF đầy đủ (8 trạng thái, 11 sự kiện)",
                  "Cao -- xử lý mọi chế độ lỗi",
                  "Cao (máy phải khớp chính xác với RFC)",
                  "Nhẹ nhàng -- quay lại trạng thái an toàn",
                  "Trung bình (12-15 chuyển tiếp)"
                ],
                [
                  "Hai trạng thái đơn giản (Lên/Xuống)",
                  "Thấp -- không có kiểm soát trao đổi cơ sở dữ liệu",
                  "Thấp",
                  "Không phục hồi từ đồng bộ một phần",
                  "Rất thấp"
                ],
                [
                  "Phương pháp chỉ dùng bộ định thời",
                  "Thấp -- không có phối hợp giao thức",
                  "Thấp",
                  "Mất gói dẫn đến bão truyền lại",
                  "Thấp"
                ]
              ]
            },
            {
              "type": "p",
              "value": "FSM láng giềng là cần thiết để đồng bộ cơ sở dữ liệu chính xác trong toàn vùng."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §10.1 (Các trạng thái láng giềng), §10.3 (Máy trạng thái láng giềng)"
            }
          ]
        },
        "n8": {
          "content": [
            {
              "type": "pre",
              "value": "                    +--------+\n                    |  Down  |\n                    +--------+\n                        |\n              HelloReceived (or Start on NBMA)\n                        |\n                        v\n                    +--------+\n                    |  Init  |\n                    +--------+\n                        |\n              2-WayReceived (router sees self in Hello)\n                        |\n                        v\n                    +---------+\n                    |  2-Way  |\n                    +---------+\n                        |\n                 AdjOK? (decide to become adjacent)\n                        |\n                        v\n                    +---------+\n                    | ExStart | <--------- SeqNumberMismatch\n                    +---------+            (return to ExStart)\n                        |\n                 NegotiationDone (master/slave resolved)\n                        |\n                        v\n                    +----------+\n                    | Exchange |\n                    +----------+\n                        |\n                  ExchangeDone (all DD packets sent/received)\n                        |\n                        v\n                    +---------+\n                    | Loading |\n                    +---------+\n                        |\n                   LoadingDone (request list empty)\n                        |\n                        v\n                    +--------+\n                    |  Full  |\n                    +--------+"
            },
            {
              "type": "p",
              "value": "<strong>Tóm tắt sự kiện:</strong>"
            },
            {
              "type": "table",
              "headers": [
                "Sự kiện",
                "Mô tả",
                "Nguyên nhân điển hình"
              ],
              "rows": [
                [
                  "HelloReceived",
                  "Gói Hello nhận được từ hàng xóm",
                  "Hello định kỳ"
                ],
                [
                  "2-WayReceived",
                  "Router thấy chính nó trong gói Hello của hàng xóm",
                  "Hai chiều được thiết lập"
                ],
                [
                  "NegotiationDone",
                  "Master/slave được phân giải trong ExStart",
                  "Bắt đầu trao đổi DD"
                ],
                [
                  "ExchangeDone",
                  "Hoàn tất chuỗi DD đầy đủ",
                  "Tất cả tiêu đề LSA đã được trao đổi"
                ],
                [
                  "LoadingDone",
                  "Danh sách yêu cầu trạng thái đường liên kết trống",
                  "Tất cả LSA bị thiếu đã được nhận"
                ],
                [
                  "SeqNumberMismatch",
                  "Số thứ tự DD bất ngờ",
                  "Lỗi trong quá trình trao đổi"
                ],
                [
                  "1-Way",
                  "Gói Hello nhận được không có chính nó trong danh sách",
                  "Lỗi đường liên kết, khởi động lại"
                ],
                [
                  "InactivityTimer",
                  "Bộ đếm Dead hết hạn mà không có Hello",
                  "Hàng xóm hoặc đường liên kết xuống"
                ],
                [
                  "KillNbr",
                  "Hủy bỏ hàng xóm bắt buộc",
                  "Giao diện xuống"
                ],
                [
                  "AdjOK?",
                  "Đánh giá lại điều kiện kết nối láng giềng",
                  "Thay đổi DR/BDR (Không áp dụng cho P2P)"
                ]
              ]
            },
            {
              "type": "p",
              "value": "Trên liên kết P2P, mọi hàng xóm đều trở thành kết nối -- không có bầu chọn DR/BDR. Quá trình chuyển từ 2-Way sang ExStart diễn ra vô điều kiện."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §10.3 (Máy trạng thái hàng xóm), §10.4 (Có nên trở thành kết nối láng giềng không)"
            }
          ]
        }
      },
      "subFlows": {}
    },
    "n9": {
      "overview": [],
      "flow": [
        {
          "id": "n10",
          "label": "4.1 Tại sao cần giao thức Master/Slave?",
          "nodeType": "leaf"
        },
        {
          "id": "n11",
          "label": "4.2 Định dạng gói DD",
          "nodeType": "leaf"
        },
        {
          "id": "n12",
          "label": "4.3 Chuỗi trao đổi",
          "nodeType": "leaf"
        },
        {
          "id": "n13",
          "label": "4.4 LSR / LSU / LSAck -- Yêu cầu dữ liệu bị thiếu",
          "nodeType": "leaf"
        }
      ],
      "nodes": {
        "n10": {
          "content": [
            {
              "type": "p",
              "value": "Trao đổi cơ sở dữ liệu là quá trình hai router hàng xóm đồng bộ hóa cơ sở dữ liệu trạng thái đường liên kết của chúng. OSPF sử dụng mô hình thăm dò master/slave thay vì cách tiếp cận gửi tất cả đơn giản hơn vì:"
            },
            {
              "type": "ul",
              "items": [
                "<strong>Phân phối có thứ tự</strong>: Master kiểm soát nhịp độ thông qua số thứ tự, ngăn slave bị quá tải.",
                "<strong>Phục hồi lỗi</strong>: Xác nhận rõ ràng từng gói Mô tả Cơ sở dữ liệu (DD) cho phép truyền lại mà không có sự mơ hồ.",
                "<strong>Trao đổi nguyên tử</strong>: Các tín hiệu M-bit (More) và I-bit (Init) tạo ra ranh giới bắt đầu/kết thúc rõ ràng."
              ]
            },
            {
              "type": "p",
              "value": "<strong>So sánh: Master/Slave vs. Unidirectional Dump</strong>"
            },
            {
              "type": "table",
              "headers": [
                "Phương pháp",
                "Nhịp độ",
                "Phục hồi lỗi",
                "Sử dụng tài nguyên",
                "Theo dõi trạng thái"
              ],
              "rows": [
                [
                  "Master/Slave (OSPF)",
                  "Thăm dò do Master kiểm soát",
                  "Truyền lại theo bộ đếm thời gian hoặc NAK",
                  "Thấp -- một gói chưa xử lý",
                  "Theo dõi chuỗi đầy đủ"
                ],
                [
                  "Unidirectional dump (kiểu RIP)",
                  "Gửi tất cả mục cùng một lúc",
                  "Truyền lại toàn bộ khi hết thời gian",
                  "Cao -- lưu lượng bùng nổ",
                  "Tối thiểu"
                ],
                [
                  "Độc lập hai chiều",
                  "Mỗi bên gửi độc lập",
                  "Truyền lại độc lập",
                  "Trung bình",
                  "Vừa phải"
                ]
              ]
            },
            {
              "type": "p",
              "value": "Phương pháp master/slave đảm bảo rằng cả hai router kết thúc quá trình trao đổi với kiến thức giống hệt nhau về những gì bên kia có và những gì bị thiếu."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §10.8 (Gửi gói Mô tả Cơ sở dữ liệu)"
            }
          ]
        },
        "n11": {
          "content": [
            {
              "type": "pre",
              "value": "OSPF Header              (24 bytes)\n  Type = 2 (Database Description)\n\nDD-specific fields:\n  Interface MTU          = max IP datagram size without fragmentation\n  Options                = optional capabilities (E-bit)\n  Bits:\n    I-bit (Init)         = 1 for first packet in exchange\n    M-bit (More)         = 1 if more DD packets follow\n    MS-bit (Master/Slave)= 1 if sender claims master role\n  DD Sequence Number     = sequence number for ordering\n  LSA Headers[]          = summaries of LSAs in database\n                            (each LSA header: LS type, LS ID,\n                             Advertising Router, LS sequence,\n                             LS age, checksum)"
            }
          ]
        },
        "n12": {
          "content": [
            {
              "type": "pre",
              "value": "RouterA (lower RouterID, becomes Slave)     RouterB (higher RouterID, becomes Master)\n  |                                                   |\n  |  (1) ExStart: Empty DD(I=1,M=1,MS=1,seq=x)      |\n  |-------------------------------------------------->|\n  |                                                   |\n  |  (2) RouterB sees higher RouterID -> asserts      |\n  |      master. Empty DD(I=1,M=1,MS=1,seq=y)         |\n  |<--------------------------------------------------|\n  |                                                   |\n  |  (3) RouterA accepts B as master.                 |\n  |      Empty DD(I=0,M=1,MS=0,seq=y)                 |\n  |-------------------------------------------------->|\n  |                                                   |\n  |  (4) Exchange: DD(seq=y+1, M=1, MS=1,            |\n  |        LSA_hdrs=[H1,H2,H3])                       |\n  |<--------------------------------------------------|\n  |                                                   |\n  |  (5) DD(seq=y+1, M=1, MS=0,                      |\n  |        LSA_hdrs=[H4,H5,H6])                       |\n  |-------------------------------------------------->|\n  |                                                   |\n  |  (6) ... continue until M=0 on both sides ...     |\n  |                                                   |\n  |  (7) DD(seq=y+n, M=0, MS=1, LSA_hdrs=[])         |\n  |<--------------------------------------------------|\n  |                                                   |\n  |  (8) DD(seq=y+n, M=0, MS=0, LSA_hdrs=[])         |\n  |-------------------------------------------------->|\n  |                                                   |\n  |  ExchangeDone -> both routers know what is missing |"
            },
            {
              "type": "p",
              "value": "Trong trạng thái Trao đổi, mỗi router ghi lại LSA nào mà hàng xóm có gần đây hơn. Các LSA này được đưa vào Danh sách yêu cầu trạng thái đường liên kết."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §10.6 (Nhận gói Mô tả Cơ sở dữ liệu), §10.8"
            }
          ]
        },
        "n13": {
          "content": [
            {
              "type": "p",
              "value": "Sau ExchangeDone, mỗi router chuyển sang trạng thái Tải và gửi gói Yêu cầu Trạng thái Đường liên kết (LSR) cho các LSA mà nó cần:"
            },
            {
              "type": "pre",
              "value": "Link State Request Packet:\n  Requests[]:\n    LS type       = type of LSA requested (1 for Router-LSA)\n    Link State ID = identifies the LSA instance\n    Advertising Router = originating router's ID"
            },
            {
              "type": "p",
              "value": "Hàng xóm trả lời bằng các gói Cập nhật Trạng thái Đường liên kết (LSU) chứa các LSA được yêu cầu, được xác nhận bằng các gói Xác nhận Trạng thái Đường liên kết (LSAck):"
            },
            {
              "type": "pre",
              "value": "RouterA (Loading)                RouterB (Full)\n  |                                    |\n  |--- LSR(type=1, id=R1, adv=R1) --> |\n  |<-- LSU(lsas=[RouterLSA_R1]) ------ |\n  |--- LSAck(hdrs=[...]) ------------> |\n  |                                    |\n  |--- LSR(type=1, id=R3, adv=R3) --> |\n  |<-- LSU(lsas=[RouterLSA_R3]) ------ |\n  |--- LSAck(hdrs=[...]) ------------> |\n  |                                    |\n  |  (Request list empty -> LoadingDone)\n  |                                    |\n  |  State -> Full                     |"
            },
            {
              "type": "p",
              "value": "Khi tất cả LSA được yêu cầu đã được nhận, Danh sách yêu cầu trạng thái đường liên kết trở nên trống, kích hoạt LoadingDone, và hàng xóm chuyển sang trạng thái Full."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §10.7 (Nhận gói Yêu cầu Trạng thái Đường liên kết), §10.9 (Gửi gói Yêu cầu Trạng thái Đường liên kết)"
            }
          ]
        }
      },
      "subFlows": {}
    },
    "n14": {
      "overview": [],
      "flow": [
        {
          "id": "n15",
          "label": "5.1 Tại sao cần Cơ sở dữ liệu Trạng thái Đường liên kết?",
          "nodeType": "leaf"
        },
        {
          "id": "n16",
          "label": "5.2 Router-LSA (Loại 1)",
          "nodeType": "leaf"
        },
        {
          "id": "n17",
          "label": "5.3 Network-LSA (Loại 2)",
          "nodeType": "leaf"
        },
        {
          "id": "n18",
          "label": "5.4 LSDB như một Cấu trúc Dữ liệu Bản đồ",
          "nodeType": "leaf"
        }
      ],
      "nodes": {
        "n15": {
          "content": [
            {
              "type": "p",
              "value": "Mọi router trong một vùng OSPF phải tính toán các tuyến đường từ cùng một cấu trúc liên kết. LSDB là cấu trúc dữ liệu được chia sẻ giúp điều này khả thi:"
            },
            {
              "type": "ul",
              "items": [
                "<strong>Bản sao giống hệt</strong>: Flooding đảm bảo tất cả router giữ nội dung LSDB giống hệt nhau cho vùng.",
                "<strong>Biểu diễn đồ thị</strong>: LSDB mô tả một đồ thị có hướng nơi các đỉnh là router và mạng chuyển tiếp, và các cạnh là liên kết với chi phí.",
                "<strong>Khởi tạo cục bộ</strong>: Mỗi router tạo ra một Router-LSA mô tả các giao diện riêng của nó; không router nào mô tả trạng thái của router khác."
              ]
            },
            {
              "type": "p",
              "value": "<strong>So sánh: LSDB vs. Bảng Distance-Vector</strong>"
            },
            {
              "type": "table",
              "headers": [
                "Khía cạnh",
                "Cơ sở dữ liệu trạng thái đường liên kết (OSPF)",
                "Véc-tơ khoảng cách (RIP)"
              ],
              "rows": [
                [
                  "Nội dung lưu trữ",
                  "Toàn bộ cấu trúc liên kết mạng",
                  "Đường đi tốt nhất đến mỗi đích"
                ],
                [
                  "Bộ nhớ trên mỗi bộ định tuyến",
                  "O(V + E) mỗi vùng",
                  "O(N) đích"
                ],
                [
                  "Hội tụ",
                  "Nhanh (flood + SPF)",
                  "Chậm (đếm đến vô cùng)"
                ],
                [
                  "Không vòng lặp",
                  "Vốn có (cây SPF)",
                  "Split horizon / hold-down"
                ],
                [
                  "Lưu lượng giao thức",
                  "Cập nhật LSA được lan truyền",
                  "Đổ toàn bộ bảng định kỳ"
                ]
              ]
            },
            {
              "type": "p",
              "value": "LSDB đánh đổi bộ nhớ để lấy hội tụ nhanh, không vòng lặp -- hệ quả trực tiếp của thuật toán Dijkstra hoạt động trên cấu trúc liên kết toàn cục thay vì Bellman-Ford phân tán."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §2.1 (Biểu diễn bộ định tuyến và mạng), §12.2 (Cơ sở dữ liệu trạng thái đường liên kết)"
            }
          ]
        },
        "n16": {
          "content": [
            {
              "type": "p",
              "value": "Mỗi bộ định tuyến tạo ra một Router-LSA cho mỗi vùng. Trong phạm vi P2P một vùng, mỗi Router-LSA chứa:"
            },
            {
              "type": "pre",
              "value": "RouterLSA {\n    // LSA Header (20 bytes)\n    lsAge            : uint16  = 0 on origination\n    options          : uint8   = E-bit set\n    lsType           : uint8   = 1 (Router-LSA)\n    linkStateID      : uint32  = originating router's Router ID\n    advertisingRouter: uint32  = originating router's Router ID\n    lsSequenceNumber : int32   = initial: 0x80000001\n    lsChecksum       : uint16\n    length           : uint16\n\n    // Body\n    bitV             : bool    = virtual link endpoint (false for P2P)\n    bitE             : bool    = AS boundary router flag\n    bitB             : bool    = area border router flag (false for single area)\n    numLinks         : uint16\n\n    links[]: {\n        linkID       : uint32  = for P2P: neighbor Router ID\n        linkData     : uint32  = for P2P: interface index / IP\n        type         : uint8   = 1 (P2P) or 3 (stub network)\n        numTOS       : uint8   = 0 (TOS not in scope)\n        metric       : uint16  = link cost (1-65535)\n    }\n}"
            },
            {
              "type": "p",
              "value": "Các loại liên kết chính cho mạng P2P:"
            },
            {
              "type": "table",
              "headers": [
                "Loại",
                "Tên",
                "Trường ID Liên kết",
                "Dữ liệu Liên kết",
                "Mục đích"
              ],
              "rows": [
                [
                  "1",
                  "Điểm-điểm",
                  "ID Bộ định tuyến láng giềng",
                  "Chỉ số giao diện",
                  "Kết nối trực tiếp bộ định tuyến-bộ định tuyến"
                ],
                [
                  "3",
                  "Mạng stub",
                  "Mạng/phân mạng IP",
                  "Mặt nạ mạng",
                  "Mạng lá gắn với bộ định tuyến"
                ]
              ]
            },
            {
              "type": "p",
              "value": "Đối với một liên kết P2P giữa các bộ định tuyến R1 và R2, mỗi bộ định tuyến thêm một liên kết Loại 1 trỏ đến ID Bộ định tuyến của bộ kia với chi phí giao diện đã cấu hình. Tùy chọn, nếu liên kết P2P có một phân mạng IP được gán, cả hai bộ định tuyến cũng thêm một liên kết stub Loại 3."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §12.4.1 (Router-LSA), §A.4.2 (Định dạng Router-LSA)"
            }
          ]
        },
        "n17": {
          "content": [
            {
              "type": "p",
              "value": "Trong phạm vi giải thích này (chỉ P2P), Network-LSA không được tạo ra vì chúng đại diện cho các mạng transit quảng bá và NBMA nơi có Bộ định tuyến được chỉ định. Đồ thị P2P chỉ sử dụng các liên kết Router-LSA Loại 1 để kết nối trực tiếp các bộ định tuyến."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §12.4.2 (Network-LSA)"
            }
          ]
        },
        "n18": {
          "content": [
            {
              "type": "pre",
              "value": "LSDB = map<LSAKey, LSA> where\n  LSAKey = (advertisingRouter, lsType, linkStateID)\n\nOperations:\n  installLSA(LSA):   // Compare sequence number; replace if newer\n    if LSA.lsSequenceNumber > existing.lsSequenceNumber:\n      overwrite(LSA)\n      scheduleSPF()\n      return true     // new LSA installed\n    else:\n      return false    // old or duplicate\n\n  lookupLSA(key): LSA?\n  getAllLSAs(): List<RouterLSA>\n  getSequenceNumber(): int32  // increment for self-origination"
            },
            {
              "type": "p",
              "value": "Số thứ tự LSA là số nguyên 32-bit có dấu bắt đầu từ 0x80000001 (InitialSequenceNumber). Mỗi khi bộ định tuyến tạo ra một phiên bản mới của Router-LSA (do thay đổi cấu trúc liên kết), số thứ tự sẽ tăng lên."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §12.1.6 (Số thứ tự LS)"
            }
          ]
        }
      },
      "subFlows": {}
    },
    "n19": {
      "overview": [],
      "flow": [
        {
          "id": "n20",
          "label": "6.1 Tại sao Flooding?",
          "nodeType": "leaf"
        },
        {
          "id": "n21",
          "label": "6.2 Quy trình Flooding",
          "nodeType": "loop"
        },
        {
          "id": "n22",
          "label": "6.3 Lão hóa và MaxAge",
          "nodeType": "leaf"
        }
      ],
      "nodes": {
        "n20": {
          "content": [
            {
              "type": "p",
              "value": "Khi một bộ định tuyến phát hiện thay đổi cấu trúc liên kết (liên kết lên/xuống, thay đổi chi phí), nó phải cập nhật toàn bộ vùng. Flooding đảm bảo:"
            },
            {
              "type": "ul",
              "items": [
                "<strong>Lan truyền tin cậy</strong>: Mọi bộ định tuyến đều nhận được mọi LSA, thông qua xác nhận và truyền lại.",
                "<strong>Hội tụ</strong>: Bản cập nhật được phân phối trong O(đường kính) bước nhảy.",
                "<strong>Độc lập về thứ tự</strong>: Số thứ tự và tuổi đảm bảo thông tin mới nhất thắng bất kể thứ tự phân phối."
              ]
            },
            {
              "type": "p",
              "value": "<strong>So sánh: Flooding so với Cập nhật tập trung</strong>"
            },
            {
              "type": "table",
              "headers": [
                "Phương pháp",
                "Độ tin cậy",
                "Tốc độ hội tụ",
                "Độ phức tạp giao thức",
                "Khả năng mở rộng"
              ],
              "rows": [
                [
                  "Flooding tin cậy (OSPF)",
                  "Rất cao (ACK + truyền lại)",
                  "Nhanh (O(đường kính) bước nhảy)",
                  "Trung bình (phát hiện trùng lặp, ACK)",
                  "O(E) thông báo mỗi sự kiện"
                ],
                [
                  "Máy chủ tập trung",
                  "Điểm lỗi đơn",
                  "Chậm (máy chủ phải tính toán)",
                  "Cao (phối hợp máy chủ)",
                  "O(1) tại biên"
                ],
                [
                  "Gossip / dịch bệnh",
                  "Xác suất",
                  "Có thể điều chỉnh (fan-out)",
                  "Thấp",
                  "O(N log N)"
                ]
              ]
            },
            {
              "type": "p",
              "value": "Flooding là lựa chọn tiêu chuẩn cho các giao thức trạng thái liên kết vì nó cung cấp sự hội tụ nhanh, xác định mà không có bất kỳ điểm lỗi đơn nào."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §13 (Quy trình Flooding)"
            }
          ]
        },
        "n21": {
          "content": [
            {
              "type": "pre",
              "value": "RouterA (detects change)       RouterB                    RouterC\n  |                                |                         |\n  |  (1) Originate new RouterLSA   |                         |\n  |  (2) Update own LSDB           |                         |\n  |  (3) Create LSU(lsas=[LSA])    |                         |\n  |                                |                         |\n  |  (4) Send LSU on all           |                         |\n  |      interfaces except source  |                         |\n  |---------------------------------->|                       |\n  |                                |                         |\n  |                    (5) processLSU():                      |\n  |                     - validate checksum                   |\n  |                     - check if is newer                   |\n  |                       (sequence number comparison)        |\n  |                     - if newer:                           |\n  |                       (a) install in LSDB                 |\n  |                       (b) schedule SPF                    |\n  |                       (c) flood to other interfaces       |\n  |                       (d) send LSAck back to A            |\n  |                                |                         |\n  |<-- LSAck --------------------- |                         |\n  |                                |                         |\n  |                                |  (6) flood further:     |\n  |                                |  LSU(lsas=[LSA])        |\n  |                                |-------------------------->|\n  |                                |                         |\n  |                                |  (7) process, install,  |\n  |                                |      flood, ack         |"
            },
            {
              "type": "p",
              "value": "<strong>Từng bước:</strong>"
            }
          ]
        },
        "n22": {
          "content": [
            {
              "type": "p",
              "value": "Mỗi LSA trong cơ sở dữ liệu có trường <code>lsAge</code>, được tăng lên bởi <code>InfTransDelay</code> (mặc định 1 giây) trên mỗi bước nhảy flooding. Bộ định tuyến cũng tăng tuổi của tất cả các LSA trong cơ sở dữ liệu của nó khi thời gian trôi qua. Khi tuổi đạt đến <code>MaxAge</code> (3600 giây), LSA không còn được sử dụng trong tính toán đường đi và được flood lại để loại bỏ khỏi miền."
            },
            {
              "type": "p",
              "value": "Cơ chế này ngăn các LSA cũ tồn tại vô thời hạn sau khi bộ định tuyến lỗi mà không gửi xóa."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §14 (Lão hóa Cơ sở dữ liệu trạng thái liên kết)"
            }
          ]
        }
      },
      "subFlows": {
        "n21": {
          "flow": [
            {
              "id": "n21-l1",
              "label": "Xác thực LSA",
              "nodeType": "leaf"
            },
            {
              "id": "n21-l2",
              "label": "So sánh với bản sao trong cơ sở dữ liệu",
              "nodeType": "leaf"
            },
            {
              "id": "n21-l3",
              "label": "Nếu LSA nhận được mới hơn",
              "nodeType": "leaf"
            },
            {
              "id": "n21-l4",
              "label": "Nếu LSA nhận được cũ hơn",
              "nodeType": "leaf"
            },
            {
              "id": "n21-l5",
              "label": "Nếu cùng phiên bản",
              "nodeType": "leaf"
            }
          ],
          "nodes": {
            "n21-l1": {
              "content": [
                {
                  "type": "p",
                  "value": "<strong>Xác thực LSA</strong>: Kiểm tra checksum, loại LS, và (đối với AS-external) loại trừ stub area."
                }
              ]
            },
            "n21-l2": {
              "content": [
                {
                  "type": "p",
                  "value": "<strong>So sánh với bản sao trong cơ sở dữ liệu</strong>: Xác định phiên bản nào mới hơn theo thứ tự ưu tiên: LS sequence number -> LS checksum -> LS age (trong giới hạn MaxAgeDiff)."
                }
              ]
            },
            "n21-l3": {
              "content": [
                {
                  "type": "p",
                  "value": "<strong>Nếu LSA nhận được mới hơn</strong>:"
                },
                {
                  "type": "ul",
                  "items": [
                    "Cài đặt LSA vào LSDB (thay thế bản sao cũ).",
                    "Loại bỏ phiên bản cũ khỏi danh sách truyền lại của tất cả các láng giềng.",
                    "Lên lịch tính toán SPF.",
                    "Gửi tràn LSA ra tất cả các giao diện đủ điều kiện (tất cả các giao diện trong area ngoại trừ giao diện nhận được, đối với LSA nội vùng).",
                    "Thêm LSA vào danh sách truyền lại của từng láng giềng.",
                    "Gửi LSAck trở lại láng giềng đã gửi."
                  ]
                }
              ]
            },
            "n21-l4": {
              "content": [
                {
                  "type": "p",
                  "value": "<strong>Nếu LSA nhận được cũ hơn</strong>: Gửi bản sao cục bộ (mới hơn) trở lại người gửi."
                }
              ]
            },
            "n21-l5": {
              "content": [
                {
                  "type": "p",
                  "value": "<strong>Nếu cùng phiên bản</strong>: Coi như xác nhận ngầm; loại bỏ khỏi danh sách truyền lại."
                },
                {
                  "type": "p",
                  "value": "<strong>Phát hiện trùng lặp</strong> ngăn chặn việc gửi tràn vòng lặp: một LSA được nhận từ giao diện mà router đã gửi tràn ra ngoài sẽ bị loại bỏ âm thầm."
                },
                {
                  "type": "rfc",
                  "value": "Tham khảo: §13 (Quy trình gửi tràn), §13.1 (Xác định LSA nào mới hơn)"
                }
              ]
            }
          },
          "subFlows": {}
        }
      }
    },
    "n23": {
      "overview": [],
      "flow": [
        {
          "id": "n24",
          "label": "7.1 Tại sao lại là Dijkstra?",
          "nodeType": "leaf"
        },
        {
          "id": "n25",
          "label": "7.2 Thuật toán (Dijkstra hai giai đoạn)",
          "nodeType": "leaf"
        },
        {
          "id": "n26",
          "label": "7.3 ECMP: Đa đường có chi phí bằng nhau",
          "nodeType": "leaf"
        }
      ],
      "nodes": {
        "n24": {
          "content": [
            {
              "type": "p",
              "value": "OSPF sử dụng thuật toán đường đi ngắn nhất (SPF) của Dijkstra để tính cây đường đi ngắn nhất. Sự lựa chọn này so với các giải pháp thay thế là có chủ đích:"
            },
            {
              "type": "p",
              "value": "<strong>So sánh: Các thuật toán tính toán đường đi SPF</strong>"
            },
            {
              "type": "table",
              "headers": [
                "Thuật toán",
                "Độ phức tạp",
                "Không vòng lặp",
                "Hỗ trợ ECMP",
                "Cập nhật gia tăng"
              ],
              "rows": [
                [
                  "Dijkstra (OSPF)",
                  "O((V+E) log V)",
                  "Vốn có",
                  "Có (danh sách tiền nhiệm)",
                  "Hạn chế (SPF một phần [Ref1])"
                ],
                [
                  "Bellman-Ford (RIP)",
                  "O(V * E)",
                  "Không (đếm đến vô hạn)",
                  "Khó",
                  "Có (lan tỏa)"
                ],
                [
                  "Floyd-Warshall",
                  "O(V^3)",
                  "Vốn có",
                  "Có",
                  "Tính toán lại toàn bộ"
                ]
              ]
            },
            {
              "type": "p",
              "value": "Dijkstra là tối ưu cho các giao thức trạng thái đường liên kết vì: (a) nó tạo ra một cây không vòng lặp có gốc tại router tính toán, (b) nó xử lý chi phí liên kết dương một cách hiệu quả với hàng đợi ưu tiên, và (c) nó hỗ trợ ECMP một cách tự nhiên bằng cách tích lũy nhiều tiền nhiệm có chi phí bằng nhau."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §16.1 (Tính toán cây đường đi ngắn nhất cho một area)"
            }
          ]
        },
        "n25": {
          "content": [
            {
              "type": "p",
              "value": "<strong>Giai đoạn 1: Xây dựng cây đường đi ngắn nhất của các đỉnh chuyển tiếp (chỉ các router, trong phạm vi P2P)</strong>"
            },
            {
              "type": "pre",
              "value": "// Input:  LSDB (all Router-LSAs in area)\n// Output: shortest-path tree: dist[], nextHop[]\n\n// Initialize\nfor each router R in area:\n    dist[R] = INFINITY\n    nextHop[R] = []\n    candidateList = empty priority queue\n\ndist[self] = 0\ncandidateList.insert(self, dist=0)\n\n// Main loop\nwhile candidateList is not empty:\n    V = candidateList.extractMin()   // vertex closest to root\n    add V to shortest-path tree\n\n    for each link in V's RouterLSA:\n        if link.type != 1:               // skip non-P2P links\n            continue\n        W = link.neighborRouterID        // adjacent router\n        if W is already on tree:\n            continue\n\n        // Verify W has a link back to V (bidirectional check)\n        if W's RouterLSA does not contain link back to V:\n            continue\n\n        newDist = dist[V] + link.metric\n\n        if W not in candidateList:\n            candidateList.insert(W, newDist)\n            nextHop[W] = computeNextHop(parent=V)\n        else if newDist < candidateList.dist[W]:\n            candidateList.decreaseKey(W, newDist)\n            nextHop[W] = computeNextHop(parent=V)\n        else if newDist == candidateList.dist[W]:\n            // Equal-cost path -- ECMP!\n            nextHop[W].append(computeNextHop(parent=V))"
            },
            {
              "type": "p",
              "value": "<strong>Giai đoạn 2: Thêm các stub network làm lá</strong>"
            },
            {
              "type": "pre",
              "value": "for each router V in shortest-path tree:\n    for each link in V's RouterLSA:\n        if link.type == 3:                 // stub network link\n            network = link.linkID\n            netDist = dist[V] + link.metric\n\n            if netDist < currentBestCost[network]:\n                installRoute(network, netDist, nextHop[V])\n            else if netDist == currentBestCost[network]:\n                appendNextHops(network, nextHop[V])"
            }
          ]
        },
        "n26": {
          "content": [
            {
              "type": "p",
              "value": "Khi bước nới lỏng tìm thấy <code>newDist == dist[W]</code>, tiền nhiệm mới được thêm vào danh sách thay vì thay thế cái hiện tại. Điều này tích lũy tất cả các đường có chi phí bằng nhau:"
            },
            {
              "type": "pre",
              "value": "Example:  R1 with cost 5 to R3\n          R2 with cost 5 to R3\n          dist[R3] = 5\n          nextHop[R3] = [R1, R2]    // ECMP: both next-hops used"
            },
            {
              "type": "p",
              "value": "Ở cấp bảng định tuyến, một mục ECMP mang nhiều next-hop:"
            },
            {
              "type": "pre",
              "value": "RoutingEntry:\n  destination = 10.0.0.10\n  cost        = 12\n  nextHops    = [{interface: ppg[0], neighbor: R2},\n                 {interface: ppg[1], neighbor: R5}]\n  isECMP      = true"
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §16.1.1 (Tính toán next hop), §16.8 (Đa đường chi phí bằng nhau)"
            }
          ]
        }
      },
      "subFlows": {}
    },
    "n27": {
      "overview": [],
      "flow": [
        {
          "id": "n28",
          "label": "8.1 Cấu trúc bảng định tuyến",
          "nodeType": "leaf"
        },
        {
          "id": "n29",
          "label": "8.2 Chuyển tiếp dữ liệu",
          "nodeType": "leaf"
        }
      ],
      "nodes": {
        "n28": {
          "content": [
            {
              "type": "p",
              "value": "Bảng định tuyến là đầu ra của tính toán SPF. Mỗi mục chứa:"
            },
            {
              "type": "pre",
              "value": "RoutingEntry {\n    destination      : RouterID      // destination router\n    cost             : uint16        // total path cost from self\n    nextHops         : List<NextHop> // can be multiple for ECMP\n    isECMP           : bool          // true if 2+ equal-cost paths exist\n}\n\nNextHop {\n    interfaceID      : int           // outgoing interface index\n    neighborRouterID : RouterID      // next-hop router\n}"
            },
            {
              "type": "p",
              "value": "Bảng định tuyến được xây dựng lại từ đầu sau mỗi lần tính toán SPF. Đối với một mạng P2P đơn vùng với 10 router, bảng chứa tối đa 9 đích (tất cả các router khác)."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §11 (Cấu trúc bảng định tuyến)"
            }
          ]
        },
        "n29": {
          "content": [
            {
              "type": "p",
              "value": "Khi một gói dữ liệu đến:"
            },
            {
              "type": "pre",
              "value": "forwardData(packet):\n    dest = packet.destinationRouterID\n    entry = routingTable.lookup(dest)\n\n    if entry is null:\n        discard(packet)               // no route\n        return\n\n    if entry.isECMP:\n        // Select next-hop (round-robin or hash-based)\n        nextHop = entry.nextHops[roundRobinCounter % entry.nextHops.length]\n        roundRobinCounter++\n    else:\n        nextHop = entry.nextHops[0]\n\n    send(packet, nextHop.interfaceID)"
            },
            {
              "type": "p",
              "value": "Phân phối ECMP có thể sử dụng round-robin, băm theo luồng, hoặc lựa chọn ngẫu nhiên. Bảng định tuyến lưu trữ tất cả các next-hop có chi phí bằng nhau; hành vi chuyển tiếp là một quyết định chính sách cục bộ."
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §11.1 (Tra cứu bảng định tuyến)"
            }
          ]
        }
      },
      "subFlows": {}
    },
    "n30": {
      "overview": [],
      "flow": [
        {
          "id": "n31",
          "label": "9.1 Trạng thái ổn định",
          "nodeType": "leaf"
        },
        {
          "id": "n32",
          "label": "9.2 Lỗi liên kết",
          "nodeType": "leaf"
        },
        {
          "id": "n33",
          "label": "9.3 Phục hồi liên kết",
          "nodeType": "leaf"
        },
        {
          "id": "n34",
          "label": "9.4 Nhiều lỗi đồng thời",
          "nodeType": "leaf"
        }
      ],
      "nodes": {
        "n31": {
          "content": [
            {
              "type": "p",
              "value": "Trong trạng thái ổn định, tất cả các router ở trạng thái láng giềng Full với tất cả các láng giềng P2P kề cận. Các gói Hello được trao đổi mỗi HelloInterval giây để duy trì quan hệ láng giềng. LSDB giống nhau trên toàn area, và cây SPF ổn định. Không có bản cập nhật LSA nào được tạo ra ngoại trừ làm mới Router-LSA định kỳ (mỗi LSRefreshTime, ví dụ: 1800 giây giảm xuống còn 180 giây trong các mô phỏng tăng tốc)."
            },
            {
              "type": "pre",
              "value": "All routers:\n  State: Full (all neighbors)\n  LSDB:  synchronized\n  SPF:   idle (no calculation needed)\n  Traffic: periodic Hello packets only"
            }
          ]
        },
        "n32": {
          "content": [
            {
              "type": "p",
              "value": "Khi một liên kết P2P bị lỗi:"
            },
            {
              "type": "p",
              "value": "1. Các router ở hai đầu phát hiện lỗi thông qua hết hạn Inactivity Timer (không nhận được Hello trong RouterDeadInterval giây)."
            },
            {
              "type": "p",
              "value": "2. Láng giềng chuyển sang trạng thái Down."
            },
            {
              "type": "p",
              "value": "3. Mỗi router khởi tạo một Router-LSA mới phản ánh liên kết bị mất, với số sequence tăng lên."
            },
            {
              "type": "p",
              "value": "4. LSA mới được gửi tràn khắp area."
            },
            {
              "type": "p",
              "value": "5. Mọi router nhận được cài đặt LSA mới, lên lịch SPF."
            },
            {
              "type": "p",
              "value": "6. SPF tính toán lại cây đường đi ngắn nhất, bỏ qua liên kết bị lỗi."
            },
            {
              "type": "p",
              "value": "7. Bảng định tuyến được cập nhật."
            },
            {
              "type": "p",
              "value": "<strong>Mốc thời gian hội tụ (mô phỏng, bộ định thời tăng tốc):</strong>"
            },
            {
              "type": "table",
              "headers": [
                "Sự kiện",
                "Thời gian"
              ],
              "rows": [
                [
                  "Liên kết đứt",
                  "T = 20.0s"
                ],
                [
                  "Inactivity Timer kích hoạt (DeadInterval = 4s sau Hello cuối cùng)",
                  "T = ~21.0s đến ~24.0s"
                ],
                [
                  "Router-LSA mới được khởi tạo và gửi tràn",
                  "T + ~0.01s"
                ],
                [
                  "SPF được lên lịch (SPFDelay = 0.5s)",
                  "T + ~0.51s"
                ],
                [
                  "Bảng định tuyến được cập nhật",
                  "T + ~0,52s"
                ]
              ]
            },
            {
              "type": "rfc",
              "value": "Tham khảo: §16.7 (Các sự kiện được sinh ra do thay đổi bảng định tuyến)"
            }
          ]
        },
        "n33": {
          "content": [
            {
              "type": "p",
              "value": "Khi một liên kết bị lỗi được khôi phục:"
            },
            {
              "type": "p",
              "value": "1. Các gói Hello bắt đầu lưu thông trở lại."
            },
            {
              "type": "p",
              "value": "2. Trạng thái láng giềng tiến triển Down -> Init -> 2-Way -> ExStart -> Exchange -> Loading -> Full."
            },
            {
              "type": "p",
              "value": "3. Trong quá trình Exchange/Loading, hai bộ định tuyến đồng bộ hóa cơ sở dữ liệu của chúng (có thể đã bị phân kỳ)."
            },
            {
              "type": "p",
              "value": "4. Mỗi bộ định tuyến tạo một Router-LSA mới với liên kết đã được khôi phục."
            },
            {
              "type": "p",
              "value": "5. Quá trình Flooding lan truyền bản cập nhật; tất cả các bộ định tuyến tính toán lại SPF."
            },
            {
              "type": "p",
              "value": "6. Bảng định tuyến hội tụ về cấu trúc liên kết mới."
            }
          ]
        },
        "n34": {
          "content": [
            {
              "type": "p",
              "value": "Trong kịch bản đa lỗi (ví dụ, hai liên kết backbone cùng lúc bị lỗi):"
            },
            {
              "type": "p",
              "value": "1. Mỗi lỗi độc lập kích hoạt hết thời gian Inactivity Timer và tạo Router-LSA mới."
            },
            {
              "type": "p",
              "value": "2. Flooding có thể mang cả hai LSA đồng thời; mỗi bộ định tuyến xử lý chúng một cách độc lập theo thứ tự nhận."
            },
            {
              "type": "p",
              "value": "3. SPF tính toán lại sau mỗi lần cài đặt LSA (trừ khi được gộp lại bởi SPFDelay)."
            },
            {
              "type": "p",
              "value": "4. Bảng định tuyến hội tụ về các đường dẫn khả dụng tốt nhất dựa trên kết nối còn lại."
            },
            {
              "type": "p",
              "value": "Nếu các lỗi phân vùng mạng, một số đích trở nên không thể truy cập -- bảng định tuyến sẽ không có mục nhập cho các bộ định tuyến đó, và các gói dữ liệu bị loại bỏ."
            }
          ]
        }
      },
      "subFlows": {}
    },
    "n35": {
      "overview": [],
      "flow": [
        {
          "id": "n36",
          "label": "10.1 Tại sao Link-State lại hơn Distance-Vector",
          "nodeType": "leaf"
        },
        {
          "id": "n37",
          "label": "10.2 Tại sao Reliable Flooding lại hơn Gossip",
          "nodeType": "leaf"
        }
      ],
      "nodes": {
        "n36": {
          "content": [
            {
              "type": "table",
              "headers": [
                "Quyết định",
                "OSPF (Link-State)",
                "Giải pháp thay thế (Distance-Vector)"
              ],
              "rows": [
                [
                  "Kiến thức về cấu trúc liên kết",
                  "Cấu trúc liên kết mạng đầy đủ",
                  "Chỉ bước nhảy tiếp theo cho mỗi đích"
                ],
                [
                  "Tốc độ hội tụ",
                  "Nhanh (flood + tính toán)",
                  "Chậm (Bellman-Ford qua các bước nhảy)"
                ],
                [
                  "Ngăn chặn vòng lặp",
                  "Tự nhiên (cây SPF)",
                  "Split horizon, hold-down, poison reverse"
                ],
                [
                  "Lưu lượng giao thức",
                  "LSA flooding theo sự kiện",
                  "Đổ toàn bộ bảng định kỳ"
                ],
                [
                  "Linh hoạt về metric",
                  "Tùy ý (chi phí 16-bit)",
                  "Hop count (RIP) hoặc composite (IGRP)"
                ],
                [
                  "Khả năng mở rộng",
                  "Các khu vực phân cấp",
                  "Bảng định tuyến đầy đủ trên mỗi bộ định tuyến"
                ]
              ]
            }
          ]
        },
        "n37": {
          "content": [
            {
              "type": "table",
              "headers": [
                "Quyết định",
                "Flooding trong OSPF",
                "Giải pháp thay thế (Gossip/Epidemic)"
              ],
              "rows": [
                [
                  "Đảm bảo phân phối",
                  "Xác nhận rõ ràng (ACK) + truyền lại",
                  "Fan-out xác suất"
                ],
                [
                  "Tính xác định hội tụ",
                  "O(diameter) được đảm bảo",
                  "Xác suất có thể điều chỉnh"
                ],
                [
                  "Chi phí bản tin (Overhead)",
                  "O(E) mỗi sự kiện",
                  "O(N log N)"
                ],
                [
                  "Xử lý trùng lặp",
                  "Dựa trên số thứ tự + age",
                  "Time-to-live hoặc bloom filters"
                ],
                [
                  "Khả năng chịu lỗi",
                  "Chịu được N-1 lỗi bộ định tuyến",
                  "Cao (đường dẫn dự phòng)"
                ]
              ]
            }
          ]
        }
      },
      "subFlows": {}
    }
  },
  "appendix": {
    "rfcTable": {
      "title": "RFC 2328 Tham chiếu",
      "rows": [
        [
          "Tổng quan và kiến trúc giao thức",
          "Mục 1.1"
        ],
        [
          "Biểu diễn đồ thị của mạng",
          "Mục 2.1"
        ],
        [
          "Đa đường chi phí bằng nhau",
          "Mục 2.4, §16.8"
        ],
        [
          "Giao thức Hello",
          "Mục 7.1, §9.5"
        ],
        [
          "Trạng thái giao diện",
          "Mục 9.1"
        ],
        [
          "Trạng thái láng giềng",
          "Mục 10.1"
        ],
        [
          "Máy trạng thái láng giềng (chi tiết)",
          "Mục 10.3"
        ],
        [
          "Nhận các gói Hello",
          "Mục 10.5"
        ],
        [
          "Nhận các gói DD",
          "Mục 10.6"
        ],
        [
          "Gửi các gói DD",
          "Mục 10.8"
        ],
        [
          "Nhận các gói LSR",
          "Mục 10.7"
        ],
        [
          "Xử lý Link State Update (flooding)",
          "Mục 13"
        ],
        [
          "Xác định LSA mới hơn",
          "Mục 13.1"
        ],
        [
          "Cài đặt LSA vào cơ sở dữ liệu",
          "Mục 13.2"
        ],
        [
          "Quy trình lan truyền bước tiếp theo",
          "Mục 13.3"
        ],
        [
          "Xác nhận trạng thái liên kết",
          "Mục 13.5"
        ],
        [
          "Lão hóa LSA",
          "Mục 14"
        ],
        [
          "Định dạng tiêu đề gói OSPF",
          "Mục A.3.1"
        ],
        [
          "Định dạng gói Hello",
          "Mục A.3.2"
        ],
        [
          "Định dạng gói DD",
          "Mục A.3.3"
        ],
        [
          "Định dạng gói LSR",
          "Mục A.3.4"
        ],
        [
          "Định dạng gói LSU",
          "Mục A.3.5"
        ],
        [
          "Định dạng gói LSAck",
          "Mục A.3.6"
        ],
        [
          "Định dạng Router-LSA (Loại 1)",
          "Mục A.4.2"
        ],
        [
          "Định dạng Network-LSA (Loại 2)",
          "Mục A.4.3"
        ],
        [
          "Tính toán cây đường đi ngắn nhất (Dijkstra)",
          "Mục 16.1"
        ],
        [
          "Tính toán bước nhảy tiếp theo",
          "Mục 16.1.1"
        ],
        [
          "Cấu trúc bảng định tuyến",
          "Mục 11"
        ],
        [
          "Bộ định thời có thể cấu hình",
          "Phụ lục C"
        ]
      ]
    }
  }
};
