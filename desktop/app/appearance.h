#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QString>

enum class ThemeId { Dark, Sepia, Light };
enum class FontChoice { Default, Sans, Serif };

struct AppearanceSettings {
    ThemeId theme = ThemeId::Dark;
    FontChoice font = FontChoice::Default;
    int font_size = 16;
    int line_height_percent = 125;

    static constexpr int kMinFontSize = 12;
    static constexpr int kMaxFontSize = 28;
    static constexpr int kMinLineHeight = 100;
    static constexpr int kMaxLineHeight = 200;

    bool operator==(const AppearanceSettings &) const = default;
};

struct ThemeColors {
    QColor pane_bg{0x2d, 0x2d, 0x2d};
    QColor text{0xff, 0xff, 0xff};
    QColor selection_bg{0x21, 0x42, 0x83};
    QColor selection_text{0xff, 0xff, 0xff};
    QColor token_highlight{255, 68, 68};
    QColor override_underline{0x72, 0x75, 0xfb};
    QColor search_bg{160, 110, 20, 180};
    QColor search_text{0xff, 0xff, 0xff};
    QColor search_active_bg{255, 150, 0};
    QColor search_active_text{0x00, 0x00, 0x00};
    QString icon_color{QStringLiteral("#cfcfd8")};
};

[[nodiscard]] AppearanceSettings load_appearance();
void save_appearance(const AppearanceSettings &settings);

[[nodiscard]] ThemeColors theme_colors_for(ThemeId theme);
[[nodiscard]] QString build_stylesheet(ThemeId theme);

[[nodiscard]] QFont reader_font_cn(const AppearanceSettings &settings);
[[nodiscard]] QFont reader_font_sv(const AppearanceSettings &settings);
[[nodiscard]] QFont reader_font_vn(const AppearanceSettings &settings);

[[nodiscard]] QIcon themed_svg_icon(const QString &resource_path, const QString &color,
                                      const QString &hover_resource_path = {},
                                      const QString &hover_color = {});
