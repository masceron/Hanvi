#include "dict.h"
#include "sqlite_helper.h"
#include "unicode_utils.h"
#include <future>
#include <thread>
#include <mutex>
#include <iostream>

namespace hanvi {

static sqlite3* g_db = nullptr;
static std::string g_db_path = "dict.db";
static std::mutex g_db_mutex;

void init_db(const std::string& db_path)
{
    std::lock_guard lock(g_db_mutex);
    if (g_db) return;

    g_db_path = db_path;
    if (sqlite3_open_v2(g_db_path.c_str(), &g_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK)
    {
        std::cerr << "Failed to open database: " << g_db_path << std::endl;
        return;
    }

    char* errmsg = nullptr;
    sqlite3_exec(g_db, "PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;", nullptr, nullptr, &errmsg);
    if (errmsg) sqlite3_free(errmsg);

    sqlite3_exec(g_db, "CREATE TABLE IF NOT EXISTS variants (original TEXT PRIMARY KEY, canonical TEXT NOT NULL) WITHOUT ROWID;", nullptr, nullptr, nullptr);
}

void close_db()
{
    std::lock_guard lock(g_db_mutex);
    if (g_db)
    {
        sqlite3_close(g_db);
        g_db = nullptr;
    }
}

sqlite3* get_db_handle()
{
    if (!g_db) init_db();
    return g_db;
}

static void load_name_sets_data()
{
    name_sets.clear();
    sqlite3* db = get_db_handle();
    if (!db) return;

    SqliteStmt stmt(db, "SELECT id, title FROM name_sets");
    while (stmt.step())
    {
        int id = stmt.column_int(0);
        auto title = stmt.column_text16(1);
        name_sets.emplace_back(id, std::u16string(title));
    }
}

static void load_global_data(const std::function<void()>& on_finished)
{
    const std::string path = g_db_path;

    auto do_load = [path, on_finished]() {
        auto fut_sv = std::async(std::launch::async, [path] {
            sqlite3* db_sv = nullptr;
            if (sqlite3_open_v2(path.c_str(), &db_sv, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) == SQLITE_OK)
            {
                SqliteStmt stmt(db_sv, "SELECT original, translated FROM sv_readings");
                while (stmt.step())
                {
                    auto orig = stmt.column_text16(0);
                    auto trans = stmt.column_text16(1);
                    if (!orig.empty())
                    {
                        if (orig.front() == 0xFEFF)
                        {
                            orig.remove_prefix(1);
                        }
                        if (!orig.empty())
                        {
                            char32_t cp = orig[0];
                            if (is_high_surrogate(orig[0]) && orig.size() >= 2 && is_low_surrogate(orig[1]))
                            {
                                cp = to_code_point(orig[0], orig[1]);
                            }
                            sv_readings.insert_or_assign(cp, std::u16string(trans));
                        }
                    }
                }
                sqlite3_close(db_sv);
            }
        });

        auto fut_punc = std::async(std::launch::async, [path] {
            sqlite3* db_punc = nullptr;
            if (sqlite3_open_v2(path.c_str(), &db_punc, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) == SQLITE_OK)
            {
                SqliteStmt stmt(db_punc, "SELECT original, normalized FROM punctuations");
                while (stmt.step())
                {
                    auto orig = stmt.column_text16(0);
                    auto norm = stmt.column_text16(1);
                    if (!orig.empty() && !norm.empty())
                    {
                        punctuations.insert_or_assign(orig[0], norm[0]);
                    }
                }
                sqlite3_close(db_punc);
            }
        });

        auto fut_trie = std::async(std::launch::async, [path] {
            sqlite3* db_trie = nullptr;
            if (sqlite3_open_v2(path.c_str(), &db_trie, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) == SQLITE_OK)
            {
                {
                    SqliteStmt stmt(db_trie, "SELECT original, translated FROM names");
                    while (stmt.step())
                    {
                        dictionary.insert_bulk(stmt.column_text16(0), Priority::NAME, stmt.column_text16(1));
                    }
                }
                {
                    SqliteStmt stmt(db_trie, "SELECT original, translated FROM phrases");
                    while (stmt.step())
                    {
                        dictionary.insert_bulk(stmt.column_text16(0), Priority::PHRASE, stmt.column_text16(1));
                    }
                }
                {
                    SqliteStmt stmt(db_trie, "SELECT original_start, original_end, translated_start, translated_end FROM grammar_rules");
                    while (stmt.step())
                    {
                        dictionary.insert_rule(stmt.column_text16(0), stmt.column_text16(1),
                                               stmt.column_text16(2), stmt.column_text16(3));
                    }
                }
                sqlite3_close(db_trie);
            }
        });

        auto fut_var = std::async(std::launch::async, [path] {
            sqlite3* db_var = nullptr;
            if (sqlite3_open_v2(path.c_str(), &db_var, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nullptr) == SQLITE_OK)
            {
                SqliteStmt stmt(db_var, "SELECT original, canonical FROM variants");
                if (stmt.is_valid())
                {
                    while (stmt.step())
                    {
                        auto orig = stmt.column_text16(0);
                        auto canon = stmt.column_text16(1);
                        if (!orig.empty() && !canon.empty())
                        {
                            if (orig.front() == 0xFEFF) orig.remove_prefix(1);
                            if (!orig.empty())
                            {
                                char32_t cp = orig[0];
                                if (is_high_surrogate(orig[0]) && orig.size() >= 2 && is_low_surrogate(orig[1]))
                                {
                                    cp = to_code_point(orig[0], orig[1]);
                                }
                                variants.insert_or_assign(cp, std::u16string(canon));
                            }
                        }
                    }
                }
                sqlite3_close(db_var);
            }
        });

        fut_sv.get();
        fut_punc.get();
        fut_trie.get();
        fut_var.get();

        if (on_finished)
        {
            on_finished();
        }
    };

    if (on_finished)
    {
        std::thread(do_load).detach();
    }
    else
    {
        do_load();
    }
}

void load_dict(const std::function<void()>& on_finished)
{
    init_db();
    load_name_sets_data();
    load_global_data(on_finished);
}

void load_dict(const std::string& db_path, const std::function<void()>& on_finished)
{
    init_db(db_path);
    load_name_sets_data();
    load_global_data(on_finished);
}

void load_name_set(const int id)
{
    current_name_set_id = id;
    name_set_dictionary = Dictionary();

    if (id == -1) return;

    sqlite3* db = get_db_handle();
    if (!db) return;

    SqliteStmt stmt(db, "SELECT original, translated FROM name_set_entries WHERE set_id = ?");
    stmt.bind_int(1, id);

    while (stmt.step())
    {
        name_set_dictionary.insert_bulk(stmt.column_text16(0), Priority::NAME, stmt.column_text16(1));
    }
}

void reload_dict(const std::function<void()>& on_finished)
{
    sv_readings.clear();
    variants.clear();
    punctuations.clear();
    dictionary = Dictionary();
    name_sets.clear();

    load_name_sets_data();
    load_name_set(current_name_set_id);
    load_global_data(on_finished);
}

} // namespace hanvi
