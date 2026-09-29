#pragma once

#include <memory>
#include <string>

#include "etwc/common/config.hpp"
#include "etwc/normalizer/normalized_event.hpp"

struct sqlite3;

namespace etwc {

// Step 3b — Lưu trữ kép: ghi bất tuần tự NormalizedEvent xuống SQLite 3
// để lưu vết lịch sử phục vụ điều tra số hồi tố.
// Dùng WAL + batch trong transaction để không nghẽn luồng telemetry.
class SqliteStore {
public:
    explicit SqliteStore(const Config& cfg);
    ~SqliteStore();

    SqliteStore(const SqliteStore&) = delete;
    SqliteStore& operator=(const SqliteStore&) = delete;

    void open();
    void close();

    // Ghi một sự kiện (được batch nội bộ, flush theo ngưỡng/timer).
    void append(const NormalizedEvent& ev);
    void flush();

private:
    void ensure_schema();

    const Config& cfg_;
    sqlite3* db_ = nullptr;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace etwc
