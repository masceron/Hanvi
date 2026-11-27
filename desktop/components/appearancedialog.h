#pragma once

#include <QDialog>
#include "app/appearance.h"

class QButtonGroup;
class QLabel;
class QPushButton;
class QSlider;

class AppearanceDialog : public QDialog {
    Q_OBJECT

public:
    explicit AppearanceDialog(const AppearanceSettings &initial, QWidget *parent = nullptr);

signals:
    void settings_changed(const AppearanceSettings &settings);

private:
    void update_controls();
    void apply_change();

    AppearanceSettings current_;
    QButtonGroup *theme_group_ = nullptr;
    QButtonGroup *font_group_ = nullptr;
    QSlider *font_size_slider_ = nullptr;
    QSlider *line_height_slider_ = nullptr;
    QLabel *font_size_label_ = nullptr;
    QLabel *line_height_label_ = nullptr;
    QPushButton *reset_btn_ = nullptr;
};
