#include "rulepopup.h"
#include "ui_RulePopup.h"
#include "core/structures.h"

RulePopup::RulePopup(QWidget* parent) :
    QDialog(parent), ui(new Ui::RulePopup)
{
    ui->setupUi(this);
    setWindowTitle("Grammar rule");

    connect(ui->close_popup, &QPushButton::clicked, this, [this]
    {
        accept();
    });
}

RulePopup::~RulePopup()
{
    delete ui;
}

void RulePopup::load_data(const Rule* rule) const
{
    if (rule != nullptr)
    {
        ui->original_start->setText(QString::fromUtf16(rule->original_start.data(), static_cast<qsizetype>(rule->original_start.size())));
        ui->original_end->setText(QString::fromUtf16(rule->original_end.data(), static_cast<qsizetype>(rule->original_end.size())));
        ui->translation_start->setText(QString::fromUtf16(rule->translation_start.data(), static_cast<qsizetype>(rule->translation_start.size())));
        ui->translation_end->setText(QString::fromUtf16(rule->translation_end.data(), static_cast<qsizetype>(rule->translation_end.size())));
    }
}
