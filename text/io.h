#pragma once
#include <expected>

#include "../convert/trie.h"

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

void io_insert(const QString& key, const QString& value, Priority priority);
void io_reorder(const QString& key, const QStringList& new_order, Priority priority);
void io_remove(const QString& key, Priority priority);
void io_remove_meaning(const QString& key, const QString& value, Priority priority);