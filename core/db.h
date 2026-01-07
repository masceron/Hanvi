#pragma once
#include "trie.h"

void db_insert(const QString& key, const QString& value, Priority priority);
void db_reorder(const QString& key, const QStringList& new_order, Priority priority);
void db_remove(const QString& key, Priority priority);
void db_remove_meaning(const QString& key, const QString& value, Priority priority);