#include "history.h"
#include "dict.h"
#include "sqlite_helper.h"
#include <algorithm>
#include <map>
#include <ranges>
#include <tuple>

namespace hanvi {
    // The value the key holds now, read from the database rather than the dictionaries in
    // memory, so that changes made outside Hanvi count.
    static std::optional<std::u16string> stored_value(sqlite3 *db, const HistoryKind kind, const int set_id,
                                                      const std::u16string_view original) {
        const char *sql = "SELECT canonical FROM variants WHERE original = ?1";
        if (kind == HistoryKind::Phrase) {
            sql = "SELECT translated FROM phrases WHERE original = ?1";
        } else if (kind == HistoryKind::Name) {
            sql = set_id == -1
                      ? "SELECT translated FROM names WHERE original = ?1"
                      : "SELECT translated FROM name_set_entries WHERE original = ?1 AND set_id = ?2";
        }
        SqliteStmt q(db, sql);
        q.bind_text16(1, original);
        if (kind == HistoryKind::Name && set_id != -1) q.bind_int(2, set_id);
        if (!q.step()) return std::nullopt;
        return std::u16string(q.column_text16(0));
    }

    static bool name_set_exists(sqlite3 *db, const int set_id) {
        SqliteStmt q(db, "SELECT 1 FROM name_sets WHERE id = ?");
        q.bind_int(1, set_id);
        return q.step();
    }

    static std::vector<std::u16string_view> meanings(const std::u16string_view value) {
        std::vector<std::u16string_view> list;
        size_t start = 0;
        while (start <= value.size()) {
            const size_t end = std::min(value.find(u'\x1F', start), value.size());
            list.push_back(value.substr(start, end - start));
            start = end + 1;
        }
        return list;
    }

    static void derive_change(HistoryAction &action) {
        if (!action.before) {
            action.change = HistoryChange::Add;
            return;
        }
        if (!action.after) {
            action.change = HistoryChange::Delete;
            return;
        }
        action.change = HistoryChange::Edit;
        if (action.kind != HistoryKind::Phrase) return;

        const auto before = meanings(*action.before);
        const auto after = meanings(*action.after);
        if (before.size() == after.size() && std::ranges::is_permutation(before, after)) {
            action.change = HistoryChange::Reorder;
        } else if (after.size() == before.size() + 1 && std::ranges::equal(after | std::views::drop(1), before)) {
            // A new meaning goes first.
            action.change = HistoryChange::AddMeaning;
            action.meaning = after.front();
        } else if (before.size() == after.size() + 1) {
            const auto diverge = std::ranges::mismatch(before, after).in1 - before.begin();
            if (std::ranges::equal(before | std::views::drop(diverge + 1), after | std::views::drop(diverge))) {
                action.change = HistoryChange::RemoveMeaning;
                action.meaning = before[diverge];
            }
        }
    }

    static std::optional<std::u16string> optional_text(const SqliteStmt &q, const int col) {
        if (sqlite3_column_type(q.get(), col) == SQLITE_NULL) return std::nullopt;
        return std::u16string(q.column_text16(col));
    }

    static HistoryAction read_action(const SqliteStmt &q) {
        HistoryAction action;
        action.id = q.column_int(0);
        action.time = sqlite3_column_int64(q.get(), 1);
        action.kind = static_cast<HistoryKind>(q.column_int(2));
        action.set_id = q.column_int(3);
        action.original = q.column_text16(4);
        action.before = optional_text(q, 5);
        action.after = optional_text(q, 6);
        action.state = q.column_int(7) ? UndoState::Undone : UndoState::Undoable;
        derive_change(action);
        return action;
    }

    // changed_by: the latest action after this one that changed the same key and is not undone.
    static void set_undo_state(sqlite3 *db, HistoryAction &action, const int changed_by) {
        if (action.state == UndoState::Undone) return;
        if (action.set_id != -1 && !name_set_exists(db, action.set_id)) {
            action.state = UndoState::NameSetDeleted;
        } else if (stored_value(db, action.kind, action.set_id, action.original) != action.after) {
            action.state = changed_by ? UndoState::ChangedLater : UndoState::ChangedOutside;
            action.changed_by = changed_by;
        }
    }

