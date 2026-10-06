# Kế hoạch hiện thực — ETW Telemetry Collector

Tài liệu này chia toàn bộ công việc thành các **giai đoạn (phase)** và **task** cụ thể.
Mỗi task ghi rõ: mục tiêu, file liên quan, chi tiết kỹ thuật, tiêu chí hoàn thành (DoD),
và phụ thuộc. Trạng thái: ⬜ chưa làm · 🟡 đang làm · ✅ xong.

**Chú giải độ ưu tiên:** P0 = cốt lõi bắt buộc · P1 = quan trọng · P2 = tối ưu/hoàn thiện.

**Thứ tự khuyến nghị:** Phase 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8.
Trong đó Phase 1 (Sensor) và Phase 4 (Graph) là đường găng (critical path).

---

## Phase 0 — Hạ tầng build & khung dự án

Mục tiêu: `cmake --build` chạy xanh với đầy đủ dependency, CI cơ bản hoạt động.

### T0.1 — Cố định baseline vcpkg & cài dependency ✅ (P0)
- **File:** `vcpkg.json`, `CMakeLists.txt`, `src/CMakeLists.txt`
- **Đã làm:** Ghi `builtin-baseline` hợp lệ `b8b8df22...` bằng
  `vcpkg x-update-baseline --add-initial-baseline`. Xác minh 5 package resolve:
  krabsetw 4.3.2, sqlite3 3.53.4, nlohmann-json 3.12.0, stduuid 1.2.3, catch2 3.16.0.
  Đổi `find_package(... QUIET)` + guard `if(TARGET)` → `REQUIRED` + link trực tiếp.
- **Lưu ý phát sinh:** krabsetw là **header-only**, KHÔNG có CMake config `krabs`;
  dùng `find_path(KRABSETW_INCLUDE_DIRS "krabs.hpp" REQUIRED)` +
  `target_include_directories`. VCPKG_ROOT dùng bản vcpkg tích hợp trong VS
  (`...\VC\vcpkg`).
- **DoD:** ✅ `cmake --preset x64-release` configure thành công, build 24/24 object.

