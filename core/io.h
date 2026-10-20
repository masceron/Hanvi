#pragma once

#include <vector>
#include <string>
#include <string_view>
#include "structures.h"

#ifdef QT_CORE_LIB
#include <QList>
#include <QString>
#include <QStringView>
#include <QStringList>
#include "../app/desktop_io.h"
#endif

namespace hanvi {

std::vector<std::u16string_view> paginate(std::u16string_view input_text, int min_length);

void io_insert(int id, std::u16string_view key, std::u16string_view value, Priority priority);
void io_reorder(std::u16string_view key, const std::vector<std::u16string>& new_order);
void io_remove(int id, std::u16string_view key, Priority priority);
void io_remove_meaning(std::u16string_view key, std::u16string_view value);

#ifdef QT_CORE_LIB
inline QList<QStringView> paginate(const QString& input_text, int min_length) {
    auto pages = hanvi::paginate(q_to_u16(input_text), min_length);
    QList<QStringView> qpages;
    qpages.reserve(static_cast<qsizetype>(pages.size()));
    for (const auto& p : pages) {
        qpages.append(QStringView(reinterpret_cast<const char16_t*>(p.data()), static_cast<qsizetype>(p.size())));
    }
    return qpages;
}

inline void io_insert(int id, const QString& key, const QString& value, Priority priority) {
    hanvi::io_insert(id, q_to_u16(key), q_to_u16(value), priority);
}
inline void io_reorder(const QString& key, const QStringList& new_order) {
    std::vector<std::u16string> order;
    order.reserve(new_order.size());
    for (const auto& s : new_order) {
        order.push_back(s.toStdU16String());
    }
    hanvi::io_reorder(q_to_u16(key), order);
}
inline void io_remove(int id, const QString& key, Priority priority) {
    hanvi::io_remove(id, q_to_u16(key), priority);
}
inline void io_remove_meaning(const QString& key, const QString& value) {
    hanvi::io_remove_meaning(q_to_u16(key), q_to_u16(value));
}
#endif

} // namespace hanvi

using hanvi::paginate;
using hanvi::io_insert;
using hanvi::io_reorder;
using hanvi::io_remove;
using hanvi::io_remove_meaning;
