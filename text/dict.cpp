#include "dict.h"

#include <QSqlQuery>
#include <QtConcurrent>
#include <QDebug>
#include <QSqlError>

#include "../convert/trie.h"

void init_db()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("dict.db");

    if (!db.open()) {
        qDebug() << db.lastError().text();
        exit(1);
    }

    QSqlQuery query(db);
    query.exec("VACUUM;");
}

void load_data_on_startup(std::function<void()> on_finished)
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

void load_dict(const std::function<void()>& on_finished)
{
    init_db();
    load_data_on_startup(on_finished);
}