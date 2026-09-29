# ETW Telemetry Collector

Bộ thu thập nguồn gốc dữ liệu (provenance collector) trên Windows, lấy cảm hứng từ
kiến trúc của **UNICORN** (CamFlow/LSM trên Linux) nhưng chuyển sang hạ tầng
**Event Tracing for Windows (ETW)**.

Service chạy ngầm (Windows Service, C++20) thu thập sự kiện Process / File /
Registry / Network qua ETW, chuẩn hóa, đưa qua Ring Buffer, dựng đồ thị nhân quả
`BehaviorGraph` trong RAM, đồng thời lưu vết xuống SQLite 3 phục vụ điều tra số.

## Kiến trúc (pipeline 5 bước)

```
[ETW Kernel Providers]
        │  raw events
        ▼
   (1) Sensor  ── KRABSETW consumer, đăng ký 4 nhóm provider
        │
        ▼
   (2) Normalizer ── UUID v4, ánh xạ Opcode, làm giàu PPID/token
        │  NormalizedEvent
        ▼
   (3) RingBuffer (65,536 slots, mutex + condition_variable)
        │                         └──► (3b) SqliteStore (lưu trữ kép)
        ▼
   (4) BehaviorGraph ── adjacency list, streaming update, partial ordering
        │
        ▼
   (5) Pruner ── dead node purging + clean subgraph collapsing (RAM 20–40 MB)
```

## Cấu trúc thư mục

```
etw-telemetry-collector/
├── CMakeLists.txt              # Build gốc (C++20)
├── CMakePresets.json
├── vcpkg.json                  # Khai báo dependency (krabsetw, sqlite3, ...)
├── README.md
├── .gitignore
├── .clang-format
│
├── cmake/                      # Module CMake tùy biến (FindKrabsEtw, ...)
│
├── include/etwc/               # Public headers (namespace etwc)
│   ├── common/                 # Types, config, logging, uuid
│   ├── sensor/                 # ETW trace session + providers
│   ├── normalizer/             # NormalizedEvent + Normalizer
│   ├── buffer/                 # RingBuffer
│   ├── storage/                # SqliteStore
│   ├── graph/                  # BehaviorGraph + Pruner
│   └── service/                # Windows Service host
│
├── src/                        # Implementation (.cpp)
│   ├── common/
│   ├── sensor/
│   ├── normalizer/
│   ├── buffer/
│   ├── storage/
│   ├── graph/
│   ├── service/
│   └── main.cpp                # Entry point (service + console mode)
│
├── third_party/               # Thư viện vendored (nếu không dùng vcpkg)
├── tests/                      # Unit test (Catch2/GoogleTest)
├── tools/                      # Script cài đặt service, quản trị
├── config/                     # File cấu hình mặc định
├── scripts/                    # Build/dev scripts
└── docs/                       # Tài liệu thiết kế
```

## Build (yêu cầu)

- Windows 10/11 hoặc Windows Server
- Visual Studio 2022 (MSVC v143, hỗ trợ C++20)
- CMake >= 3.24, vcpkg (khuyến nghị)

```bash
cmake --preset x64-release
cmake --build --preset x64-release
```

Chạy service ở chế độ console để debug (cần quyền Administrator để mở ETW kernel session):

```bash
.\build\x64-release\src\etwcollector.exe --console
```
