#pragma once

#include <optional>
#include <vector>
#include <string>
#include <string_view>
#include "structures.h"

namespace hanvi {

std::vector<std::u16string_view> paginate(std::u16string_view input_text, int min_length);

void io_insert(int id, std::u16string_view key, std::u16string_view value, Priority priority);
void io_reorder(std::u16string_view key, const std::vector<std::u16string>& new_order);
void io_remove(int id, std::u16string_view key, Priority priority);
void io_remove_meaning(std::u16string_view key, std::u16string_view value);
void io_insert_variant(std::u16string_view original, std::u16string_view canonical);
void io_remove_variant(std::u16string_view original);

// Nameset management; keeps name_sets in sync. create/import return the new set id, or nullopt
// on failure (e.g. a duplicate title); import is all-or-nothing.
std::optional<int> io_create_name_set(std::u16string_view title);
std::optional<int> io_import_name_set(std::u16string_view title, const std::vector<Entry>& entries);
bool io_rename_name_set(int id, std::u16string_view title);
void io_delete_name_set(int id);

} // namespace hanvi

using hanvi::paginate;
using hanvi::io_insert;
using hanvi::io_reorder;
using hanvi::io_remove;
using hanvi::io_remove_meaning;
using hanvi::io_insert_variant;
using hanvi::io_remove_variant;
using hanvi::io_create_name_set;
using hanvi::io_import_name_set;
using hanvi::io_rename_name_set;
using hanvi::io_delete_name_set;
