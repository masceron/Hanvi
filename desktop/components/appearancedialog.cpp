#include "appearancedialog.h"
#include "ui_appearancedialog.h"

#include <QButtonGroup>

AppearanceDialog::AppearanceDialog(const AppearanceSettings &initial, QWidget *parent)
    : QDialog(parent), ui(new Ui::AppearanceDialog), current_(initial) {
    ui->setupUi(this);

    theme_group_ = new QButtonGroup(this);
    theme_group_->setExclusive(true);
    theme_group_->addButton(ui->theme_dark_btn, static_cast<int>(ThemeId::Dark));
    theme_group_->addButton(ui->theme_sepia_btn, static_cast<int>(ThemeId::Sepia));
    theme_group_->addButton(ui->theme_light_btn, static_cast<int>(ThemeId::Light));

    font_group_ = new QButtonGroup(this);
    font_group_->setExclusive(true);
    font_group_->addButton(ui->font_default_btn, static_cast<int>(FontChoice::Default));
    font_group_->addButton(ui->font_sans_btn, static_cast<int>(FontChoice::Sans));
    font_group_->addButton(ui->font_serif_btn, static_cast<int>(FontChoice::Serif));

    ui->font_size_slider->setRange(AppearanceSettings::kMinFontSize, AppearanceSettings::kMaxFontSize);
    ui->line_height_slider->setRange(AppearanceSettings::kMinLineHeight, AppearanceSettings::kMaxLineHeight);

    update_controls();

    connect(theme_group_, &QButtonGroup::idClicked, this, [this](const int id) {
        current_.theme = static_cast<ThemeId>(id);
        apply_change();
    });

    connect(font_group_, &QButtonGroup::idClicked, this, [this](const int id) {
        current_.font = static_cast<FontChoice>(id);
        apply_change();
    });

    connect(ui->font_size_slider, &QSlider::valueChanged, this, [this](const int value) {
        if (current_.font_size == value) return;
        current_.font_size = value;
        apply_change();
    });

    connect(ui->line_height_slider, &QSlider::valueChanged, this, [this](const int value) {
        if (current_.line_height_percent == value) return;
        current_.line_height_percent = value;
        apply_change();
    });

    connect(ui->reset_btn, &QPushButton::clicked, this, [this] {
        current_ = AppearanceSettings{};
        update_controls();
        apply_change();
    });

    connect(ui->close_btn, &QPushButton::clicked, this, &QDialog::accept);
}

AppearanceDialog::~AppearanceDialog() {
    delete ui;
}

void AppearanceDialog::update_controls() {
    if (auto *btn = theme_group_->button(static_cast<int>(current_.theme))) {
        btn->setChecked(true);
    }
    if (auto *btn = font_group_->button(static_cast<int>(current_.font))) {
        btn->setChecked(true);
    }

    ui->font_size_slider->blockSignals(true);
    ui->font_size_slider->setValue(current_.font_size);
    ui->font_size_slider->blockSignals(false);
    ui->font_size_value->setText(QStringLiteral("%1 px").arg(current_.font_size));

    ui->line_height_slider->blockSignals(true);
    ui->line_height_slider->setValue(current_.line_height_percent);
    ui->line_height_slider->blockSignals(false);
    ui->line_height_value->setText(QStringLiteral("%1%").arg(current_.line_height_percent));

    ui->reset_btn->setEnabled(current_ != AppearanceSettings{});
}

void AppearanceDialog::apply_change() {
    ui->font_size_value->setText(QStringLiteral("%1 px").arg(current_.font_size));
    ui->line_height_value->setText(QStringLiteral("%1%").arg(current_.line_height_percent));
    ui->reset_btn->setEnabled(current_ != AppearanceSettings{});
    emit settings_changed(current_);
}
