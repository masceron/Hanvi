#pragma once

#include <QString>
#include <QStringView>
#include <QStringList>
#include <QList>
#include <string>
#include <string_view>
#include <vector>
#include "core/io.h"

namespace hanvi {

inline std::u16string_view q_to_u16(QStringView sv) noexcept {
    return std::u16string_view(reinterpret_cast<const char16_t*>(sv.utf16()), static_cast<size_t>(sv.size()));
}

inline std::u16string_view q_to_u16(const QString& s) noexcept {
    return std::u16string_view(reinterpret_cast<const char16_t*>(s.utf16()), static_cast<size_t>(s.size()));
}

inline QString to_qstring(std::u16string_view sv) {
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(sv.data()), static_cast<qsizetype>(sv.size()));
}

inline std::vector<std::u16string> to_u16_vector(const QStringList& list) {
    std::vector<std::u16string> res;
    res.reserve(list.size());
    for (const auto& s : list) {
        res.push_back(s.toStdU16String());
    }
    return res;
}

inline QList<QStringView> paginate_q(const QString& input_text, int min_length) {
    auto pages = hanvi::paginate(q_to_u16(input_text), min_length);
    QList<QStringView> qpages;
    qpages.reserve(static_cast<qsizetype>(pages.size()));
    for (const auto& p : pages) {
        qpages.append(QStringView(reinterpret_cast<const char16_t*>(p.data()), static_cast<qsizetype>(p.size())));
    }
    return qpages;
}

} // namespace hanvi

using hanvi::q_to_u16;
using hanvi::to_qstring;
using hanvi::to_u16_vector;
using hanvi::paginate_q;

