#include "etwc/storage/sqlite_store.hpp"

#include <vector>

#include "etwc/common/logging.hpp"

// TODO: liên kết sqlite3. Dùng WAL, prepared statement, batch insert
//       trong transaction để đạt throughput cao mà không nghẽn consumer.

namespace etwc {

struct SqliteStore::Impl {
    std::vector<NormalizedEvent> pending;  // batch buffer
    static constexpr std::size_t kFlushThreshold = 512;
};

SqliteStore::SqliteStore(const Config& cfg)
    : cfg_(cfg), impl_(std::make_unique<Impl>()) {}

SqliteStore::~SqliteStore() { close(); }

void SqliteStore::open() {
    if (!cfg_.enable_sqlite) return;
    // TODO: sqlite3_open_v2(cfg_.sqlite_path, &db_, ...);
    ensure_schema();
    ETWC_LOG_INFO("SqliteStore opened (stub)");
}

void SqliteStore::ensure_schema() {
    // TODO: CREATE TABLE IF NOT EXISTS events(
    //   uuid TEXT PRIMARY KEY, kind INTEGER, ts INTEGER,
    //   pid INTEGER, ppid INTEGER, process_name TEXT, parent_name TEXT,
    //   command_line TEXT, target TEXT, is_system INTEGER,
    //   token_elevated INTEGER, remote_addr TEXT, remote_port INTEGER);
    // CREATE INDEX idx_events_pid ON events(pid);
    // CREATE INDEX idx_events_ts  ON events(ts);
}

void SqliteStore::append(const NormalizedEvent& ev) {
    if (!cfg_.enable_sqlite) return;
    impl_->pending.push_back(ev);
    if (impl_->pending.size() >= Impl::kFlushThreshold) flush();
}

void SqliteStore::flush() {
    if (impl_->pending.empty()) return;
    // TODO: BEGIN; bind + step cho từng row; COMMIT.
    impl_->pending.clear();
}

void SqliteStore::close() {
    if (!db_ && impl_ && impl_->pending.empty()) return;
    flush();
    // TODO: sqlite3_close(db_); db_ = nullptr;
}

}  // namespace etwc
