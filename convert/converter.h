#pragma once
#include <QObject>

std::pair<QString, QString> convert(const QStringView& input, const std::function<void(int)>& progress_callback = nullptr);
QString convert_plain(const QStringView& input, const std::function<void(int)>& progress_callback = nullptr);