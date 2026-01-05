#include "dictpopup.h"
#include "ui_DictPopup.h"


DictPopup::DictPopup(QWidget* parent) :
    QDialog(parent), ui(new Ui::DictPopup)
{
    ui->setupUi(this);
}

DictPopup::~DictPopup()
{
    delete ui;
}