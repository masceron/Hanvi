#ifndef HANVI_NAMESETCHOOSER_HPP
#define HANVI_NAMESETCHOOSER_HPP

#include <QDialog>
#include <QStandardItemModel>

QT_BEGIN_NAMESPACE

namespace Ui
{
    class namesetchooser;
}

QT_END_NAMESPACE

class namesetchooser : public QDialog
{
    Q_OBJECT

public:
    explicit namesetchooser(QWidget* parent = nullptr);
    ~namesetchooser() override;

    int get_chosen_id() const;

private:
    Ui::namesetchooser* ui;
    int chosen_id = -2;

    void load_data(const QString& filter = "");
};

#endif //HANVI_NAMESETCHOOSER_HPP
