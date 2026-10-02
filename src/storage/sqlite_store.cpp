#include "etwc/storage/sqlite_store.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "etwc/common/encoding.hpp"
#include "etwc/common/logging.hpp"

namespace etwc {
namespace {

// Chạy một câu lệnh không trả dữ liệu; log lỗi nếu có.
bool exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        ETWC_LOG_ERROR(std::string("sqlite exec lỗi: ") + (err ? err : "?") + " | " + sql);
        sqlite3_free(err);
        return false;
    }
    return true;
}

// Bind TEXT (copy) hoặc NULL nếu rỗng.
void bind_text(sqlite3_stmt* st, int idx, const std::string& s) {
    if (s.empty())
        sqlite3_bind_null(st, idx);
    else
        sqlite3_bind_text(st, idx, s.data(), static_cast<int>(s.size()), SQLITE_TRANSIENT);
}

}  // namespace

struct SqliteStore::Impl {
    std::vector<NormalizedEvent> pending;  // batch buffer
    sqlite3_stmt* insert_stmt = nullptr;
    static constexpr std::size_t kFlushThreshold = 512;
};

SqliteStore::SqliteStore(const Config& cfg) : cfg_(cfg), impl_(std::make_unique<Impl>()) {}

SqliteStore::~SqliteStore() {
    close();
}

void SqliteStore::open() {
    if (!cfg_.enable_sqlite)
        return;

    // Tạo thư mục chứa DB nếu cần.
    std::error_code ec;
    if (cfg_.sqlite_path.has_parent_path())
        std::filesystem::create_directories(cfg_.sqlite_path.parent_path(), ec);

    const std::string db_path = wide_to_utf8(cfg_.sqlite_path.wstring());
    if (sqlite3_open_v2(db_path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                        nullptr) != SQLITE_OK) {
        ETWC_LOG_ERROR(std::string("sqlite mở DB thất bại: ") + (db_ ? sqlite3_errmsg(db_) : "?"));
        if (db_) {
            sqlite3_close_v2(db_);
            db_ = nullptr;
        }
        return;
    }

    sqlite3_busy_timeout(db_, 5000);  // chờ tối đa 5s khi DB bận
    // WAL cho ghi song song/không nghẽn; NORMAL đổi độ bền lấy throughput.
    exec(db_, "PRAGMA journal_mode=WAL;");
    exec(db_, "PRAGMA synchronous=NORMAL;");
    exec(db_, "PRAGMA temp_store=MEMORY;");

    ensure_schema();
    prepare_statements();
    ETWC_LOG_INFO("SqliteStore opened: " + db_path);
}

void SqliteStore::ensure_schema() {
    exec(db_,
         "CREATE TABLE IF NOT EXISTS events("
         "uuid TEXT PRIMARY KEY,"
         "kind INTEGER NOT NULL,"
         "ts INTEGER NOT NULL,"
         "pid INTEGER NOT NULL,"
         "ppid INTEGER NOT NULL,"
         "process_name TEXT,"
         "parent_name TEXT,"
         "command_line TEXT,"
         "target TEXT,"
         "is_system INTEGER NOT NULL,"
         "token_elevated INTEGER NOT NULL,"
         "remote_addr TEXT,"
         "remote_port INTEGER);");
    exec(db_, "CREATE INDEX IF NOT EXISTS idx_events_pid ON events(pid);");
    exec(db_, "CREATE INDEX IF NOT EXISTS idx_events_ts ON events(ts);");
    exec(db_, "CREATE INDEX IF NOT EXISTS idx_events_kind ON events(kind);");
}

void SqliteStore::prepare_statements() {
    static const char* kInsert =
        "INSERT OR IGNORE INTO events("
        "uuid,kind,ts,pid,ppid,process_name,parent_name,command_line,target,"
        "is_system,token_elevated,remote_addr,remote_port) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(db_, kInsert, -1, &impl_->insert_stmt, nullptr) != SQLITE_OK) {
        ETWC_LOG_ERROR(std::string("sqlite prepare lỗi: ") + sqlite3_errmsg(db_));
        impl_->insert_stmt = nullptr;
    }
}

void SqliteStore::append(const NormalizedEvent& ev) {
    if (!cfg_.enable_sqlite || db_ == nullptr)
        return;
    impl_->pending.push_back(ev);
    if (impl_->pending.size() >= Impl::kFlushThreshold)
        flush();
}

void SqliteStore::flush() {
    if (db_ == nullptr || impl_->insert_stmt == nullptr || impl_->pending.empty())
        return;

    sqlite3_stmt* st = impl_->insert_stmt;
    exec(db_, "BEGIN IMMEDIATE;");
    for (const NormalizedEvent& ev : impl_->pending) {
        bind_text(st, 1, ev.uuid);
        sqlite3_bind_int(st, 2, static_cast<int>(ev.kind));
        sqlite3_bind_int64(st, 3, static_cast<sqlite3_int64>(ev.timestamp));
        sqlite3_bind_int64(st, 4, ev.pid);
        sqlite3_bind_int64(st, 5, ev.ppid);
        bind_text(st, 6, ev.process_name);
        bind_text(st, 7, ev.parent_name);
        bind_text(st, 8, ev.command_line);
        bind_text(st, 9, ev.target);
        sqlite3_bind_int(st, 10, ev.is_system ? 1 : 0);
        sqlite3_bind_int(st, 11, ev.token_elevated ? 1 : 0);
        if (ev.remote_addr)
            bind_text(st, 12, *ev.remote_addr);
        else
            sqlite3_bind_null(st, 12);
        if (ev.remote_port)
            sqlite3_bind_int(st, 13, *ev.remote_port);
        else
            sqlite3_bind_null(st, 13);

        if (sqlite3_step(st) != SQLITE_DONE)
            ETWC_LOG_ERROR(std::string("sqlite insert lỗi: ") + sqlite3_errmsg(db_));
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
    }
    exec(db_, "COMMIT;");
    impl_->pending.clear();
}

std::int64_t SqliteStore::count_events() {
    flush();
    if (db_ == nullptr)
        return -1;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM events;", -1, &st, nullptr) != SQLITE_OK)
        return -1;
    std::int64_t n = -1;
    if (sqlite3_step(st) == SQLITE_ROW)
        n = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return n;
}

void SqliteStore::close() {
    if (db_ == nullptr) {
        if (impl_)
            impl_->pending.clear();
        return;
    }
    flush();
    if (impl_->insert_stmt) {
        sqlite3_finalize(impl_->insert_stmt);
        impl_->insert_stmt = nullptr;
    }
    sqlite3_close_v2(db_);
    db_ = nullptr;
}

}  // namespace etwc
