#pragma once

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

} // namespace hanvi

using hanvi::paginate;
using hanvi::io_insert;
using hanvi::io_reorder;
using hanvi::io_remove;
using hanvi::io_remove_meaning;
using hanvi::io_insert_variant;
using hanvi::io_remove_variant;
