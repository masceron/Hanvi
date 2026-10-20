#pragma once
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <cstdint>

namespace hanvi {

class SqliteStmt {
public:
    SqliteStmt() = default;
    SqliteStmt(sqlite3* db, const char* sql) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
            stmt_ = nullptr;
        }
    }
    ~SqliteStmt() {
        if (stmt_) sqlite3_finalize(stmt_);
    }

    SqliteStmt(const SqliteStmt&) = delete;
    SqliteStmt& operator=(const SqliteStmt&) = delete;

    SqliteStmt(SqliteStmt&& o) noexcept : stmt_(o.stmt_) { o.stmt_ = nullptr; }
    SqliteStmt& operator=(SqliteStmt&& o) noexcept {
        if (this != &o) {
            if (stmt_) sqlite3_finalize(stmt_);
            stmt_ = o.stmt_;
            o.stmt_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] sqlite3_stmt* get() const noexcept { return stmt_; }
    [[nodiscard]] bool is_valid() const noexcept { return stmt_ != nullptr; }

    bool bind_int(int index, int val) {
        return sqlite3_bind_int(stmt_, index, val) == SQLITE_OK;
    }

    bool bind_text16(int index, std::u16string_view val) {
        return sqlite3_bind_text16(stmt_, index, val.data(),
                                   static_cast<int>(val.size() * sizeof(char16_t)),
                                   SQLITE_TRANSIENT) == SQLITE_OK;
    }

    bool bind_text(int index, std::string_view val) {
        return sqlite3_bind_text(stmt_, index, val.data(),
                                 static_cast<int>(val.size()),
                                 SQLITE_TRANSIENT) == SQLITE_OK;
    }

    bool step() {
        return sqlite3_step(stmt_) == SQLITE_ROW;
    }

    int exec() {
        return sqlite3_step(stmt_);
    }

    void reset() {
        if (stmt_) {
            sqlite3_reset(stmt_);
            sqlite3_clear_bindings(stmt_);
        }
    }

    [[nodiscard]] int column_int(int col) const {
        return sqlite3_column_int(stmt_, col);
    }

    [[nodiscard]] std::u16string_view column_text16(int col) const {
        const auto* ptr = reinterpret_cast<const char16_t*>(sqlite3_column_text16(stmt_, col));
        if (!ptr) return {};
        const size_t bytes = sqlite3_column_bytes16(stmt_, col);
        return std::u16string_view(ptr, bytes / sizeof(char16_t));
    }

    [[nodiscard]] std::string_view column_text(int col) const {
        const auto* ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, col));
        if (!ptr) return {};
        const size_t bytes = sqlite3_column_bytes(stmt_, col);
        return std::string_view(ptr, bytes);
    }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

} // namespace hanvi

