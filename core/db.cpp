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
        size_t pos = str.find(sep, start);
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

    std::u16string joined = join_u16(new_order, u'\x1F');
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
    std::string sql = std::string("DELETE FROM ") + table + " WHERE original = ?";
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
        auto raw = q.column_text16(0);
        auto list = split_u16(raw, u'\x1F');

        std::vector<std::u16string> remaining;
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
            std::u16string joined = join_u16(remaining, u'\x1F');
            SqliteStmt update(db, "UPDATE phrases SET translated = ? WHERE original = ?");
            update.bind_text16(1, joined);
            update.bind_text16(2, key);
            update.exec();
        }
    }
}

void nameset_db_insert(const std::u16string_view key, const std::u16string_view value)
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
    q.bind_int(2, current_name_set_id);
    q.bind_text16(3, value);
    q.exec();
}

void nameset_db_remove(const std::u16string_view key)
{
    sqlite3* db = get_db_handle();
    if (!db) return;

    SqliteStmt q(db, "DELETE FROM name_set_entries WHERE set_id = ? AND original = ?");
    q.bind_int(1, current_name_set_id);
    q.bind_text16(2, key);
    q.exec();
}

} // namespace hanvi
