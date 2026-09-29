# Kiến trúc ETW Telemetry Collector

Tài liệu này ánh xạ thiết kế (UNICORN → ETW) sang mã nguồn thực tế.

## Pipeline & vị trí mã nguồn

| Bước | Thành phần | Header | Nguồn |
| :--- | :--- | :--- | :--- |
| 1 | Sensor (KRABSETW) | [`sensor/etw_session.hpp`](../include/etwc/sensor/etw_session.hpp) | [`src/sensor/etw_session.cpp`](../src/sensor/etw_session.cpp) |
| 1 | Provider registry | [`sensor/providers.hpp`](../include/etwc/sensor/providers.hpp) | `src/sensor/providers.cpp` |
| 1 | PEB command-line fallback | [`sensor/peb_reader.hpp`](../include/etwc/sensor/peb_reader.hpp) | `src/sensor/peb_reader.cpp` |
| 2 | Normalizer | [`normalizer/normalizer.hpp`](../include/etwc/normalizer/normalizer.hpp) | `src/normalizer/normalizer.cpp` |
| 2 | NormalizedEvent | [`normalizer/normalized_event.hpp`](../include/etwc/normalizer/normalized_event.hpp) | — |
| 3 | Ring Buffer | [`buffer/ring_buffer.hpp`](../include/etwc/buffer/ring_buffer.hpp) | header-only |
| 3b | SQLite store | [`storage/sqlite_store.hpp`](../include/etwc/storage/sqlite_store.hpp) | `src/storage/sqlite_store.cpp` |
| 4 | BehaviorGraph | [`graph/behavior_graph.hpp`](../include/etwc/graph/behavior_graph.hpp) | `src/graph/behavior_graph.cpp` |
| 5 | Pruner | [`graph/pruner.hpp`](../include/etwc/graph/pruner.hpp) | `src/graph/pruner.cpp` |
| — | Orchestrator | [`service/collector.hpp`](../include/etwc/service/collector.hpp) | `src/service/collector.cpp` |
| — | Windows Service | [`service/windows_service.hpp`](../include/etwc/service/windows_service.hpp) | `src/service/service_main.cpp`, `service_control.cpp` |

## Luồng dữ liệu

```
EtwSession (Producer thread)
   → RawEvent
   → Normalizer::normalize → NormalizedEvent
   → RingBuffer<NormalizedEvent>::push
                                   │
Collector::consumer_loop (Consumer thread)
   → RingBuffer::pop
   → BehaviorGraph::ingest      (step 4)
   → SqliteStore::append        (step 3b)
   → Pruner::run_cycle (định kỳ) (step 5)
```

## Nguyên tắc kế thừa từ UNICORN

- **Partial Ordering Guarantee** → `BehaviorGraph::ingest` cập nhật tăng dần,
  cạnh vào (parent→child, process→resource) được ghi trước khi nút đích phát sinh
  cạnh ra; không duyệt lại toàn đồ thị.
- **Chống TOCTTOU** → thu thập ở mức kernel qua ETW + xác minh PEB trong RAM.
- **Quản lý bộ nhớ** → thay Graph Sketching (HistoSketch) bằng cắt tỉa nút chết
  và thu gọn subgraph sạch để giữ RAM 20–40 MB.

## Việc cần hoàn thiện (TODO chính)

- Đấu nối thật KRABSETW trong `EtwSession::Impl`.
- Ánh xạ opcode → `EventKind` trong `Normalizer::map_opcode`.
- Đọc PEB (`NtQueryInformationProcess` + `ReadProcessMemory`).
- Bảng volume `\Device\HarddiskVolumeN` → ổ DOS trong `path_normalizer`.
- Prepared statements + WAL trong `SqliteStore`.
- Thuật toán purge/collapse thực tế trong `Pruner`.
