#pragma once

#include <QDialog>
#include "app/appearance.h"

class QButtonGroup;

QT_BEGIN_NAMESPACE

namespace Ui {
    class AppearanceDialog;
}

QT_END_NAMESPACE

class AppearanceDialog : public QDialog {
    Q_OBJECT

public:
    explicit AppearanceDialog(const AppearanceSettings &initial, QWidget *parent = nullptr);

    ~AppearanceDialog() override;

signals:
    void settings_changed(const AppearanceSettings &settings);

private:
    void update_controls();
    void apply_change();

    Ui::AppearanceDialog *ui;
    AppearanceSettings current_;
    QButtonGroup *theme_group_ = nullptr;
    QButtonGroup *font_group_ = nullptr;
};
