#include <QStyleFactory>
#include <QIcon>

#include "app.h"

void init_style(QApplication& app)
{
    QApplication::setWindowIcon(QIcon(":/resources/icon.ico"));
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    const QString css =
    #include "global.qcss"
    ;

    app.setStyleSheet(css);
}
