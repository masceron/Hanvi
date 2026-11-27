#include "appearance.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QPixmap>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>
#include <array>
#include <utility>

namespace {
    QString appearance_file_path() {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        return dir.isEmpty() ? QStringLiteral("appearance.json") : dir + QStringLiteral("/appearance.json");
    }

    QString theme_to_string(const ThemeId theme) {
        switch (theme) {
            case ThemeId::Dark: return QStringLiteral("dark");
            case ThemeId::Sepia: return QStringLiteral("sepia");
            case ThemeId::Light: return QStringLiteral("light");
        }
        return QStringLiteral("dark");
    }

    ThemeId theme_from_string(const QString &value) {
        if (value == QLatin1String("sepia")) return ThemeId::Sepia;
        if (value == QLatin1String("light")) return ThemeId::Light;
        return ThemeId::Dark;
    }

    QString font_to_string(const FontChoice font) {
        switch (font) {
            case FontChoice::Default: return QStringLiteral("default");
            case FontChoice::Sans: return QStringLiteral("sans");
            case FontChoice::Serif: return QStringLiteral("serif");
        }
        return QStringLiteral("default");
    }

    FontChoice font_from_string(const QString &value) {
        if (value == QLatin1String("sans")) return FontChoice::Sans;
        if (value == QLatin1String("serif")) return FontChoice::Serif;
        return FontChoice::Default;
    }

