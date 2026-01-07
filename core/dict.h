#pragma once

#include <QSqlDatabase>

void load_dict(const std::function<void()>& on_finished);
void load_name_set(int id);
void delete_name_set(int id);
