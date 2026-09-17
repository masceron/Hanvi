#pragma once
#include <QString>
#include <functional>
#include <memory>

class AlignedDocument;
std::shared_ptr<AlignedDocument> convert(const QStringView& input,
                                         const std::function<void(int)>& progress_callback = nullptr);
QString convert_plain(const QStringView& input, const std::function<void(int)>& progress_callback = nullptr);
