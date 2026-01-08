#include "app/app.h"
#include "components/loader.h"
#include "components/mainwindow.h"
#include "core/dict.h"

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    init_style(a);

    Loader loading_screen;
    loading_screen.show();

    MainWindow w;

    load_dict([&]
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
