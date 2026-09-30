# Tổng kết tiến độ — sau Phase 0, 1, 2

_Cập nhật: 2026-09-30_

Tài liệu này tóm tắt những gì đã hoàn thành cho **ETW Telemetry Collector** sau 3 giai
đoạn đầu (hạ tầng, cảm biến ETW, chuẩn hóa), trạng thái build/test, các quyết định
kỹ thuật & lỗi đã xử lý, và phần còn lại.

---

## 1. Tình trạng tổng quan

| Hạng mục | Trạng thái |
| :--- | :--- |
| Build (MSVC C++20, `/W4 /WX`) | ✅ Xanh |
| Unit test (`ctest`) | ✅ **15/15 pass** |
| clang-format | ✅ Sạch |
| CI (GitHub Actions) | ✅ Đã cấu hình (build + test + format) |
| Pipeline Sensor → Normalizer → Graph | ✅ Nối thông (cần quyền admin để có event thật) |
| Lưu trữ SQLite / Pruner / Config JSON | 🟡 Khung (stub), sẽ làm ở phase sau |

**Đường đi dữ liệu hiện tại:**
```
ETW providers → EtwSession(krabs) → RawEvent → Normalizer → NormalizedEvent
    → RingBuffer → [BehaviorGraph + SqliteStore(stub)] → Pruner(stub)
```

---

## 2. Phase 0 — Hạ tầng build & khung dự án ✅

- **CMake C++20** đa target: thư viện lõi `etwc_core` + exe `etwcollector` + test `etwc_tests`.
- **vcpkg manifest** ([vcpkg.json](../vcpkg.json)), baseline cố định `b8b8df22…`. Dependencies:
  `krabsetw 4.3.2`, `sqlite3`, `nlohmann-json`, `stduuid`, `catch2 3.16`.
- **Cảnh báo nghiêm**: interface `etwc_warnings` (`/W4 /permissive- /EHsc /utf-8`,
  option `ETWC_WARNINGS_AS_ERRORS` bật `/WX`, `/external:W0` cho header thư viện).
- **CI** ([.github/workflows/ci.yml](../.github/workflows/ci.yml)): windows runner →
  `msvc-dev-cmd` (ép MSVC) → cài Ninja (choco) → clone vcpkg full → configure/build/test;
  job `format` chạy `clang-format --dry-run --Werror`.
- **Logging** ([logging.hpp](../include/etwc/common/logging.hpp)): timestamp (ms) + level +
  thread id, xoay vòng file theo kích thước, macro `ETWC_LOG_*`, echo ra console cho
  chế độ `--console`, `SetUnhandledExceptionFilter` ghi lại crash.
- **`scripts/build.ps1`** self-contained: tự dò Visual Studio, vào Dev Shell (cl+ninja),
  set `VCPKG_ROOT`, build — chạy được từ bất kỳ PowerShell nào.

---

## 3. Phase 1 — Lớp cảm biến ETW (Sensor) ✅

- **`EtwSession`** ([etw_session.cpp](../src/sensor/etw_session.cpp)) bọc
  `krabs::user_trace`: khởi tạo trace trên thread riêng, dừng an toàn, đếm
  `events_received()`.
- **4 kernel provider** đăng ký **bằng GUID** + keyword mask (đã xác minh bằng
  `logman query providers`):
  - Kernel-Process `{22FB2CD6-…}` — process start/stop.
  - Kernel-File `{EDD08927-…}` — create/read/write/delete/rename.
  - Kernel-Registry `{70EB4F03-…}` — set/create/delete key & value.
  - Kernel-Network `{7DD42A49-…}` — IPv4/IPv6 (outbound).
- **Decode generic** `EVENT_RECORD → RawEvent`: duyệt `parser.properties()`, chuyển theo
  `TDH_IN_TYPE` sang chuỗi UTF-8; kèm provider/event_id/opcode/pid/tid/timestamp.
- **Đọc PEB** ([peb_reader.cpp](../src/sensor/peb_reader.cpp)):
  `NtQueryInformationProcess` + `ReadProcessMemory` để lấy CommandLine/ImagePath khi
  ETW thiếu.
