#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include "aligned_document.h"

#ifdef QT_CORE_LIB
#include <QString>
#include <QStringView>
#endif

namespace hanvi {

void init_opencc(const std::string& config_path);

std::shared_ptr<AlignedDocument> convert(std::u16string_view input,
                                         const std::function<void(int)>& progress_callback = nullptr);
std::u16string convert_plain(std::u16string_view input,
                             const std::function<void(int)>& progress_callback = nullptr);

std::u16string to_simplified_chinese(std::u16string_view input);
std::u16string canonicalize_variants(std::u16string_view input);
std::u16string normalize_text(std::u16string_view input);

// UTF-8 helpers
std::string convert_plain_utf8(std::string_view input,
                               const std::function<void(int)>& progress_callback = nullptr);
std::string to_simplified_chinese_utf8(std::string_view input);
std::string normalize_text_utf8(std::string_view input);

#ifdef QT_CORE_LIB
inline std::shared_ptr<AlignedDocument> convert(QStringView input,
                                                const std::function<void(int)>& progress_callback = nullptr) {
    return convert(q_to_u16(input), progress_callback);
}
inline std::shared_ptr<AlignedDocument> convert(const QString& input,
                                                const std::function<void(int)>& progress_callback = nullptr) {
    return convert(q_to_u16(QStringView(input)), progress_callback);
}
inline QString convert_plain(QStringView input,
                             const std::function<void(int)>& progress_callback = nullptr) {
    auto res = convert_plain(q_to_u16(input), progress_callback);
    return QString::fromUtf16(res.data(), static_cast<qsizetype>(res.size()));
}
inline QString convert_plain(const QString& input,
                             const std::function<void(int)>& progress_callback = nullptr) {
    return convert_plain(QStringView(input), progress_callback);
}
inline QString to_simplified_chinese(const QString& input) {
    auto res = to_simplified_chinese(q_to_u16(input));
    return QString::fromUtf16(res.data(), static_cast<qsizetype>(res.size()));
}
inline QString canonicalize_variants(const QString& input) {
    auto res = canonicalize_variants(q_to_u16(input));
    return QString::fromUtf16(res.data(), static_cast<qsizetype>(res.size()));
}
inline QString normalize_text(const QString& input) {
    auto res = normalize_text(q_to_u16(input));
    return QString::fromUtf16(res.data(), static_cast<qsizetype>(res.size()));
}
#endif

} // namespace hanvi

using hanvi::convert;
using hanvi::convert_plain;
using hanvi::to_simplified_chinese;
using hanvi::canonicalize_variants;
using hanvi::normalize_text;
