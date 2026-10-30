#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <sqlite3.h>
#include "structures.h"

namespace hanvi {

inline std::unordered_map<char32_t, std::u16string> sv_readings;
inline std::unordered_map<char32_t, std::u16string> variants;
inline std::unordered_map<char16_t, char16_t> punctuations;
inline Dictionary dictionary;
inline Dictionary name_set_dictionary;
inline int current_name_set_id = -1;
inline std::vector<NameSet> name_sets;

void init_db(const std::string& db_path = "dict.db");
void close_db();
sqlite3* get_db_handle();
bool save_db_copy(const std::string& path);
int db_change_count();

void load_dict(const std::function<void()>& on_finished = nullptr);
void load_dict(const std::string& db_path, const std::function<void()>& on_finished = nullptr);
void load_name_set(int id);
void reload_dict(const std::function<void()>& on_finished = nullptr);

} // namespace hanvi

using hanvi::sv_readings;
using hanvi::variants;
using hanvi::punctuations;
using hanvi::dictionary;
using hanvi::name_set_dictionary;
using hanvi::current_name_set_id;
using hanvi::name_sets;
using hanvi::init_db;
using hanvi::close_db;
using hanvi::save_db_copy;
using hanvi::db_change_count;
using hanvi::load_dict;
using hanvi::load_name_set;
using hanvi::reload_dict;
