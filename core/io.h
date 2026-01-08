#pragma once
#include <expected>

#include "trie.h"

enum class io_error
{
    not_text,
    file_not_readable,
    file_not_writeable,
};

QList<QStringView> paginate(const QString& input_text, int min_length);
std::expected<QString, io_error> load_from_clipboard();
std::expected<QString, io_error> load_from_file(const QString& name);
int save_to_file(const QString& name, const QString& text);
void save_to_clipboard();

void io_insert(int id, const QString& key, const QString& value, Priority priority);
void io_reorder(const QString& key, const QStringList& new_order);
void io_remove(int id, const QString& key, Priority priority);
void io_remove_meaning(const QString& key, const QString& value);