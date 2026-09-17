#include <QStyleFactory>
#include <QIcon>
#include <QFontDatabase>
#include <QFile>

#include "app.h"

void init_style(QApplication& app)
{
    QApplication::setWindowIcon(QIcon(":/resources/icon.ico"));
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    const QString font_path = QCoreApplication::applicationDirPath() + "/NotoSansSC.ttf";
    if (QFile::exists(font_path))
    {
        QFontDatabase::addApplicationFont(font_path);
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
