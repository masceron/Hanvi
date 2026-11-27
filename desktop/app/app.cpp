#include <QStyleFactory>
#include <QIcon>
#include <QFontDatabase>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "app.h"
#include "appearance.h"

QString dict_db_path() {
    const QString path = QCoreApplication::applicationDirPath() + "/dict.db";
    return QFile::exists(path) ? path : "dict.db";
}

namespace {
    // Conversion settings, beside appearance.json.
    QString settings_file_path() {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        return dir.isEmpty() ? QStringLiteral("settings.json") : dir + QStringLiteral("/settings.json");
    }

    QJsonObject read_settings() {
        QFile file(settings_file_path());
        if (!file.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }
}

bool load_frequency_segmentation() {
    return read_settings().value(QStringLiteral("segmentation")).toString() == QStringLiteral("frequency");
}

void save_frequency_segmentation(const bool enabled) {
    QJsonObject settings = read_settings();
    settings.insert(QStringLiteral("segmentation"), enabled ? QStringLiteral("frequency") : QStringLiteral("greedy"));
    const QString path = settings_file_path();
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (QFile file(path); file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(settings).toJson(QJsonDocument::Compact));
    }
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
