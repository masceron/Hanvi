#include "rulepopup.h"
#include "ui_RulePopup.h"

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
        ui->original_start->setText(rule->original_start);
        ui->original_end->setText(rule->original_end);
        ui->translation_start->setText(rule->translation_start);
        ui->translation_end->setText(rule->translation_end);
    }
}
