#include "app/app.h"
#include "components/loader.h"
#include "components/mainwindow.h"
#include "core/converter.h"
#include "core/dict.h"

#include <QFile>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    init_style(a);

    if (const QString opencc = QCoreApplication::applicationDirPath() + "/opencc/t2s.json"; QFile::exists(opencc))
    {
        init_opencc(opencc.toLocal8Bit().toStdString());
    }

    Loader loading_screen;
    loading_screen.show();

    MainWindow w;

    load_dict(dict_db_path().toStdString(), [&]
    {
        QMetaObject::invokeMethod(&w, [&]()
        {
            loading_screen.close();
            w.load_data();
            w.show();
        }, Qt::QueuedConnection);
    });

    return QApplication::exec();
}
