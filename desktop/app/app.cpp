#include <QStyleFactory>
#include <QIcon>
#include <QFontDatabase>
#include <QFile>
#include <QSqlQuery>

#include "app.h"

static void init_desktop_db()
{
    auto db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("dict.db");
    if (db.open())
    {
        QSqlQuery query(db);
        query.exec("PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;");
    }
}

void init_style(QApplication& app)
{
    QApplication::setWindowIcon(QIcon(":/resources/icon.ico"));
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    init_desktop_db();

    const QString font_path = QCoreApplication::applicationDirPath() + "/NotoSansSC.ttf";
    if (QFile::exists(font_path))
    {
        QFontDatabase::addApplicationFont(font_path);
    }
    else if (QFile::exists("desktop/resources/NotoSansSC.ttf"))
    {
        QFontDatabase::addApplicationFont("desktop/resources/NotoSansSC.ttf");
    }
    else if (QFile::exists("resources/NotoSansSC.ttf"))
    {
        QFontDatabase::addApplicationFont("resources/NotoSansSC.ttf");
    }

    const QString css =
    #include "global.qcss"
    ;

    app.setStyleSheet(css);
}
