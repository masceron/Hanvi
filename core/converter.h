#pragma once
#include <QString>
#include <tuple>
#include <functional>

std::tuple<QString, QString, QString> convert(const QStringView& input, const std::function<void(int)>& progress_callback = nullptr);
QString convert_plain(const QStringView& input, const std::function<void(int)>& progress_callback = nullptr);