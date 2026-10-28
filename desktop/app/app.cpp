#include <QStyleFactory>
#include <QIcon>
#include <QFontDatabase>
#include <QFile>

#include "app.h"

QString dict_db_path()
{
    const QString path = QCoreApplication::applicationDirPath() + "/dict.db";
    return QFile::exists(path) ? path : "dict.db";
}

void init_style(QApplication& app)
{
    QApplication::setWindowIcon(QIcon(":/resources/icon.ico"));
    QApplication::setStyle(QStyleFactory::create("Fusion"));

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
