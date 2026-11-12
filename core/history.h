#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace hanvi {
    // Dictionary edits made through io_* are kept in dict.db's history table, newest last, so they
    // can be listed and undone. Each action changed one key and holds its value before and after
    // (nullopt: absent), exactly as stored.
    inline constexpr int HISTORY_LIMIT = 500;

    enum class HistoryKind { Name = 0, Phrase = 1, Variant = 2 };

    // What an action did, derived from its values before and after.
    enum class HistoryChange { Add, Delete, Edit, Reorder, AddMeaning, RemoveMeaning };

    // An action can be undone only while its key still holds the value the action left.
    enum class UndoState {
        Undoable,
        Undone,
        ChangedLater,   // a later action changed the key again: undo it first
        ChangedOutside, // the key no longer holds that value, and no action here explains it
        NameSetDeleted,
    };

    struct HistoryAction {
        int id = 0;
        int64_t time = 0; // Unix seconds
        HistoryKind kind = HistoryKind::Name;
        int set_id = -1; // a nameset's name, or -1 for the global dictionary
        std::u16string original;
        std::optional<std::u16string> before;
        std::optional<std::u16string> after;
        HistoryChange change = HistoryChange::Edit;
        std::u16string meaning; // the meaning added or removed, for AddMeaning and RemoveMeaning
        UndoState state = UndoState::Undoable;
        int changed_by = 0; // the latest action that changed the key again, for ChangedLater
    };

    // The most recent actions first.
    std::vector<HistoryAction> history_list(int limit = HISTORY_LIMIT);

    std::optional<HistoryAction> history_get(int id);

    void history_mark_undone(int id);

    // Forgets every action, zeroing their rows in the file rather than just freeing them.
    void history_clear();

    void create_history_table(sqlite3 *db);

    // Records one edit of one key: the value before when constructed, the value after when
    // destroyed, in one transaction with the edit. Edits that change nothing are not recorded.
    class HistoryRecord {
    public:
        HistoryRecord(HistoryKind kind, int set_id, std::u16string_view original);

        ~HistoryRecord();

        HistoryRecord(const HistoryRecord &) = delete;

        HistoryRecord &operator=(const HistoryRecord &) = delete;

    private:
        sqlite3 *db_ = nullptr;
        HistoryKind kind_;
        int set_id_;
        std::u16string original_;
        std::optional<std::u16string> before_;
    };
} // namespace hanvi

using hanvi::HistoryKind;
using hanvi::HistoryChange;
using hanvi::UndoState;
using hanvi::HistoryAction;
using hanvi::history_list;
using hanvi::history_clear;
