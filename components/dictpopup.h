#pragma once

#include <QDialog>

#include "chipdelegate.h"
#include "mainwindow.h"

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
    void load_data(const QString& selected_chinese_text) const;

private:
    void close_popup();
    bool changed = false;
    void add_new_phrase() const;
    void edit_finished_phrase();
    void add_new_name() const;
    void edit_finished_name();
    ~DictPopup() override;
    Ui::DictPopup* ui;
    ChipDelegate* phrase_chip_delegate;
    ChipDelegate* name_chip_delegate;
    void capitalize(int type, int mode) const;
};