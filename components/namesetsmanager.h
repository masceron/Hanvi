#ifndef CONVERTER_NAMESETSMANAGER_H
#define CONVERTER_NAMESETSMANAGER_H

#include <QDialog>


QT_BEGIN_NAMESPACE

namespace Ui
{
    class NamesetsManager;
}

QT_END_NAMESPACE

class NamesetsManager : public QDialog
{
    Q_OBJECT

public:
    explicit NamesetsManager(QWidget* parent = nullptr);
    ~NamesetsManager() override;

private:
    Ui::NamesetsManager* ui;
    void load_data();
    QWidget* create_action_widget(int id, const QString& current_title);
    void add_new_name_set();
    void import_set();
};


#endif