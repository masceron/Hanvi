#include "dict.h"

#include <QSqlQuery>
#include <QtConcurrent>
#include <QDebug>
#include <QSqlError>

#include "trie.h"

void init_db()
{
    auto db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("dict.db");

    if (!db.open()) {
        qDebug() << db.lastError().text();
        exit(1);
    }

    QSqlQuery query(db);
    query.exec("PRAGMA foreign_keys = ON;");
    query.exec("VACUUM;");
}

void load_data_on_startup(const std::function<void()>& on_finished)
{
    QFuture<void> future_sv = QtConcurrent::run([]
    {
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "SV_thread");
            db.setDatabaseName("dict.db");
            if (db.open()) {
                QSqlQuery query(db);
                query.setForwardOnly(true);
                query.exec("SELECT original, translated FROM sv_readings");
                while (query.next()) {
                    QChar key = query.value(0).toString().at(0);
                    QString val = query.value(1).toString();
                    sv_readings.insert(key, val);
                }
                db.close();
            }
        }
        QSqlDatabase::removeDatabase("SV_thread");
    });

    QFuture<void> future_punc = QtConcurrent::run([]
    {
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "P_thread");
            db.setDatabaseName("dict.db");
            if (db.open()) {
                QSqlQuery query(db);
                query.setForwardOnly(true);
                query.exec("SELECT original, normalized FROM punctuations");
                while (query.next()) {
                    const QChar key = query.value(0).toString().at(0);
                    const QChar val = query.value(1).toString().at(0);
                    punctuations.insert(key, val);
                }
                db.close();
            }
        }
        QSqlDatabase::removeDatabase("P_thread");
    });

    QFuture<void> future_trie = QtConcurrent::run([]
    {
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "NP_thread");
            db.setDatabaseName("dict.db");
            if (db.open()) {
                QSqlQuery query(db);
                query.setForwardOnly(true);

                query.exec("SELECT original, translated FROM names");
                while (query.next()) {
                    dictionary.insert_bulk(query.value(0).toString(), NAME, query.value(1).toString());
                }

                query.exec("SELECT original, translated FROM phrases");
                while (query.next()) {
                    dictionary.insert_bulk(query.value(0).toString(), PHRASE, query.value(1).toString());
                }
                db.close();
            }
        }
        QSqlDatabase::removeDatabase("NP_thread");
    });

    const QFuture<void> master_future = QtConcurrent::run([future_sv, future_punc, future_trie]() mutable {
        future_sv.waitForFinished();
        future_punc.waitForFinished();
        future_trie.waitForFinished();
    });

    auto* watcher = new QFutureWatcher<void>();

    QObject::connect(watcher, &QFutureWatcher<void>::finished, [watcher, on_finished]() {
        if (on_finished) on_finished();

        watcher->deleteLater();
    });

    watcher->setFuture(master_future);
}

void load_name_sets_data()
{
    QSqlQuery query;
    query.prepare("SELECT id, title FROM name_sets");
    query.exec();
    while (query.next())
    {
        int id = query.value(0).toInt();
        QString title = query.value(1).toString();
        name_sets.emplace_back(id, title);
    }
}

void load_dict(const std::function<void()>& on_finished)
{
    init_db();
    load_name_sets_data();
    load_data_on_startup(on_finished);
}

void load_name_set(const int id)
{
    current_name_set_id = id;
    name_set_dictionary = Dictionary();

    if (id == -1) return;

    QSqlQuery query;
    query.prepare("SELECT original, translated FROM name_set_entries WHERE set_id = :id");
    query.bindValue(":id", id);

    if (query.exec()) {
        while (query.next()) {
            QString key = query.value(0).toString();
            QString val = query.value(1).toString();
            name_set_dictionary.insert_bulk(key, NAME, val);
        }
    }
}