#include "db.h"
#include "dict.h"
#include "sqlite_helper.h"
#include <vector>
#include <string>

namespace hanvi {

static std::vector<std::u16string_view> split_u16(std::u16string_view str, const char16_t sep)
{
    std::vector<std::u16string_view> result;
    size_t start = 0;
    while (start < str.size())
    {
        const size_t pos = str.find(sep, start);
        if (pos == std::u16string_view::npos)
        {
            result.push_back(str.substr(start));
            break;
        }
        result.push_back(str.substr(start, pos - start));
        start = pos + 1;
    }
    return result;
}

static std::u16string join_u16(const std::vector<std::u16string>& parts, const char16_t sep)
{
    if (parts.empty()) return {};
    std::u16string res = parts[0];
    for (size_t i = 1; i < parts.size(); ++i)
    {
        res.push_back(sep);
        res += parts[i];
    }
    return res;
}

static const char* get_table_name(const Priority priority)
{
    return priority == NAME ? "names" : "phrases";
}

void db_insert(std::u16string_view key, std::u16string_view value, const Priority priority)
{
    if (priority == NONE) return;
    sqlite3* db = get_db_handle();
    if (!db) return;

    // A name has a single translation, loaded whole: replace it. Phrases keep a list of
    // meanings, with the new one first.
    if (priority == NAME)
    {
        SqliteStmt upsert(db, "INSERT INTO names (original, translated) VALUES (?, ?) "
                              "ON CONFLICT (original) DO UPDATE SET translated = excluded.translated");
        upsert.bind_text16(1, key);
        upsert.bind_text16(2, value);
        upsert.exec();
        return;
    }

    const char* table = get_table_name(priority);
    std::string select_sql = std::string("SELECT translated FROM ") + table + " WHERE original = ?";
    SqliteStmt q(db, select_sql.c_str());
    q.bind_text16(1, key);

    if (q.step())
    {
        auto raw = q.column_text16(0);
        auto list = split_u16(raw, u'\x1F');

        std::vector<std::u16string> items;
        items.emplace_back(value);
        for (const auto& item : list)
        {
            if (item != value && !item.empty())
            {
                items.emplace_back(item);
            }
        }

        std::u16string joined = join_u16(items, u'\x1F');
        std::string update_sql = std::string("UPDATE ") + table + " SET translated = ? WHERE original = ?";
        SqliteStmt update_stmt(db, update_sql.c_str());
        update_stmt.bind_text16(1, joined);
        update_stmt.bind_text16(2, key);
        update_stmt.exec();
    }
    else
    {
        std::string insert_sql = std::string("INSERT INTO ") + table + " (original, translated) VALUES (?, ?)";
        SqliteStmt insert_stmt(db, insert_sql.c_str());
        insert_stmt.bind_text16(1, key);
        insert_stmt.bind_text16(2, value);
        insert_stmt.exec();
    }
}

void db_reorder(const std::u16string_view key, const std::vector<std::u16string>& new_order)
{
    sqlite3* db = get_db_handle();
    if (!db) return;

    const std::u16string joined = join_u16(new_order, u'\x1F');
    SqliteStmt q(db, "INSERT OR REPLACE INTO phrases (original, translated) VALUES (?, ?)");
    q.bind_text16(1, key);
    q.bind_text16(2, joined);
    q.exec();
}

void db_remove(const std::u16string_view key, const Priority priority)
{
    if (priority == NONE) return;
    sqlite3* db = get_db_handle();
    if (!db) return;

    const char* table = get_table_name(priority);
    const std::string sql = std::string("DELETE FROM ") + table + " WHERE original = ?";
    SqliteStmt q(db, sql.c_str());
    q.bind_text16(1, key);
    q.exec();
}

void db_remove_meaning(const std::u16string_view key, const std::u16string_view value)
{
    sqlite3* db = get_db_handle();
    if (!db) return;

    SqliteStmt q(db, "SELECT translated FROM phrases WHERE original = ?");
    q.bind_text16(1, key);

    if (q.step())
    {
        const auto raw = q.column_text16(0);
        const auto list = split_u16(raw, u'\x1F');

        std::vector<std::u16string> remaining;
        remaining.reserve(list.size());
        for (const auto& item : list)
        {
            if (item != value && !item.empty())
            {
                remaining.emplace_back(item);
            }
        }

        if (remaining.empty())
        {
            SqliteStmt del(db, "DELETE FROM phrases WHERE original = ?");
            del.bind_text16(1, key);
            del.exec();
        }
        else
        {
            const std::u16string joined = join_u16(remaining, u'\x1F');
            SqliteStmt update(db, "UPDATE phrases SET translated = ? WHERE original = ?");
            update.bind_text16(1, joined);
            update.bind_text16(2, key);
            update.exec();
        }
    }
}

static std::string entries_source(const int set_id, const Priority priority)
{
    if (set_id != -1) return "name_set_entries WHERE set_id = ?1";
    return std::string(get_table_name(priority)) + " WHERE TRUE";
}

std::vector<Entry> db_list_entries(const int set_id, const Priority priority, const std::u16string_view filter,
                                   const int offset, const int limit)
{
    std::vector<Entry> entries;
    sqlite3* db = get_db_handle();
    if (!db) return entries;

    std::string sql = "SELECT original, translated FROM " + entries_source(set_id, priority);
    if (!filter.empty()) sql += " AND (original LIKE ?2 OR translated LIKE ?2)";
    sql += " ORDER BY original LIMIT ?3 OFFSET ?4";

    SqliteStmt q(db, sql.c_str());
    if (set_id != -1) q.bind_int(1, set_id);
    if (!filter.empty())
    {
        std::u16string pattern = u"%";
        pattern += filter;
        pattern += u'%';
        q.bind_text16(2, pattern);
    }
    q.bind_int(3, limit);
    q.bind_int(4, offset);

    while (q.step())
    {
        entries.push_back(Entry{
            .original = std::u16string(q.column_text16(0)),
            .translated = std::u16string(q.column_text16(1))
        });
    }
    return entries;
}

int db_count_entries(const int set_id, const Priority priority)
{
    sqlite3* db = get_db_handle();
    if (!db) return 0;

    const std::string sql = "SELECT COUNT(*) FROM " + entries_source(set_id, priority);
    SqliteStmt q(db, sql.c_str());
    if (set_id != -1) q.bind_int(1, set_id);
    return q.step() ? q.column_int(0) : 0;
}

std::optional<std::u16string> db_get_translation(const std::u16string_view key, const Priority priority)
{
    if (priority == NONE) return std::nullopt;
    sqlite3* db = get_db_handle();
    if (!db) return std::nullopt;

    const std::string sql = std::string("SELECT translated FROM ") + get_table_name(priority) + " WHERE original = ?";
    SqliteStmt q(db, sql.c_str());
    q.bind_text16(1, key);
    if (!q.step()) return std::nullopt;
    return std::u16string(q.column_text16(0));
}

void nameset_db_insert(const int set_id, const std::u16string_view key, const std::u16string_view value)
{
    sqlite3* db = get_db_handle();
    if (!db) return;

    const char* sql =
        "INSERT INTO name_set_entries (original, set_id, translated) "
        "VALUES (?, ?, ?) "
        "ON CONFLICT (set_id, original) "
        "DO UPDATE SET translated = excluded.translated";

    SqliteStmt q(db, sql);
    q.bind_text16(1, key);
    q.bind_int(2, set_id);
    q.bind_text16(3, value);
    q.exec();
}

void nameset_db_remove(const int set_id, const std::u16string_view key)
{
    sqlite3* db = get_db_handle();
    if (!db) return;

    SqliteStmt q(db, "DELETE FROM name_set_entries WHERE set_id = ? AND original = ?");
    q.bind_int(1, set_id);
    q.bind_text16(2, key);
    q.exec();
}

std::unordered_map<int, int> nameset_db_entry_counts()
{
    std::unordered_map<int, int> counts;
    sqlite3* db = get_db_handle();
    if (!db) return counts;

    SqliteStmt q(db, "SELECT set_id, COUNT(*) FROM name_set_entries GROUP BY set_id");
    while (q.step())
    {
        counts.emplace(q.column_int(0), q.column_int(1));
    }
    return counts;
}

std::optional<int> nameset_db_create(const std::u16string_view title)
{
    sqlite3* db = get_db_handle();
    if (!db) return std::nullopt;

    SqliteStmt q(db, "INSERT INTO name_sets (title) VALUES (?)");
    q.bind_text16(1, title);
    if (q.exec() != SQLITE_DONE) return std::nullopt;
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

std::optional<int> nameset_db_import(const std::u16string_view title, const std::vector<Entry>& entries)
{
    sqlite3* db = get_db_handle();
    if (!db) return std::nullopt;
    if (sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr) != SQLITE_OK) return std::nullopt;

    const auto set_id = nameset_db_create(title);
    bool ok = set_id.has_value();
    if (ok)
    {
        SqliteStmt q(db, "INSERT INTO name_set_entries (original, translated, set_id) VALUES (?, ?, ?)");
        for (const auto& [original, translated] : entries)
        {
            q.bind_text16(1, original);
            q.bind_text16(2, translated);
            q.bind_int(3, *set_id);
            ok = q.exec() == SQLITE_DONE;
            q.reset();
            if (!ok) break;
        }
    }

    if (ok && sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK) return set_id;
    sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    return std::nullopt;
}

bool nameset_db_rename(const int set_id, const std::u16string_view title)
{
    sqlite3* db = get_db_handle();
    if (!db) return false;

    SqliteStmt q(db, "UPDATE name_sets SET title = ? WHERE id = ?");
    q.bind_text16(1, title);
    q.bind_int(2, set_id);
    return q.exec() == SQLITE_DONE;
}

void nameset_db_delete(const int set_id)
{
    sqlite3* db = get_db_handle();
    if (!db) return;

    // name_set_entries rows go with it through ON DELETE CASCADE.
    SqliteStmt q(db, "DELETE FROM name_sets WHERE id = ?");
    q.bind_int(1, set_id);
    q.exec();
}

void db_insert_variant(const std::u16string_view original, const std::u16string_view canonical)
{
    sqlite3* db = get_db_handle();
    if (!db) return;

    SqliteStmt q(db, "INSERT OR REPLACE INTO variants (original, canonical) VALUES (?, ?)");
    q.bind_text16(1, original);
    q.bind_text16(2, canonical);
    q.exec();
}

void db_remove_variant(const std::u16string_view original)
{
    sqlite3* db = get_db_handle();
    if (!db) return;

    SqliteStmt q(db, "DELETE FROM variants WHERE original = ?");
    q.bind_text16(1, original);
    q.exec();
}

} // namespace hanvi
