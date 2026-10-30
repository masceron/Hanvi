#pragma once
#include "structures.h"
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace hanvi {

void db_insert(std::u16string_view key, std::u16string_view value, Priority priority);
void db_reorder(std::u16string_view key, const std::vector<std::u16string>& new_order);
void db_remove(std::u16string_view key, Priority priority);
void db_remove_meaning(std::u16string_view key, std::u16string_view value);

std::vector<Entry> db_list_entries(int set_id, Priority priority, std::u16string_view filter, int offset, int limit);
int db_count_entries(int set_id, Priority priority);
std::optional<std::u16string> db_get_translation(std::u16string_view key, Priority priority);

void nameset_db_insert(int set_id, std::u16string_view key, std::u16string_view value);
void nameset_db_remove(int set_id, std::u16string_view key);

std::unordered_map<int, int> nameset_db_entry_counts();
std::optional<int> nameset_db_create(std::u16string_view title);
std::optional<int> nameset_db_import(std::u16string_view title, const std::vector<Entry>& entries);
bool nameset_db_rename(int set_id, std::u16string_view title);
void nameset_db_delete(int set_id);

void db_insert_variant(std::u16string_view original, std::u16string_view canonical);
void db_remove_variant(std::u16string_view original);

} // namespace hanvi

using hanvi::db_insert;
using hanvi::db_reorder;
using hanvi::db_remove;
using hanvi::db_remove_meaning;
using hanvi::db_list_entries;
using hanvi::db_count_entries;
using hanvi::db_get_translation;
using hanvi::nameset_db_insert;
using hanvi::nameset_db_remove;
using hanvi::nameset_db_entry_counts;
using hanvi::nameset_db_create;
using hanvi::nameset_db_import;
using hanvi::nameset_db_rename;
using hanvi::nameset_db_delete;
using hanvi::db_insert_variant;
using hanvi::db_remove_variant;