### T0.2 — Bật cảnh báo nghiêm & chuẩn C++20 ✅ (P1)
- **File:** `CMakeLists.txt`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`
- **Đã làm:** Interface target `etwc_warnings` với `/W4 /permissive- /EHsc /utf-8`,
  option `ETWC_WARNINGS_AS_ERRORS` (bật `/WX`, dùng ở CI). Áp cho core + exe + tests.
- **DoD:** ✅ Build sạch ở `/W4 /WX` trên toàn bộ code (đã verify).

### T0.3 — CI pipeline (GitHub Actions) ✅ (P1)
- **File:** `.github/workflows/ci.yml`, `CMakePresets.json`
- **Đã làm:** Job `build-and-test` (windows-latest, bootstrap vcpkg, cache x-gha,
  configure `-D ETWC_WARNINGS_AS_ERRORS=ON`, build, ctest) + job `format`
  (`clang-format --dry-run --Werror`). Thêm test preset `x64-release`.
- **DoD:** ✅ Cấu hình xong. (Cần push lên GitHub để thấy chạy xanh thực tế.)

### T0.4 — Khung logging dùng được ✅ (P1)
- **File:** `include/etwc/common/logging.hpp`, `src/common/logging.cpp`
- **Đã làm:** Log kèm timestamp (mili giây, giờ địa phương) + level + thread id,
  xoay vòng file theo kích thước (`max_file_bytes`, giữ N file `.1..N`), macro
  `ETWC_LOG_TRACE/DEBUG/INFO/WARN/ERROR`, `LogConfig`, `log_shutdown()`. Sửa lỗi
  tiềm ẩn dangling (`file_path` chuyển từ `string_view` → `std::string`).
- **DoD:** ✅ Có timestamp + level; ghi có khóa mutex nên đa luồng không xen dòng.

---

## Phase 1 — Lớp cảm biến ETW (Sensor) · đường găng

Mục tiêu: nhận được RawEvent thật từ 4 nhóm provider qua KRABSETW.

### T1.1 — Đấu nối KRABSETW trong EtwSession::Impl ✅ (P0)
- **File:** `include/etwc/sensor/etw_session.hpp`, `src/sensor/etw_session.cpp`
- **Đã làm:** `Impl` chứa `krabs::user_trace` + vector provider. `start()` chạy
  `trace.start()` trên thread riêng (blocking) trong try/catch; `stop()` gọi
  `trace.stop()` an toàn từ thread khác + join. Counter `events_received()` để đo.
- **Lưu ý phát sinh:** Constructor `krabs::provider<>(providerName)` duyệt toàn bộ
  catalog qua **COM/PLA** → cực chậm/treo. Đã chuyển sang khởi tạo **bằng GUID**
  (`krabs::provider<>(krabs::guid(L"{...}"))`).
- **DoD:** ✅ Verify console: 4 provider enable, trace loop chạy, dừng đúng với lỗi
  rõ ràng "Need to be an admin" khi chưa elevated (đúng bản chất ETW). Đo events/s
  > 0 cần chạy **Administrator** (xem README).
- **Phụ thuộc:** T0.1.

### T1.2 — Đăng ký providers + keyword filter ✅ (P0)
- **File:** `include/etwc/sensor/providers.hpp`, `src/sensor/etw_session.cpp`
- **Đã làm:** `configure_providers()` bật đúng 4 provider theo `Config`. GUID +
  keyword bitmask **xác minh bằng `logman query providers`**: Process 0x10;
  File CREATE|READ|WRITE|DELETE_PATH|RENAME|FILENAME; Registry
  SetValue|DeleteValue|CreateKey|DeleteKey; Network IPv4|IPv6. Callback riêng mỗi
  provider gắn `ProviderId` để biết nguồn.
- **DoD:** ✅ Log "enabled 4 providers"; chỉ keyword đã chọn được bật.
- **Phụ thuộc:** T1.1.

### T1.3 — Decode schema event → RawEvent ✅ (P0)
- **File:** `src/sensor/etw_session.cpp`, `include/etwc/sensor/raw_event.hpp`
- **Đã làm:** `on_raw_record` dựng `RawEvent{provider, event_id, opcode, timestamp
  (FILETIME 100ns từ EVENT_HEADER), pid, tid}` + bộ trích property **generic**:
  duyệt `parser.properties()`, switch theo `TDH_IN_TYPE`, `try_parse<T>` rồi đổi
  sang chuỗi UTF-8 (string/int/uint/bool/pointer/filetime). RawEvent thêm
  `ProviderId provider` + `has()`.
- **DoD:** ✅ Build OK; decode chạy trong callback (kiểm chứng đầy đủ cần admin để
  có event thật — thuộc T1.1/T7.3).
- **Phụ thuộc:** T1.2.

### T1.4 — Đọc PEB để bù CommandLine/ImagePath ✅ (P1)
- **File:** `include/etwc/sensor/peb_reader.hpp`, `src/sensor/peb_reader.cpp`
- **Đã làm:** `OpenProcess(QUERY_LIMITED_INFORMATION|VM_READ)` →
  `NtQueryInformationProcess(ProcessBasicInformation)` (winternl.h) → `ReadProcessMemory`
  đọc `PEB.ProcessParameters` → `CommandLine`/`ImagePathName` (UNICODE_STRING) →
  UTF-8. Collector x64 đọc được cả tiến trình WOW64 (PEB native), nên chưa cần
  đường Wow64 riêng.
- **DoD:** ✅ Build + link ntdll OK. Verify runtime với tiến trình thật ở T7.3.
- **Phụ thuộc:** T1.1 (độc lập, đã làm song song).

### T1.5 — Chuẩn hóa đường dẫn NT → DOS ✅ (P1)
- **File:** `include/etwc/common/path_normalizer.hpp`, `src/common/path_normalizer.cpp`
- **Đã làm:** Bảng `\Device\HarddiskVolumeN → X:` qua `GetLogicalDrives` +
  `QueryDosDeviceW`, cache `shared_mutex` (lazy init + `refresh_volume_map`). Xử lý
  `\??\`, DOS path, UNC `\\`; device không map → `nullopt`.
- **DoD:** ✅ 5 unit test pass (strip `\??\`, passthrough DOS/UNC, nullopt cho device
  lạ, map ổ hệ thống). Tổng ctest 9/9 pass.
- **Phụ thuộc:** không.

> **Còn tinh chỉnh cho Phase 2:** event ID → EventKind chính xác (một số ID trong
> `providers.hpp` là dự kiến, sẽ chốt khi có dump event thật ở T2.1); format IPv4/IPv6
> cho địa chỉ mạng; correlate FileKey→tên file cho Read/Write.

---

## Phase 2 — Chuẩn hóa & làm giàu ngữ cảnh (Normalizer)

Mục tiêu: RawEvent → NormalizedEvent đầy đủ định danh, phả hệ, đặc quyền.

### T2.1 — Ánh xạ opcode → EventKind ✅ (P0)
- **File:** `include/etwc/normalizer/normalizer.hpp`, `src/normalizer/normalizer.cpp`
- **Đã làm:** `classify()` tra `(ProviderId, event_id)` → EventKind: Process 1/2,
  File Read/Write/DeletePath/RenamePath/Create, Registry Create/SetValue/Delete,
  Network TCP connect v4/v6. Trả `Unknown` → Normalizer bỏ qua (nullopt).
- **DoD:** ✅ Unit test: event ngoài phạm vi → nullopt; mỗi provider ra đúng kind.
- **Phụ thuộc:** T1.3.

### T2.2 — Trích property theo từng EventKind ✅ (P0)
- **File:** `src/normalizer/normalizer.cpp`
- **Đã làm:** `fill_process/file/registry/network`: pid/ppid, process_name (chuẩn
  hóa DOS), target (file path / registry key\value / IPv4:port), remote_addr/port
  (format IPv4 + đổi byte-order cổng). File Read/Write không mang tên → tra
  **cache FileObject→tên** nạp từ Create/NameCreate. CommandLine bù từ PEB khi thiếu.
  Bộ trích dùng danh sách tên ứng viên (`first_of`) để chịu được khác biệt schema.
- **DoD:** ✅ Unit test cho process/registry/network/file-cache (target đúng, DOS path).
- **Phụ thuộc:** T2.1, T1.4, T1.5.

### T2.3 — Cache phả hệ tiến trình & làm giàu parent_name ✅ (P1)
- **File:** `include/etwc/normalizer/normalizer.hpp`, `src/normalizer/normalizer.cpp`
- **Đã làm:** `std::unordered_map<Pid, ProcessInfo>` nạp khi ProcessCreate;
  `enrich_lineage()` điền `ppid`, `process_name`, `parent_name` cho mọi event từ cache.
- **DoD:** ✅ Unit test chuỗi cha→con: parent_name đúng.
- **Phụ thuộc:** T2.2.
- **Còn lại:** so `start_time` để xử lý PID tái dụng (chốt ở T4.2).

### T2.4 — Gán nhãn đặc quyền (is_system / token_elevated) ✅ (P1)
- **File:** `src/normalizer/normalizer.cpp`
- **Đã làm:** `token_elevated` lấy trực tiếp từ property `ProcessTokenIsElevated` của
  ProcessStart; `is_system` qua `OpenProcessToken`+`TokenUser` so SID `S-1-5-18`
  (best-effort, cache theo pid trong ProcessInfo). Event khác thừa hưởng từ cache.
- **DoD:** ✅ token_elevated verify bằng unit test; is_system cần tiến trình thật
  (kiểm chứng khi chạy admin — T7.3).
- **Phụ thuộc:** T2.2.

### T2.5 — Gán UUID v4 chuẩn ✅ (P2)
- **File:** `include/etwc/common/uuid.hpp`, `src/common/uuid.cpp`
- **Đã làm:** Bản hiện tại (mt19937_64 thread_local, set version/variant) đủ dùng,
  gọi trong `normalize()`. Test format đã có.
- **DoD:** ✅ `test_uuid.cpp` pass.
- **Phụ thuộc:** không.

> **Cần chốt bằng dump event thật (chạy admin, T7.3):** một số event ID (Registry,
> File) và tên property dựa trên manifest tài liệu; code đã phòng thủ bằng
> `first_of` nhiều tên ứng viên nhưng cần đối chiếu thực tế. Ngoài ra: IPv6 address,
> chuẩn hóa registry `\REGISTRY\MACHINE` → `HKLM`.

---

## Phase 3 — Ring Buffer & Lưu trữ SQLite

Mục tiêu: đường ống Producer/Consumer chịu tải cao, lưu vết bền vững.

### T3.1 — Đo & kiểm chứng Ring Buffer dưới tải ✅ (P1)
- **File:** `include/etwc/buffer/ring_buffer.hpp`, `tests/test_ring_buffer.cpp`
- **Đã làm:** Test đa luồng 4 producer × 5000 (buffer nhỏ 64 ép tranh chấp) kiểm
  chứng **không mất phần tử** (đếm + tổng khớp). Bổ sung `pop_for(timeout)` cho
  consumer thức dậy định kỳ (phục vụ flush/prune).
- **DoD:** ✅ Stress test pass, không deadlock/mất phần tử.
- **Còn lại (P2):** chính sách `overwrite` + đệm power-of-two khi cần đo drop rate.

### T3.2 — Mở DB SQLite + schema + PRAGMA ✅ (P0)
- **File:** `include/etwc/storage/sqlite_store.hpp`, `src/storage/sqlite_store.cpp`
- **Đã làm:** `sqlite3_open_v2(READWRITE|CREATE)`, `busy_timeout=5000`, PRAGMA
  `journal_mode=WAL` / `synchronous=NORMAL` / `temp_store=MEMORY`. Bảng `events`
  (13 cột) + index (pid, ts, kind). Tạo thư mục `data/`. Đường dẫn UTF-8. Xử lý
  lỗi mở DB (log + nullptr).
- **DoD:** ✅ Verify runtime: tạo `telemetry.sqlite` + file WAL; mở lại dữ liệu bền.
- **Phụ thuộc:** T0.1.

### T3.3 — Ghi batch bằng prepared statement trong transaction ✅ (P0)
- **File:** `src/storage/sqlite_store.cpp`
- **Đã làm:** `append()` gom `pending`; `flush()` bọc `BEGIN IMMEDIATE…COMMIT`,
  một prepared `INSERT OR IGNORE` (dedup uuid), `bind`/`step`/`reset`/`clear_bindings`
  mỗi row. Flush theo ngưỡng 512 **và** theo timer 1s (trong `consumer_loop`).
  `busy_timeout` xử lý `SQLITE_BUSY`.
- **DoD:** ✅ Unit test: 1500 event (>ngưỡng nhiều lần) → đúng 1500 row; uuid trùng
  bị bỏ qua.
- **Phụ thuộc:** T3.2.

### T3.4 — Vòng đời & flush an toàn khi tắt ✅ (P1)
- **File:** `src/storage/sqlite_store.cpp`, `src/service/collector.cpp`
- **Đã làm:** `close()` flush `pending` + finalize statement + `sqlite3_close_v2`.
  `consumer_loop` thức dậy mỗi 200ms, flush định kỳ 1s, và **rút nốt** phần còn lại
  rồi flush trước khi thread thoát.
- **DoD:** ✅ Unit test mở lại DB thấy đủ dữ liệu; flush định kỳ đảm bảo mất tối đa
  ~1s dữ liệu khi tắt đột ngột.
- **Còn lại (P2):** đối chiếu throughput ≥ 50k ev/s và `integrity_check` ở benchmark T7.2.

---

## Phase 4 — Đồ thị nhân quả BehaviorGraph · đường găng

Mục tiêu: dựng đồ thị provenance streaming đúng và hiệu quả.

### T4.1 — Rà soát & củng cố ingest/adjacency ✅ (P0)
- **File:** `include/etwc/graph/behavior_graph.hpp`, `src/graph/behavior_graph.cpp`
- **Đã làm:** **Dedup cạnh** theo `(src,dst,kind)` qua `edge_index_` → cạnh lặp
  tăng `count` + cập nhật `last_ts` (giảm bão read/write). `last_seen` cập nhật cho
  cả process và resource. Tách `resource_key`/`edge_key`. `Edge` mang `first_ts`,
  `last_ts`, `count`.
- **DoD:** ✅ Test cha→con→file→net (4 nút/3 cạnh); test dedup (5 lần đọc → 1 cạnh).
- **Phụ thuộc:** T2.x.

### T4.2 — Partial Ordering & xử lý PID tái dụng ✅ (P1)
- **File:** `include/etwc/graph/behavior_graph.hpp`, `src/graph/behavior_graph.cpp`
- **Đã làm:** Nút tiến trình quản lý qua `pid_index_` (pid→nút hiện hành), KHÔNG
  dùng `index_`. Mỗi `ProcessCreate` tạo **nút tiến trình mới** và trỏ lại
  `pid_index_` → PID tái dụng cho ra nút tách biệt, không trộn lịch sử. Sự kiện
  tài nguyên dùng nút hiện hành của pid. Thứ tự giữ nguyên theo luồng ETW.
- **DoD:** ✅ Test PID tái dụng → 2 nút tiến trình riêng.
- **Phụ thuộc:** T4.1.

### T4.3 — Xuất đồ thị (phục vụ điều tra) ✅ (P2)
- **File:** `include/etwc/graph/behavior_graph.hpp`, `src/graph/graph_export.cpp`
- **Đã làm:** `to_dot()` (Graphviz, màu/nhãn theo loại nút, nút chết nét đứt, cạnh
  ghi `xN`) và `to_json()` (nlohmann, vertices+edges). Lệnh `--selftest` xuất
  `data/graph.dot` + `data/graph.json` để xem không cần admin.
- **DoD:** ✅ Test DOT/JSON chứa đúng nội dung; verify runtime: `--selftest` ra đồ
  thị 5 nút/4 cạnh render được bằng Graphviz.
- **Phụ thuộc:** T4.1.
- **Còn lại (P2):** truy vấn tổ tiên/hậu duệ; khóa đọc nếu truy cập đa luồng.

---

## Phase 5 — Cắt tỉa đồ thị (Pruner)

Mục tiêu: giữ RAM ổn định 20–40 MB dưới bão log.

### T5.1 — Thu hồi nút chết (Dead Node Purging) ✅ (P0)
- **File:** `include/etwc/graph/behavior_graph.hpp`, `src/graph/behavior_graph.cpp`,
  `src/graph/pruner.cpp`
- **Đã làm:** `BehaviorGraph::remove_vertices()` xóa tập nút + mọi cạnh chạm tới,
  dựng lại `adjacency_`/`edge_index_`/`edge_count_`/`index_`/`pid_index_`.
  `purge_dead_nodes()`: xóa process `alive==false` **trừ khi còn con tiến trình
  sống** (giữ phả hệ tới tiến trình sống); xóa tài nguyên **mồ côi** (không còn
  tiến trình sống sót trỏ tới) — giữ tài nguyên dùng chung với tiến trình sống.
- **DoD:** ✅ 3 unit test: dọn process chết + file mồ côi; giữ cha chết có con sống;
  giữ tài nguyên dùng chung.
- **Phụ thuộc:** T4.1.

### T5.2 — Thu gọn subgraph sạch (Clean Subgraph Collapsing) ✅ (P1)
- **File:** `src/graph/pruner.cpp`
- **Đã làm:** `collapse_clean_subgraphs()`: với tiến trình trong allowlist
  (explorer/svchost/services…, so **basename** không phân biệt hoa thường), nếu số
  tài nguyên vượt cap (64) thì gỡ các tài nguyên **cũ nhất** (theo `last_ts`) —
  chỉ gỡ khi không tiến trình "bẩn" nào dùng chung — và cộng dồn `Vertex.collapsed`.
- **DoD:** ✅ Unit test: svchost ghi 200 file → thu gọn còn ≤ cap+parent, `collapsed`
  tăng. Chỉ tiêu RAM 20–40 MB đo ở benchmark T7.2.
- **Phụ thuộc:** T5.1.

### T5.3 — Lập lịch prune theo Config ✅ (P1)
- **File:** `src/service/collector.cpp`
- **Đã làm:** `consumer_loop` prune theo **`prune_interval_ms`** (timer) **hoặc** khi
  `vertex_count() > soft_vertex_limit`; log DEBUG số nút đã gỡ + kích thước đồ thị.
- **DoD:** ✅ Prune chạy đúng chu kỳ/ngưỡng; nằm trong vòng consumer (không chặn producer).
- **Phụ thuộc:** T5.1.

---

## Phase 6 — Windows Service & cấu hình

Mục tiêu: chạy ổn định như dịch vụ nền, cấu hình linh hoạt.

### T6.1 — Nạp Config từ JSON ⬜ (P1)
- **File:** `include/etwc/common/config.hpp`, `src/common/config.cpp`, `config/collector.json`
- **Chi tiết:** Hiện thực `Config::load()` bằng `nlohmann::json`: đọc file, override
  mặc định, validate (ring_capacity là power-of-two, đường dẫn hợp lệ). Ghi log rõ
  khi file thiếu → dùng defaults. Xác định đường dẫn config tương đối theo thư mục exe.
- **DoD:** Sửa `collector.json` (ví dụ tắt network) → hành vi collector đổi theo.
- **Phụ thuộc:** T0.1.

### T6.2 — Hoàn thiện vòng đời Service (SCM) ⬜ (P1)
- **File:** `src/service/service_main.cpp`, `src/service/service_control.cpp`
- **Chi tiết:** Đã có install/uninstall + control handler. Bổ sung: cấu hình
  recovery (tự khởi động lại khi crash) qua `ChangeServiceConfig2`; chạy dưới tài
  khoản `LocalSystem`; xử lý `SERVICE_CONTROL_SHUTDOWN`; ghi Windows Event Log khi
  start/stop/lỗi. Đặt `dwWaitHint` hợp lý để tránh SCM timeout.
- **DoD:** `sc start/stop` hoạt động mượt; service tự dậy sau khi bị kill.
- **Phụ thuộc:** T1.1.

### T6.3 — Quyền & tiền điều kiện khi khởi động ⬜ (P1)
- **File:** `src/service/collector.cpp`, `src/sensor/etw_session.cpp`
- **Chi tiết:** Kiểm tra & bật đặc quyền cần thiết (SeDebugPrivilege để đọc PEB/token
  tiến trình khác) bằng `AdjustTokenPrivileges`. Báo lỗi rõ ràng nếu thiếu quyền.
- **DoD:** Chạy đúng dưới LocalSystem; đọc được PEB/token của tiến trình user.
- **Phụ thuộc:** T1.4, T2.4.

---

## Phase 7 — Kiểm thử, hiệu năng, độ tin cậy

### T7.1 — Mở rộng unit test ⬜ (P1)
- **File:** `tests/*.cpp`, `tests/CMakeLists.txt`
- **Chi tiết:** Thêm test cho path_normalizer, normalizer (map_opcode, enrich),
  pruner (purge/collapse), config loader. Dùng NormalizedEvent/RawEvent giả lập để
  không phụ thuộc ETW thật.
- **DoD:** Độ phủ các module logic thuần > 70%; `ctest` xanh.

### T7.2 — Benchmark & đo drop rate / RAM ⬜ (P1)
- **File:** `tools/bench/` (tạo mới), tài liệu kết quả trong `docs/`
- **Chi tiết:** Kịch bản sinh tải (mở/đóng tiến trình, ghi file loạt). Đo:
  ETW drop rate (mục tiêu < 0.1% — đọc `EVENT_TRACE_PROPERTIES.EventsLost`),
  RAM RSS của collector (mục tiêu 20–40 MB), throughput event/s, độ trễ end-to-end.
- **DoD:** Báo cáo số liệu đạt mục tiêu thiết kế; có script tái lập.
- **Phụ thuộc:** Phase 1–5 xong.

### T7.3 — Kiểm thử tích hợp end-to-end ⬜ (P1)
- **File:** `tests/integration/` (tạo mới, chạy có điều kiện cần admin)
- **Chi tiết:** Chạy collector ở console, thực thi kịch bản đã biết (spawn tiến trình
  con, ghi file tạm, kết nối localhost), rồi truy vấn SQLite/graph xác nhận sự kiện
  và cạnh nhân quả xuất hiện đúng.
- **DoD:** Test tích hợp pass trên máy có quyền admin.
- **Phụ thuộc:** Phase 1–5.

### T7.4 — Xử lý lỗi & phục hồi ⬜ (P2)
- **File:** toàn cục
- **Chi tiết:** Rà soát các điểm có thể ném/lỗi (mở session, mở DB, hết RAM). Đảm
  bảo không crash service; log + tiếp tục hoặc restart thành phần lỗi. Chống rò rỉ
  handle (RAII cho HANDLE/token).
- **DoD:** Chịu được lỗi tạm thời (DB busy, mất quyền) không sập.

---

## Phase 8 — Đóng gói & tài liệu

### T8.1 — Trình cài đặt & triển khai ⬜ (P2)
- **File:** `tools/`, script đóng gói
- **Chi tiết:** Gói exe + config + phụ thuộc runtime (VC++ redist). Tùy chọn tạo
  MSI/WiX. Hoàn thiện `install-service.ps1` (copy vào `Program Files`, tạo `data/`,
  `logs/`, đặt ACL).
- **DoD:** Cài trên máy sạch (chưa có toolchain) chạy được.

### T8.2 — Tài liệu vận hành & bảo mật ⬜ (P2)
- **File:** `docs/`, `README.md`
- **Chi tiết:** Hướng dẫn cài/gỡ/cấu hình, sơ đồ dữ liệu SQLite, lưu ý quyền, chính
  sách lưu trữ/nén DB, cân nhắc riêng tư (dữ liệu nhạy cảm trong command line).
- **DoD:** Người mới theo tài liệu tự triển khai được.

---

## Ma trận phụ thuộc (rút gọn)

```
T0.1 ─┬─> T1.1 ─> T1.2 ─> T1.3 ─┬─> T2.1 ─> T2.2 ─┬─> T2.3
      │                          │                 └─> T2.4
      ├─> T3.2 ─> T3.3 ─> T3.4   │
      └─> T6.1                   └─> (dữ liệu cho) T4.1 ─> T4.2
T1.4 ──────────────> T2.2                         T4.1 ─> T5.1 ─> T5.2 ─> T5.3
T1.5 ──────────────> T2.2                         T4.1 ─> T4.3
Phase1–5 ─> T7.2 / T7.3
```

## Mốc bàn giao (milestones)

- **M1 — Sensor sống:** T1.1–T1.3 + T2.1 → thấy event thật chảy qua console.
- **M2 — Lưu vết:** + T3.2–T3.4 → sự kiện được ghi SQLite bền vững.
- **M3 — Provenance:** + T2.2–T2.4, T4.1–T4.2 → đồ thị nhân quả đúng.
- **M4 — Ổn định RAM:** + T5.1–T5.3 → chạy dài hạn 20–40 MB.
- **M5 — Dịch vụ hoàn chỉnh:** + Phase 6 → chạy như Windows Service.
- **M6 — Sẵn sàng phát hành:** + Phase 7–8 → đạt chỉ tiêu, có tài liệu & installer.
