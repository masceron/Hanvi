#include <QStyleFactory>
#include <QIcon>
#include <QFontDatabase>
#include <QFile>

#include "app.h"
#include "appearance.h"

QString dict_db_path() {
    const QString path = QCoreApplication::applicationDirPath() + "/dict.db";
    return QFile::exists(path) ? path : "dict.db";
}

void init_style(QApplication &app) {
    QApplication::setWindowIcon(QIcon(":/resources/icon.ico"));
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    for (const auto *font_file : {"NotoSansSC.ttf", "NotoSans-Regular.ttf", "NotoSans-Bold.ttf"}) {
        const QString font_path = QCoreApplication::applicationDirPath() + "/" + font_file;
        if (QFile::exists(font_path)) {
            QFontDatabase::addApplicationFont(font_path);
        } else if (const QString desktop_res = QStringLiteral("desktop/resources/") + font_file; QFile::exists(desktop_res)) {
            QFontDatabase::addApplicationFont(desktop_res);
        } else if (const QString res = QStringLiteral("resources/") + font_file; QFile::exists(res)) {
            QFontDatabase::addApplicationFont(res);
        }
    }

    QFont app_font(QStringLiteral("Noto Sans"));
    app_font.setFamilies({QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans SC")});
    app_font.setStyleHint(QFont::SansSerif);
    QApplication::setFont(app_font);

    const QString css = build_stylesheet(load_appearance().theme);

    app.setStyleSheet(css);
}
