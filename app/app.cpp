#include <QStyleFactory>
#include <QIcon>
#include <QFontDatabase>

#include "app.h"

void init_style(QApplication& app)
{
    QApplication::setWindowIcon(QIcon(":/resources/icon.ico"));
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QFontDatabase::addApplicationFont(QCoreApplication::applicationDirPath() + "/NotoSansSC.ttf");

    const QString css =
    #include "global.qcss"
    ;

    app.setStyleSheet(css);
}
