#pragma once

#include <QSqlDatabase>

void load_dict(const std::function<void()>& on_finished);
inline QSqlDatabase db;

