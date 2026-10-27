#pragma once
#include "structures.h"
#include <string>
#include <string_view>
#include <vector>

namespace hanvi {

void db_insert(std::u16string_view key, std::u16string_view value, Priority priority);
void db_reorder(std::u16string_view key, const std::vector<std::u16string>& new_order);
void db_remove(std::u16string_view key, Priority priority);
void db_remove_meaning(std::u16string_view key, std::u16string_view value);

void nameset_db_insert(std::u16string_view key, std::u16string_view value);
void nameset_db_remove(std::u16string_view key);

void db_insert_variant(std::u16string_view original, std::u16string_view canonical);
void db_remove_variant(std::u16string_view original);

} // namespace hanvi

using hanvi::db_insert;
using hanvi::db_reorder;
using hanvi::db_remove;
using hanvi::db_remove_meaning;
using hanvi::nameset_db_insert;
using hanvi::nameset_db_remove;
using hanvi::db_insert_variant;
using hanvi::db_remove_variant;
