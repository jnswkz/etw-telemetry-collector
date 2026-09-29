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
- Visual Studio 2022/2026 (MSVC hỗ trợ C++20) — đã kèm CMake + Ninja + vcpkg
- vcpkg (dùng bản tích hợp trong VS hoặc bản riêng)

Đặt `VCPKG_ROOT` trỏ tới vcpkg. Nếu dùng bản tích hợp trong Visual Studio:

```powershell
$env:VCPKG_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
```

Sau đó configure + build + test (lần đầu vcpkg sẽ build krabsetw/sqlite3 ~1–2 phút):

```bash
cmake --preset x64-release
cmake --build --preset x64-release
ctest --preset x64-release --output-on-failure
```

Bật cảnh báo-thành-lỗi (như CI) bằng `-D ETWC_WARNINGS_AS_ERRORS=ON` khi configure.

> Ghi chú: `krabsetw` là thư viện **header-only**, được nạp qua
> `find_path(KRABSETW_INCLUDE_DIRS "krabs.hpp")` chứ không phải `find_package`.

Chạy service ở chế độ console để debug (cần quyền Administrator để mở ETW kernel session):

```bash
.\build\x64-release\src\etwcollector.exe --console
```
