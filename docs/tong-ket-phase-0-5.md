# Tổng kết tiến độ — sau Phase 0 → 5

_Cập nhật: 2026-10-06_

Tài liệu này tóm tắt những gì đã hoàn thành cho **ETW Telemetry Collector** sau 6 giai
đoạn (hạ tầng, cảm biến ETW, chuẩn hóa, lưu trữ, đồ thị nhân quả, cắt tỉa), trạng thái
build/test, các quyết định kỹ thuật & lỗi đã xử lý, và phần còn lại.

> Bản tóm tắt giai đoạn đầu: [tong-ket-phase-0-2.md](tong-ket-phase-0-2.md).
> Kế hoạch & trạng thái từng task: [implementation-plan.md](implementation-plan.md).

---

## 1. Tình trạng tổng quan

| Hạng mục | Trạng thái |
| :--- | :--- |
| Build (MSVC C++20, `/W4 /WX`) | ✅ Xanh |
| Unit test (`ctest`) | ✅ **27/27 pass** |
| clang-format | ✅ Sạch |
| CI (GitHub Actions) | ✅ build + test + format |
| Pipeline **đầy đủ** (không còn stub) | ✅ Sensor → Normalizer → RingBuffer → Graph + SQLite → Pruner |
| Config JSON / Windows Service hoàn chỉnh | 🟡 Khung (Phase 6) |

**Đường đi dữ liệu hiện tại (đã nối thông toàn bộ):**
```
ETW providers → EtwSession(krabs) → RawEvent → Normalizer → NormalizedEvent
   → RingBuffer(pop_for) → consumer_loop:
        ├─ BehaviorGraph.ingest        (đồ thị nhân quả, dedup cạnh)
        ├─ SqliteStore.append/flush    (WAL, batch, prepared stmt)
        └─ Pruner.run_cycle            (theo timer / ngưỡng mềm)
```

---

## 2. Phase 0 — Hạ tầng build & khung dự án ✅

- **CMake C++20** đa target: `etwc_core` (lib) + `etwcollector` (exe) + `etwc_tests`.
- **vcpkg manifest** ([vcpkg.json](../vcpkg.json)), baseline `b8b8df22…`:
  `krabsetw 4.3.2`, `sqlite3`, `nlohmann-json`, `stduuid`, `catch2 3.16`.
- **Cảnh báo nghiêm** `etwc_warnings` (`/W4 /permissive- /EHsc /utf-8`, `/WX` qua
  option, `/external:W0`).
- **CI** ([ci.yml](../.github/workflows/ci.yml)): `msvc-dev-cmd` → Ninja (choco) →
  clone vcpkg full → configure/build/test + job `clang-format`.
- **Logging**: timestamp(ms)+level+thread id, xoay vòng file, echo console,
  `SetUnhandledExceptionFilter`.
- **`scripts/build.ps1`** self-contained (tự dò VS + Dev Shell + VCPKG_ROOT).

---

## 3. Phase 1 — Lớp cảm biến ETW (Sensor) ✅

- **`EtwSession`** bọc `krabs::user_trace`: trace trên thread riêng, dừng an toàn,
  đếm `events_received()`.
- **4 kernel provider bằng GUID** + keyword (xác minh `logman`): Kernel-Process
  `{22FB2CD6}`, Kernel-File `{EDD08927}`, Kernel-Registry `{70EB4F03}`,
  Kernel-Network `{7DD42A49}`.
- **Decode generic** `EVENT_RECORD → RawEvent` (duyệt property theo `TDH_IN_TYPE` → UTF-8).
- **Đọc PEB** (`NtQueryInformationProcess` + `ReadProcessMemory`) bù CommandLine/ImagePath.
- **Chuẩn hóa path NT→DOS** (`QueryDosDeviceW` + cache), Windows Service skeleton.

**Lỗi đã bắt & sửa:** (1) `provider(name)` treo vì COM/PLA → dùng **GUID**;
(2) AV `0xC0000005` do `std::vector` realloc hỏng `reference_wrapper` krabs giữ →
đổi **`std::deque`**.

---

## 4. Phase 2 — Chuẩn hóa (Normalizer) ✅

- `classify(ProviderId, event_id) → EventKind`; ngoài phạm vi → `nullopt`.
- Trích property theo nhóm: process (pid/ppid/ImageName/elevated), file (cache
  FileObject→tên cho Read/Write), registry (`KeyName\ValueName`), network
  (`daddr`→IPv4, `dport`→cổng → `IP:port`).
- **Cache phả hệ** `pid→ProcessInfo` điền `parent_name`; nhãn đặc quyền
  (`token_elevated` từ event, `is_system` qua token SID `S-1-5-18`).
- Bộ trích `first_of({tên ứng viên})` chịu khác biệt schema.

---

## 5. Phase 3 — Ring Buffer & Lưu trữ SQLite ✅

- **SqliteStore thật** ([sqlite_store.cpp](../src/storage/sqlite_store.cpp)):
  `sqlite3_open_v2`, PRAGMA **WAL**/`synchronous=NORMAL`/`temp_store=MEMORY`,
  `busy_timeout`. Bảng `events` (13 cột) + index (pid, ts, kind).
- **Batch**: `append()` gom `pending`; `flush()` bọc `BEGIN IMMEDIATE…COMMIT` với một
  prepared `INSERT OR IGNORE` (dedup theo uuid). Flush theo ngưỡng 512 **và** timer 1s.