    static constexpr auto SELECT_ACTIONS =
            "SELECT id, time, kind, set_id, original, before, after, undone FROM history ";

    std::vector<HistoryAction> history_list(const int limit) {
        std::vector<HistoryAction> actions;
        sqlite3 *db = get_db_handle();
        if (!db) return actions;

        SqliteStmt q(db, (std::string(SELECT_ACTIONS) + "ORDER BY id DESC LIMIT ?").c_str());
        q.bind_int(1, limit);
        // Newest first, so the latest later action on each key is known when an action is reached.
        std::map<std::tuple<HistoryKind, int, std::u16string>, int> latest;
        while (q.step()) {
            auto &action = actions.emplace_back(read_action(q));
            auto &later = latest[{action.kind, action.set_id, action.original}];
            set_undo_state(db, action, later);
            if (action.state != UndoState::Undone && !later) later = action.id;
        }
        return actions;
    }

    std::optional<HistoryAction> history_get(const int id) {
        sqlite3 *db = get_db_handle();
        if (!db) return std::nullopt;

        SqliteStmt q(db, (std::string(SELECT_ACTIONS) + "WHERE id = ?").c_str());
        q.bind_int(1, id);
        if (!q.step()) return std::nullopt;
        HistoryAction action = read_action(q);

        SqliteStmt later(db, "SELECT MAX(id) FROM history "
                         "WHERE id > ? AND kind = ? AND set_id = ? AND original = ? AND NOT undone");
        later.bind_int(1, id);
        later.bind_int(2, static_cast<int>(action.kind));
        later.bind_int(3, action.set_id);
        later.bind_text16(4, action.original);
        set_undo_state(db, action, later.step() ? later.column_int(0) : 0);
        return action;
    }

    void history_mark_undone(const int id) {
        sqlite3 *db = get_db_handle();
        if (!db) return;

        SqliteStmt q(db, "UPDATE history SET undone = 1 WHERE id = ?");
        q.bind_int(1, id);
        q.exec();
    }

    void history_clear() {
        sqlite3 *db = get_db_handle();
        if (!db) return;

        sqlite3_exec(db, "PRAGMA secure_delete = ON; DELETE FROM history; PRAGMA secure_delete = OFF;", nullptr,
                     nullptr, nullptr);
    }

    void create_history_table(sqlite3 *db) {
        sqlite3_exec(db,
                     "CREATE TABLE IF NOT EXISTS history ("
                     "id INTEGER PRIMARY KEY, time INTEGER NOT NULL, kind INTEGER NOT NULL, "
                     "set_id INTEGER NOT NULL, original TEXT NOT NULL, before TEXT, after TEXT, "
                     "undone INTEGER NOT NULL DEFAULT 0);",
                     nullptr, nullptr, nullptr);
    }

    HistoryRecord::HistoryRecord(const HistoryKind kind, const int set_id, const std::u16string_view original)
        : db_(get_db_handle()), kind_(kind), set_id_(set_id), original_(original) {
        if (!db_) return;
        sqlite3_exec(db_, "SAVEPOINT history_record", nullptr, nullptr, nullptr);
        before_ = stored_value(db_, kind_, set_id_, original_);
    }

    HistoryRecord::~HistoryRecord() {
        if (!db_) return;
        if (const auto after = stored_value(db_, kind_, set_id_, original_); after != before_) {
            SqliteStmt insert(db_, "INSERT INTO history (time, kind, set_id, original, before, after) "
                              "VALUES (unixepoch(), ?, ?, ?, ?, ?)");
            insert.bind_int(1, static_cast<int>(kind_));
            insert.bind_int(2, set_id_);
            insert.bind_text16(3, original_);
            if (before_) insert.bind_text16(4, *before_);
            if (after) insert.bind_text16(5, *after);
            insert.exec();

            SqliteStmt trim(db_, "DELETE FROM history WHERE id <= (SELECT MAX(id) FROM history) - ?");
            trim.bind_int(1, HISTORY_LIMIT);
            trim.exec();
        }
        sqlite3_exec(db_, "RELEASE history_record", nullptr, nullptr, nullptr);
    }
} // namespace hanvi
