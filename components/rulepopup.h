#ifndef CONVERTER_RULEPOPUP_H
#define CONVERTER_RULEPOPUP_H

#include <QDialog>

#include "core/structures.h"


QT_BEGIN_NAMESPACE

namespace Ui
{
    class RulePopup;
}

QT_END_NAMESPACE

class RulePopup : public QDialog
{
    Q_OBJECT

public:
    explicit RulePopup(QWidget* parent = nullptr);
    ~RulePopup() override;
    void load_data(const Rule* rule) const;

private:
    Ui::RulePopup* ui;
};


#endif //CONVERTER_RULEPOPUP_H