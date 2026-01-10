#pragma once

#include <QSqlDatabase>

#include "structures.h"

inline QHash<QChar, QString> sv_readings;
inline QHash<QChar, QChar> punctuations;
inline Dictionary dictionary;
inline Dictionary name_set_dictionary;
inline int current_name_set_id = -1;
inline std::vector<NameSet> name_sets;

void load_dict(const std::function<void()>& on_finished);
void load_name_set(int id);
