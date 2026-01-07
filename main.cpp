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
        loading_screen.close();
        w.show();
    });

    return QApplication::exec();
}
