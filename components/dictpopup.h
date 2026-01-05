//
// Created by qphuc on 1/5/2026.
//

#ifndef CONVERTER_DICTPOPUP_H
#define CONVERTER_DICTPOPUP_H

#include <QDialog>


QT_BEGIN_NAMESPACE

namespace Ui
{
    class DictPopup;
}

QT_END_NAMESPACE

class DictPopup : public QDialog
{
    Q_OBJECT

public:
    explicit DictPopup(QWidget* parent = nullptr);
    ~DictPopup() override;

private:
    Ui::DictPopup* ui;
};


#endif //CONVERTER_DICTPOPUP_H