- **Chuẩn hóa đường dẫn** NT→DOS ([path_normalizer.cpp](../src/common/path_normalizer.cpp)):
  `QueryDosDeviceW` + cache (`shared_mutex`), xử lý `\??\`, UNC.
- **Windows Service** ([service_main.cpp](../src/service/service_main.cpp),
  [service_control.cpp](../src/service/service_control.cpp)): install/uninstall/console,
  vòng đời SCM, in số liệu `events/s`.

### Lỗi đã bắt & sửa (Phase 1)
1. **krabs provider theo tên treo**: constructor `provider(name)` duyệt catalog qua
   COM/PLA rất chậm → chuyển sang khởi tạo **bằng GUID**.
2. **Access violation `0xC0000005`** khi `trace.start()`: providers lưu trong
   `std::vector` bị realloc làm hỏng `reference_wrapper` mà krabs giữ → đổi sang
   **`std::deque`** (địa chỉ phần tử ổn định).

---

## 4. Phase 2 — Chuẩn hóa & làm giàu ngữ cảnh (Normalizer) ✅

- **`classify(ProviderId, event_id) → EventKind`** ([normalizer.cpp](../src/normalizer/normalizer.cpp));
  event ngoài phạm vi → `nullopt` (bỏ qua).
- **Trích property theo nhóm**:
  - Process: pid/ppid, ImageName (DOS), token_elevated.
  - File: FileName/FilePath; Read/Write không mang tên → tra **cache FileObject→tên**
    (nạp từ Create/NameCreate).
  - Registry: `KeyName\ValueName`.
  - Network: `daddr` → IPv4 `a.b.c.d`, `dport` → số cổng (đổi byte-order) → `IP:port`.
- **Cache phả hệ** `pid → ProcessInfo{name, ppid, is_system, token_elevated}`; điền
  `parent_name` cho mọi event.
- **Nhãn đặc quyền**: `token_elevated` từ event; `is_system` qua token SID `S-1-5-18`
  (best-effort, cache theo pid).
- **UUID v4** gán cho mỗi `NormalizedEvent`; CommandLine bù từ PEB khi thiếu.
- Bộ trích dùng `first_of({tên ứng viên…})` để chịu được khác biệt schema.

---

## 5. Kiểm thử (15 unit test)

| Nhóm | Test |
| :--- | :--- |
| RingBuffer | FIFO producer/consumer; close mở khóa consumer |
| UUID | định dạng v4 |
| Path normalizer | strip `\??\`, passthrough DOS/UNC, nullopt cho device lạ, map ổ hệ thống |
| BehaviorGraph | ProcessCreate nối cha→con (nút/cạnh) |
| Normalizer | out-of-scope→nullopt; ProcessCreate; lineage; registry; network IPv4:port; file-name cache |

Chạy: `.\scripts\build.ps1` rồi `ctest --preset x64-release`.

---

## 6. Cấu trúc mã nguồn (ánh xạ pipeline)

| Bước thiết kế | Thành phần | File |
| :--- | :--- | :--- |
| 1. Sensor | EtwSession / providers / PEB | `src/sensor/*` |
| 2. Normalizer | classify + enrich | `src/normalizer/normalizer.cpp` |
| 3. Ring Buffer | template header-only | `include/etwc/buffer/ring_buffer.hpp` |
| 3b. Storage | SqliteStore (stub) | `src/storage/sqlite_store.cpp` |
| 4. BehaviorGraph | adjacency list | `src/graph/behavior_graph.cpp` |
| 5. Pruner | dead-node / collapse (stub) | `src/graph/pruner.cpp` |
| — | Orchestrator / Service | `src/service/*` |

Chi tiết kế hoạch & trạng thái từng task: [implementation-plan.md](implementation-plan.md).

---

## 7. Lưu ý môi trường (Windows 11 Home)

- **Smart App Control (enforced)** chặn `.exe` **chưa ký** → collector bị chặn chạy
  (thoát ngay). Muốn chạy: tắt SAC (không thể bật lại), dùng máy ảo, hoặc ký số.
- ETW kernel session **bắt buộc quyền Administrator**.
- Không admin: collector chạy nhưng log `Need to be an admin`, `events/s=0`.

---

## 8. Còn lại (các phase kế tiếp)

- **Chốt bằng dump event thật (chạy admin)**: xác minh event ID (Registry/File) và tên
  property; format IPv6; chuẩn hóa registry `\REGISTRY\MACHINE` → `HKLM`.
- **Phase 3** — SQLite thật (WAL, prepared statement, batch); đo Ring Buffer/drop rate.
- **Phase 4** — củng cố BehaviorGraph (chống trùng cạnh, PID tái dụng), export DOT/JSON.
- **Phase 5** — Pruner (thu hồi nút chết, thu gọn subgraph) giữ RAM 20–40 MB.
- **Phase 6** — Config JSON, hoàn thiện Windows Service (recovery, event log).
- **Phase 7–8** — benchmark, test tích hợp (admin), đóng gói/installer.
