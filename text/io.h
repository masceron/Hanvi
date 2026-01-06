#pragma once
#include <expected>

enum class io_error
{
    not_text,
    file_not_readable,
};

QList<QStringView> paginate(const QString& input_text, int min_length);
std::expected<QString, io_error> load_from_clipboard();
std::expected<QString, io_error> load_from_file(const QString& name);
void save_to_file(int min_length);
void save_to_clipboard(int min_length);