- **Ring Buffer** thêm `pop_for(timeout)` → consumer thức dậy định kỳ để flush/prune;
  `consumer_loop` rút nốt + flush khi dừng (mất tối đa ~1s khi tắt đột ngột).

---

## 6. Phase 4 — Đồ thị nhân quả (BehaviorGraph) ✅

- **Dedup cạnh** `(src,dst,kind)` qua `edge_index_` → cạnh lặp tăng `count` + cập nhật
  `last_ts` (chống bão read/write).
- **PID tái dụng**: nút tiến trình quản lý qua `pid_index_`; mỗi `ProcessCreate` tạo
  **nút mới** → PID tái dụng cho nút tách biệt, không trộn lịch sử.
- **Export** ([graph_export.cpp](../src/graph/graph_export.cpp)): `to_dot()` (Graphviz,
  màu theo loại, nút chết nét đứt, cạnh `xN`) + `to_json()` (nlohmann).
- Lệnh **`--selftest`** bơm 5 event mẫu vào SQLite **và** xuất `data/graph.dot` +
  `data/graph.json` (xem được không cần admin).

---

## 7. Phase 5 — Cắt tỉa đồ thị (Pruner) ✅

- **`BehaviorGraph::remove_vertices()`**: xóa tập nút + mọi cạnh chạm tới, dựng lại
  toàn bộ chỉ mục.
- **Thu hồi nút chết** (`purge_dead_nodes`): xóa process `alive==false` **trừ khi còn
  con tiến trình sống**; xóa tài nguyên **mồ côi** (giữ tài nguyên dùng chung với
  tiến trình sống).
- **Thu gọn subgraph sạch** (`collapse_clean_subgraphs`): tiến trình trong allowlist
  (so basename, không phân biệt hoa thường) vượt cap 64 tài nguyên → gỡ cái cũ nhất
  (theo `last_ts`), chỉ khi không tiến trình "bẩn" dùng chung; cộng dồn `Vertex.collapsed`.
- **Lập lịch**: `consumer_loop` prune theo `prune_interval_ms` **hoặc** vượt
  `soft_vertex_limit` — trong vòng consumer, không chặn producer.

---

## 8. Kiểm thử (27 unit test)

| Nhóm | Số | Nội dung |
| :--- | :--- | :--- |
| RingBuffer | 3 | FIFO, close, **stress đa producer không mất phần tử** |
| UUID | 1 | định dạng v4 |
| Path normalizer | 5 | `\??\`, DOS/UNC, device lạ, map ổ |
| Normalizer | 6 | classify, ProcessCreate, lineage, registry, network, file-name cache |
| SqliteStore | 3 | persist+dedup, batch 1500 row, disabled no-op |
| BehaviorGraph | 5 | cha→con, chuỗi file/net, dedup cạnh, PID tái dụng, export DOT/JSON |
| Pruner | 4 | dọn nút chết+mồ côi, giữ cha có con sống, giữ tài nguyên chung, collapse |

Chạy: `.\scripts\build.ps1` rồi `ctest --preset x64-release`.

---

## 9. Cấu trúc mã nguồn (ánh xạ pipeline)

| Bước thiết kế | Thành phần | File |
| :--- | :--- | :--- |
| 1. Sensor | EtwSession / providers / PEB | `src/sensor/*` |
| 2. Normalizer | classify + enrich + cache | `src/normalizer/normalizer.cpp` |
| 3. Ring Buffer | template + `pop_for` | `include/etwc/buffer/ring_buffer.hpp` |
| 3b. Storage | SqliteStore (WAL, batch) ✅ | `src/storage/sqlite_store.cpp` |
| 4. BehaviorGraph | adjacency + dedup + export ✅ | `src/graph/behavior_graph.cpp`, `graph_export.cpp` |
| 5. Pruner | purge + collapse ✅ | `src/graph/pruner.cpp` |
| — | Orchestrator / Service / selftest | `src/service/*` |

---

## 10. Cách kiểm tra nhanh (không cần admin)

```powershell
.\scripts\build.ps1                                   # build
ctest --preset x64-release                            # 27/27 test

.\build\x64-release\bin\etwcollector.exe --selftest   # bơm dữ liệu mẫu
sqlite3 data\telemetry.sqlite "SELECT kind,pid,process_name,target FROM events;"
dot -Tpng data\graph.dot -o graph.png                 # render đồ thị provenance
```

Dữ liệu thật (cần **Administrator** + vượt Smart App Control):
```powershell
.\build\x64-release\bin\etwcollector.exe --console     # events/s > 0, ghi SQLite + dựng graph
```

---

## 11. Lưu ý môi trường (Windows 11 Home)

- **Smart App Control (enforced)** chặn `.exe` **chưa ký** → thoát ngay. Muốn chạy:
  tắt SAC (không bật lại được), máy ảo, hoặc ký số.
- ETW kernel session **bắt buộc Administrator**. Không admin: log `Need to be an admin`,
  `events/s=0` (nhưng `--selftest` vẫn chạy được).

---

## 12. Còn lại

- **Chốt bằng dump event thật (chạy admin — T7.3):** xác minh event ID (Registry/File)
  & tên property; format IPv6; chuẩn hóa registry `\REGISTRY\MACHINE` → `HKLM`.
- **Phase 6** — nạp Config từ JSON (`nlohmann`), hoàn thiện Windows Service (recovery,
  Windows Event Log, đặc quyền `SeDebugPrivilege`).
- **Phase 7** — benchmark (drop rate < 0.1%, RAM 20–40 MB, throughput), test tích hợp admin.
- **Phase 8** — đóng gói/installer, tài liệu vận hành.
