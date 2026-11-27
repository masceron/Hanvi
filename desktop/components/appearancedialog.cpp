#include "appearancedialog.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

AppearanceDialog::AppearanceDialog(const AppearanceSettings &initial, QWidget *parent)
    : QDialog(parent), current_(initial) {
    setWindowTitle(QStringLiteral("Appearance"));
    resize(420, 320);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(14);

    // Theme row
    auto *theme_label = new QLabel(QStringLiteral("Theme"), this);
    theme_label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    root->addWidget(theme_label);

    auto *theme_row = new QHBoxLayout();
    theme_row->setSpacing(8);
    theme_group_ = new QButtonGroup(this);
    theme_group_->setExclusive(true);

    struct ThemeOption {
        ThemeId id;
        const char *label;
    };
    for (const auto &[id, label] : {
             ThemeOption{ThemeId::Dark, "Dark"},
             ThemeOption{ThemeId::Sepia, "Sepia"},
             ThemeOption{ThemeId::Light, "Light"},
         }) {
        auto *btn = new QPushButton(QString::fromLatin1(label), this);
        btn->setCheckable(true);
        theme_group_->addButton(btn, static_cast<int>(id));
        theme_row->addWidget(btn);
    }
    root->addLayout(theme_row);

    // Font row
    auto *font_label = new QLabel(QStringLiteral("Font"), this);
    font_label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    root->addWidget(font_label);

    auto *font_row = new QHBoxLayout();
    font_row->setSpacing(8);
    font_group_ = new QButtonGroup(this);
    font_group_->setExclusive(true);

    struct FontOption {
        FontChoice id;
        const char *label;
    };
    for (const auto &[id, label] : {
             FontOption{FontChoice::Default, "Default"},
             FontOption{FontChoice::Sans, "Sans"},
             FontOption{FontChoice::Serif, "Serif"},
         }) {
        auto *btn = new QPushButton(QString::fromLatin1(label), this);
        btn->setCheckable(true);
        font_group_->addButton(btn, static_cast<int>(id));
        font_row->addWidget(btn);
    }
    root->addLayout(font_row);

    // Font size slider
    auto *size_header = new QHBoxLayout();
    auto *size_title = new QLabel(QStringLiteral("Font size"), this);
    size_title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    font_size_label_ = new QLabel(this);
    size_header->addWidget(size_title);
    size_header->addStretch(1);
    size_header->addWidget(font_size_label_);
    root->addLayout(size_header);

    font_size_slider_ = new QSlider(Qt::Horizontal, this);
    font_size_slider_->setRange(AppearanceSettings::kMinFontSize, AppearanceSettings::kMaxFontSize);
    font_size_slider_->setSingleStep(1);
    root->addWidget(font_size_slider_);

    // Line spacing slider
    auto *lh_header = new QHBoxLayout();
    auto *lh_title = new QLabel(QStringLiteral("Line spacing"), this);
    lh_title->setStyleSheet(QStringLiteral("font-weight: bold;"));
    line_height_label_ = new QLabel(this);
    lh_header->addWidget(lh_title);
    lh_header->addStretch(1);
    lh_header->addWidget(line_height_label_);
    root->addLayout(lh_header);

    line_height_slider_ = new QSlider(Qt::Horizontal, this);
    line_height_slider_->setRange(AppearanceSettings::kMinLineHeight, AppearanceSettings::kMaxLineHeight);
    line_height_slider_->setSingleStep(5);
    line_height_slider_->setPageStep(10);
    root->addWidget(line_height_slider_);

    root->addStretch(1);

    // Footer buttons
    auto *footer = new QHBoxLayout();
    reset_btn_ = new QPushButton(QStringLiteral("Reset"), this);
    auto *close_btn = new QPushButton(QStringLiteral("Close"), this);
    close_btn->setDefault(true);
    footer->addWidget(reset_btn_);
    footer->addStretch(1);
    footer->addWidget(close_btn);
    root->addLayout(footer);

    update_controls();

    connect(theme_group_, &QButtonGroup::idClicked, this, [this](const int id) {
        current_.theme = static_cast<ThemeId>(id);
        apply_change();
    });

    connect(font_group_, &QButtonGroup::idClicked, this, [this](const int id) {
        current_.font = static_cast<FontChoice>(id);
        apply_change();
    });

    connect(font_size_slider_, &QSlider::valueChanged, this, [this](const int value) {
        if (current_.font_size == value) return;
        current_.font_size = value;
        apply_change();
    });

    connect(line_height_slider_, &QSlider::valueChanged, this, [this](const int value) {
        if (current_.line_height_percent == value) return;
        current_.line_height_percent = value;
        apply_change();
    });

    connect(reset_btn_, &QPushButton::clicked, this, [this] {
        current_ = AppearanceSettings{};
        update_controls();
        apply_change();
    });

    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
}

void AppearanceDialog::update_controls() {
    if (auto *btn = theme_group_->button(static_cast<int>(current_.theme))) {
        btn->setChecked(true);
    }
    if (auto *btn = font_group_->button(static_cast<int>(current_.font))) {
        btn->setChecked(true);
    }
    font_size_slider_->blockSignals(true);
    font_size_slider_->setValue(current_.font_size);
    font_size_slider_->blockSignals(false);
    font_size_label_->setText(QStringLiteral("%1 px").arg(current_.font_size));

    line_height_slider_->blockSignals(true);
    line_height_slider_->setValue(current_.line_height_percent);
    line_height_slider_->blockSignals(false);
    line_height_label_->setText(QStringLiteral("%1%").arg(current_.line_height_percent));

    reset_btn_->setEnabled(current_ != AppearanceSettings{});
}

void AppearanceDialog::apply_change() {
    font_size_label_->setText(QStringLiteral("%1 px").arg(current_.font_size));
    line_height_label_->setText(QStringLiteral("%1%").arg(current_.line_height_percent));
    reset_btn_->setEnabled(current_ != AppearanceSettings{});
    emit settings_changed(current_);
}
