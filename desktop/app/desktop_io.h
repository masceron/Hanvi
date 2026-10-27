#pragma once

#include <QString>
#include <expected>

enum class io_error
{
    not_text,
    file_not_readable,
    file_not_writeable,
};

std::expected<QString, io_error> load_from_clipboard();
std::expected<QString, io_error> load_from_file(const QString& name);
int save_to_file(const QString& name, const QString& text);


