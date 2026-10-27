#pragma once
#include "structures.h"
#include <string>
#include <string_view>
#include <vector>

#ifdef QT_CORE_LIB
#include <QString>
#include <QStringList>
#endif

namespace hanvi {

void db_insert(std::u16string_view key, std::u16string_view value, Priority priority);
void db_reorder(std::u16string_view key, const std::vector<std::u16string>& new_order);
void db_remove(std::u16string_view key, Priority priority);
void db_remove_meaning(std::u16string_view key, std::u16string_view value);

void nameset_db_insert(std::u16string_view key, std::u16string_view value);
void nameset_db_remove(std::u16string_view key);

void db_insert_variant(std::u16string_view original, std::u16string_view canonical);
void db_remove_variant(std::u16string_view original);

#ifdef QT_CORE_LIB
inline void db_insert(const QString& key, const QString& value, Priority priority) {
    db_insert(q_to_u16(key), q_to_u16(value), priority);
}
inline void db_reorder(const QString& key, const QStringList& new_order) {
    std::vector<std::u16string> order;
    order.reserve(new_order.size());
    for (const auto& s : new_order) {
        order.push_back(s.toStdU16String());
    }
    db_reorder(q_to_u16(key), order);
}
inline void db_remove(const QString& key, Priority priority) {
    db_remove(q_to_u16(key), priority);
}
inline void db_remove_meaning(const QString& key, const QString& value) {
    db_remove_meaning(q_to_u16(key), q_to_u16(value));
}
inline void nameset_db_insert(const QString& key, const QString& value) {
    nameset_db_insert(q_to_u16(key), q_to_u16(value));
}
inline void nameset_db_remove(const QString& key) {
    nameset_db_remove(q_to_u16(key));
}
inline void db_insert_variant(const QString& original, const QString& canonical) {
    db_insert_variant(q_to_u16(original), q_to_u16(canonical));
}
inline void db_remove_variant(const QString& original) {
    db_remove_variant(q_to_u16(original));
}
#endif

} // namespace hanvi

using hanvi::db_insert;
using hanvi::db_reorder;
using hanvi::db_remove;
using hanvi::db_remove_meaning;
using hanvi::nameset_db_insert;
using hanvi::nameset_db_remove;
using hanvi::db_insert_variant;
using hanvi::db_remove_variant;
