#include "../core/db.h"

#include <QSqlQuery>

#include "trie.h"

QString get_table_name(const Priority priority)
{
    return (priority == NAME) ? "names" : "phrases";
}

void db_insert(const QString& key, const QString& value, const Priority priority)
{
    if (priority == NONE) return;
    const QString table = get_table_name(priority);
    const QString separator = "\x1F";

    QSqlQuery q;
    q.prepare("SELECT translated FROM " + table + " WHERE original = :key");
    q.bindValue(":key", key);

    if (q.exec() && q.next())
    {
        const QString raw = q.value(0).toString();
        QStringList list = raw.split(separator, Qt::SkipEmptyParts);

        list.removeAll(value);
        list.prepend(value);

        QSqlQuery update;
        update.prepare("UPDATE " + table + " SET translated = :val WHERE original = :key");
        update.bindValue(":val", list.join(separator));
        update.bindValue(":key", key);
        update.exec();
    }
    else
    {
        QSqlQuery insert;
        insert.prepare("INSERT INTO " + table + " (original, translated) VALUES (:key, :val)");
        insert.bindValue(":key", key);
        insert.bindValue(":val", value);
        insert.exec();
    }
}

void db_reorder(const QString& key, const QStringList& new_order)
{
    const QString separator = "\x1F";

    QSqlQuery q;
    q.prepare("INSERT OR REPLACE INTO phrases (original, translated) VALUES (:key, :val)");
    q.bindValue(":key", key);
    q.bindValue(":val", new_order.join(separator));

    q.exec();
}

void db_remove(const QString& key, const Priority priority)
{
    if (priority == NONE) return;
    const QString table = get_table_name(priority);

    QSqlQuery q;
    q.prepare("DELETE FROM " + table + " WHERE original = :key");
    q.bindValue(":key", key);

    q.exec();
}

void db_remove_meaning(const QString& key, const QString& value)
{
    const QString separator = "\x1F";

    QSqlQuery q;
    q.prepare("SELECT translated FROM phrases WHERE original = :key");
    q.bindValue(":key", key);

    if (q.exec() && q.next())
    {
        const QString raw = q.value(0).toString();

        if (QStringList list = raw.split(separator, Qt::SkipEmptyParts); list.removeAll(value) > 0)
        {
            if (list.isEmpty())
            {
                QSqlQuery del;
                del.prepare("DELETE FROM phrases WHERE original = :key");
                del.bindValue(":key", key);
                del.exec();
            }
            else
            {
                QSqlQuery update;
                update.prepare("UPDATE phrases SET translated = :val WHERE original = :key");
                update.bindValue(":val", list.join(separator));
                update.bindValue(":key", key);
                update.exec();
            }
        }
    }
}

void nameset_db_insert(const QString& key, const QString& value)
{
    QSqlQuery q;
    q.prepare(
        R"(INSERT INTO name_set_entries (original, set_id, translated)
                    VALUES (:original, :set_id, :translated)
                    ON CONFLICT (set_id, original)
                    DO UPDATE SET translated = excluded.translated)"
        );

    q.bindValue(":original", key);
    q.bindValue(":set_id", current_name_set_id);
    q.bindValue(":translated", value);

    q.exec();
}

void nameset_db_remove(const QString& key)
{
    QSqlQuery q;
    q.prepare("DELETE FROM name_set_entries WHERE set_id = :set_id AND original = :original");
    q.bindValue(":set_id", current_name_set_id);
    q.bindValue(":original", key);

    q.exec();
}
