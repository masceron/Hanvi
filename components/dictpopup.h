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
    void add_new_name();
    void edit_finished_name();
    void edit_finished_phrase();
    ~DictPopup() override;
    Ui::DictPopup* ui;
    ChipDelegate* phrase_chip_delegate;
    static QString capitalize(const QString& text, int mode);
    QString original_name;
};