    QPixmap svg_pixmap(const QString &resource_path, const QString &color) {
        QFile file(resource_path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        QByteArray data = file.readAll();
        // Normal icons are drawn in #cfcfd8, hover variants in #ffffff; each file uses one of them.
        data.replace("#cfcfd8", color.toUtf8());
        data.replace("#ffffff", color.toUtf8());
        QPixmap pixmap;
        pixmap.loadFromData(data, "SVG");
        return pixmap;
    }

    using ColorMap = std::array<std::pair<const char *, const char *>, 30>;

    // Remaps every colour in a single pass, so one entry's output is never remapped again by a
    // later entry (e.g. #2d2d2d -> #ffffff must not then become #ffffff -> #18181b).
    QString remap_colors(const QString &text, const ColorMap &map) {
        static const QRegularExpression token(
            QStringLiteral(R"(#[0-9a-fA-F]{6}(?![0-9a-fA-F])|color:\s*white;)"));
        QString out;
        out.reserve(text.size());
        qsizetype last = 0;
        for (auto it = token.globalMatch(text); it.hasNext();) {
            const QRegularExpressionMatch m = it.next();
            out += QStringView(text).mid(last, m.capturedStart() - last);
            const QString found = m.captured().toLower();
            const auto entry = std::ranges::find_if(map, [&](const auto &e) {
                return found == QLatin1String(e.first);
            });
            out += entry != map.end() ? QString::fromLatin1(entry->second) : m.captured();
            last = m.capturedEnd();
        }
        out += QStringView(text).mid(last);
        return out;
    }

    // Light themes map white text to dark; these keep white where it sits on an accent or red
    // background. Appended after remapping so they are not remapped themselves.
    constexpr auto kLightThemeWhiteText = R"(
QPushButton:checked { color: #ffffff; }
QPushButton#window_close_btn:hover, QPushButton#window_close_btn:pressed { color: #ffffff; }
QPushButton#delete_button:hover, QPushButton#clear_history_button:hover,
QPushButton#add_button:hover, QPushButton#edit_button:hover, QPushButton#undo_button:hover,
QPushButton#entries_set:hover, QPushButton#edit_set:hover, QPushButton#dump_set:hover,
QPushButton#choose_set:hover, QPushButton#delete_set:hover, QPushButton#name_set_choose_none:hover,
QPushButton#current_name_set:hover, QPushButton#add_name:hover, QPushButton#add_phrase:hover,
QPushButton#delete_node_phrase:hover, QPushButton#delete_name:hover, QPushButton#close_popup:hover,
findbar QPushButton#search_button:hover, findbar QPushButton#pushButton:hover,
findbar QPushButton#close:hover { color: #ffffff; }
)";
} // namespace

AppearanceSettings load_appearance() {
    AppearanceSettings settings;
    QFile file(appearance_file_path());
    if (!file.open(QIODevice::ReadOnly)) return settings;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return settings;

    const QJsonObject obj = doc.object();
    settings.theme = theme_from_string(obj.value(QStringLiteral("theme")).toString());
    settings.font = font_from_string(obj.value(QStringLiteral("font")).toString());
    if (obj.contains(QStringLiteral("fontSize"))) {
        settings.font_size = std::clamp(obj.value(QStringLiteral("fontSize")).toInt(settings.font_size),
                                        AppearanceSettings::kMinFontSize,
                                        AppearanceSettings::kMaxFontSize);
    }
    if (obj.contains(QStringLiteral("lineHeightPercent"))) {
        settings.line_height_percent = std::clamp(
            obj.value(QStringLiteral("lineHeightPercent")).toInt(settings.line_height_percent),
            AppearanceSettings::kMinLineHeight,
            AppearanceSettings::kMaxLineHeight);
    }
    return settings;
}

void save_appearance(const AppearanceSettings &settings) {
    const QString path = appearance_file_path();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;

    QJsonObject obj;
    obj.insert(QStringLiteral("theme"), theme_to_string(settings.theme));
    obj.insert(QStringLiteral("font"), font_to_string(settings.font));
    obj.insert(QStringLiteral("fontSize"), settings.font_size);
    obj.insert(QStringLiteral("lineHeightPercent"), settings.line_height_percent);
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

ThemeColors theme_colors_for(const ThemeId theme) {
    switch (theme) {
        case ThemeId::Dark:
            return {};
        case ThemeId::Sepia:
            return {
                .pane_bg = QColor(0xf4, 0xec, 0xd8),
                .text = QColor(0x3b, 0x2f, 0x20),
                .selection_bg = QColor(0xc8, 0xd8, 0xf0),
                .selection_text = QColor(0x1f, 0x19, 0x12),
                .token_highlight = QColor(0xc4, 0x2b, 0x1c),
                .override_underline = QColor(0x54, 0x57, 0xe5),
                .search_bg = QColor(226, 173, 56, 120),
                .search_text = QColor(0x2a, 0x20, 0x13),
                .search_active_bg = QColor(0xeb, 0x8c, 0x00),
                .search_active_text = QColor(0xff, 0xff, 0xff),
                .icon_color = QStringLiteral("#564532"),
            };
        case ThemeId::Light:
            return {
                .pane_bg = QColor(0xff, 0xff, 0xff),
                .text = QColor(0x18, 0x18, 0x1b),
                .selection_bg = QColor(0xcf, 0xe0, 0xfc),
                .selection_text = QColor(0x11, 0x18, 0x27),
                .token_highlight = QColor(0xd9, 0x2d, 0x20),
                .override_underline = QColor(0x54, 0x57, 0xe5),
                .search_bg = QColor(250, 204, 21, 120),
                .search_text = QColor(0x18, 0x18, 0x1b),
                .search_active_bg = QColor(0xf5, 0x9e, 0x0b),
                .search_active_text = QColor(0xff, 0xff, 0xff),
                .icon_color = QStringLiteral("#3f3f46"),
            };
    }
    return {};
}

QString build_stylesheet(const ThemeId theme) {
    QString css = QString::fromUtf8(
#include "global.qcss"
    );

    if (theme == ThemeId::Dark) {
        return css;
    }

    if (theme == ThemeId::Sepia) {
        constexpr ColorMap sepia_map{{
            {"#1e1e1e", "#dfd2b8"},
            {"#1e1e24", "#ede2cb"},
            {"#1c1b22", "#e6d8be"},
            {"#222129", "#efe4ce"},
            {"#22222a", "#e4d7be"},
            {"#25252e", "#ede1c9"},
            {"#2d2d2d", "#f4ecd8"},
            {"#2b2b2b", "#eadec6"},
            {"#2b2b36", "#e3d5b8"},
            {"#2b2a33", "#e4d6bb"},
            {"#383838", "#c9b796"},
            {"#3c3b46", "#cfc0a4"},
            {"#3b3b4a", "#c9b99b"},
            {"#42414d", "#d6c5a3"},
            {"#52525e", "#cbb894"},
            {"#5f5e6b", "#bfa984"},
            {"#575757", "#dac9aa"},
            {"#7d7d7d", "#cbb894"},
            {"#555555", "#b9a786"},
            {"#555560", "#a58f69"},
            {"#666675", "#8c7a62"},
            {"#9a9aa6", "#7b6953"},
            {"#cfcfd8", "#564532"},
            {"#b2beb5", "#8c7a62"},
            {"#ffffff", "#3b2f20"},
            {"color: white;", "color: #3b2f20;"},
            {"#214283", "#c8d8f0"},
            {"#6164f6", "#5457e5"},
            {"#4b4ec4", "#4447d0"},
            {"#61ccff", "#5457e5"},
        }};
        css = remap_colors(css, sepia_map);
        css += QStringLiteral("\nQTabBar::tab:selected { color: #3b2f20; }");
        css += QLatin1String(kLightThemeWhiteText);
        return css;
    }

    constexpr ColorMap light_map{{
        {"#1e1e1e", "#e4e6ec"},
        {"#1e1e24", "#f8f9fb"},
        {"#1c1b22", "#f1f2f6"},
        {"#222129", "#f8f9fb"},
        {"#22222a", "#eceef3"},
        {"#25252e", "#f1f3f7"},
        {"#2d2d2d", "#ffffff"},
        {"#2b2b2b", "#eef0f4"},
        {"#2b2b36", "#e7e9ef"},
        {"#2b2a33", "#eceef3"},
        {"#383838", "#c5cad6"},
        {"#3c3b46", "#d4d7e0"},
        {"#3b3b4a", "#d4d7e0"},
        {"#42414d", "#dfe2ec"},
        {"#52525e", "#cdd2de"},
        {"#5f5e6b", "#bcc2d0"},
        {"#575757", "#e2e5ed"},
        {"#7d7d7d", "#d0d5e0"},
        {"#555555", "#bcc2d0"},
        {"#555560", "#9ea5b3"},
        {"#666675", "#8a909e"},
        {"#9a9aa6", "#71717a"},
        {"#cfcfd8", "#3f3f46"},
        {"#b2beb5", "#9ca3af"},
        {"#ffffff", "#18181b"},
        {"color: white;", "color: #18181b;"},
        {"#214283", "#cfe0fc"},
        {"#6164f6", "#5457e5"},
        {"#4b4ec4", "#4447d0"},
        {"#61ccff", "#5457e5"},
    }};
    css = remap_colors(css, light_map);
    css += QStringLiteral("\nQTabBar::tab:selected { color: #18181b; }");
    css += QLatin1String(kLightThemeWhiteText);
    return css;
}

QFont reader_font_cn(const AppearanceSettings &settings) {
    QFont font;
    if (settings.font == FontChoice::Serif) {
        font.setFamilies({QStringLiteral("Noto Serif SC"), QStringLiteral("SimSun"), QStringLiteral("Noto Sans SC")});
        font.setStyleHint(QFont::Serif);
    } else {
        font.setFamilies({QStringLiteral("Noto Sans SC")});
        font.setStyleHint(QFont::SansSerif);
    }
    font.setPixelSize(settings.font_size + 2);
    return font;
}

QFont reader_font_sv(const AppearanceSettings &settings) {
    QFont font;
    if (settings.font == FontChoice::Serif) {
        font.setFamilies({QStringLiteral("Georgia"), QStringLiteral("Times New Roman"), QStringLiteral("Noto Sans SC")});
        font.setStyleHint(QFont::Serif);
    } else {
        font.setFamilies({QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans SC")});
        font.setStyleHint(QFont::SansSerif);
    }
    font.setPixelSize(settings.font_size);
    return font;
}

QFont reader_font_vn(const AppearanceSettings &settings) {
    QFont font;
    if (settings.font == FontChoice::Serif) {
        font.setFamilies({QStringLiteral("Georgia"), QStringLiteral("Times New Roman"), QStringLiteral("Noto Sans SC")});
        font.setStyleHint(QFont::Serif);
    } else {
        font.setFamilies({QStringLiteral("Noto Sans"), QStringLiteral("Noto Sans SC")});
        font.setStyleHint(QFont::SansSerif);
    }
    font.setPixelSize(settings.font_size);
    return font;
}

QIcon themed_svg_icon(const QString &resource_path, const QString &color,
                      const QString &hover_resource_path, const QString &hover_color) {
    QIcon icon;
    if (const QPixmap normal = svg_pixmap(resource_path, color); !normal.isNull()) {
        icon.addPixmap(normal, QIcon::Normal);
    } else {
        icon.addFile(resource_path, QSize(), QIcon::Normal);
    }

    if (!hover_resource_path.isEmpty()) {
        const QString &active_color = hover_color.isEmpty() ? color : hover_color;
        if (const QPixmap active = svg_pixmap(hover_resource_path, active_color); !active.isNull()) {
            icon.addPixmap(active, QIcon::Active);
        } else {
            icon.addFile(hover_resource_path, QSize(), QIcon::Active);
        }
    }
    return icon;
}
