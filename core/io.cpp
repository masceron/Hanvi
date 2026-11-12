#include "io.h"
#include "db.h"
#include "dict.h"
#include "history.h"
#include "unicode_utils.h"

namespace hanvi {
    std::vector<std::u16string_view> paginate(const std::u16string_view input_text, const int min_length) {
        std::vector<std::u16string_view> pages;
        size_t cursor = 0;
        const size_t length = input_text.length();

        while (cursor < length) {
            const size_t target_end = cursor + min_length;

            if (target_end >= length) {
                pages.push_back(input_text.substr(cursor));
                break;
            }
            const size_t cutoff = input_text.find(u'\n', target_end);
            if (cutoff == std::u16string_view::npos) {
                pages.push_back(input_text.substr(cursor));
                break;
            } else {
                const size_t chunk_len = cutoff + 1 - cursor;
                pages.push_back(input_text.substr(cursor, chunk_len));

                cursor = cutoff + 1;
            }
        }

        return pages;
    }

    static HistoryKind kind_of(const int id, const Priority priority) {
        return id == -1 && priority == PHRASE ? HistoryKind::Phrase : HistoryKind::Name;
    }

    static char32_t variant_code_point(const std::u16string_view original) {
        if (is_high_surrogate(original[0]) && original.size() >= 2 && is_low_surrogate(original[1])) {
            return to_code_point(original[0], original[1]);
        }
        return original[0];
    }

    static void insert_variant(const std::u16string_view original, const std::u16string_view canonical) {
        variants.insert_or_assign(variant_code_point(original), std::u16string(canonical));
        db_insert_variant(original, canonical);
    }

    static void remove_variant(const std::u16string_view original) {
        variants.erase(variant_code_point(original));
        db_remove_variant(original);
    }

    // Makes the key hold exactly `value` (nullopt: absent), in memory and in the database.
    static void restore(const HistoryKind kind, const int set_id, const std::u16string_view key,
                        const std::optional<std::u16string> &value) {
        switch (kind) {
            case HistoryKind::Name:
                if (set_id == -1) {
                    if (value) {
                        dictionary.insert(key, *value, NAME);
                        db_insert(key, *value, NAME);
                    } else {
                        dictionary.remove(key, NAME);
                        db_remove(key, NAME);
                    }
                } else {
                    if (value) {
                        if (set_id == current_name_set_id) name_set_dictionary.insert(key, *value, NAME);
                        nameset_db_insert(set_id, key, *value);
                    } else {
                        if (set_id == current_name_set_id) name_set_dictionary.remove(key, NAME);
                        nameset_db_remove(set_id, key);
                    }
                }
                break;
            case HistoryKind::Phrase:
                // Inserting into an absent phrase stores the value as it is, meanings and all.
                dictionary.remove(key, PHRASE);
                db_remove(key, PHRASE);
                if (value) {
                    dictionary.insert(key, *value, PHRASE);
                    db_insert(key, *value, PHRASE);
                }
                break;
            case HistoryKind::Variant:
                if (value) {
                    insert_variant(key, *value);
                } else {
                    remove_variant(key);
                }
                break;
        }
    }

    void io_insert(const int id, const std::u16string_view key, const std::u16string_view value,
                   const Priority priority) {
        if (priority == NONE) return;
        HistoryRecord record(kind_of(id, priority), id, key);
        if (id == -1) {
            dictionary.insert(key, value, priority);
            db_insert(key, value, priority);
        } else {
            if (id == current_name_set_id) name_set_dictionary.insert(key, value, priority);
            nameset_db_insert(id, key, value);
        }
    }

    void io_reorder(const std::u16string_view key, const std::vector<std::u16string> &new_order) {
        HistoryRecord record(HistoryKind::Phrase, -1, key);
        dictionary.reorder(key, new_order);
        db_reorder(key, new_order);
    }

    void io_remove(const int id, const std::u16string_view key, const Priority priority) {
        if (priority == NONE) return;
        HistoryRecord record(kind_of(id, priority), id, key);
        if (id == -1) {
            dictionary.remove(key, priority);
            db_remove(key, priority);
        } else {
            if (id == current_name_set_id) name_set_dictionary.remove(key, priority);
            nameset_db_remove(id, key);
        }
    }

    void io_remove_meaning(const std::u16string_view key, const std::u16string_view value) {
        HistoryRecord record(HistoryKind::Phrase, -1, key);
        dictionary.remove_meaning(key, value);
        db_remove_meaning(key, value);
    }

    void io_insert_variant(const std::u16string_view original, const std::u16string_view canonical) {
        if (original.empty() || canonical.empty()) return;
        HistoryRecord record(HistoryKind::Variant, -1, original);
        insert_variant(original, canonical);
    }

    void io_remove_variant(const std::u16string_view original) {
        if (original.empty()) return;
        HistoryRecord record(HistoryKind::Variant, -1, original);
        remove_variant(original);
    }

    UndoState io_undo(const int id) {
        const auto action = history_get(id);
        if (!action) return UndoState::ChangedOutside;
        if (action->state != UndoState::Undoable) return action->state;

        sqlite3 *db = get_db_handle();
        sqlite3_exec(db, "SAVEPOINT history_undo", nullptr, nullptr, nullptr);
        restore(action->kind, action->set_id, action->original, action->before);
        history_mark_undone(id);
        sqlite3_exec(db, "RELEASE history_undo", nullptr, nullptr, nullptr);
        return UndoState::Undoable;
    }

    std::optional<int> io_create_name_set(const std::u16string_view title) {
        const auto id = nameset_db_create(title);
        if (id) name_sets.emplace_back(*id, std::u16string(title));
        return id;
    }

    std::optional<int> io_import_name_set(const std::u16string_view title, const std::vector<Entry> &entries) {
        const auto id = nameset_db_import(title, entries);
        if (id) name_sets.emplace_back(*id, std::u16string(title));
        return id;
    }

    bool io_rename_name_set(const int id, const std::u16string_view title) {
        if (!nameset_db_rename(id, title)) return false;
        for (auto &name_set: name_sets) {
            if (name_set.index == id) name_set.title = title;
        }
        return true;
    }

    void io_delete_name_set(const int id) {
        nameset_db_delete(id);
        std::erase_if(name_sets, [id](const NameSet &name_set) { return name_set.index == id; });
        if (id == current_name_set_id) load_name_set(-1);
    }
} // namespace hanvi
