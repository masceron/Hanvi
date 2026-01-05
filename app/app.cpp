#include <QStyleFactory>

#include "app.h"

void init_app()
{

}

void init_style(QApplication& app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    const QString css =
    #include "global.qcss"
    ;

    app.setStyleSheet(css);
}
