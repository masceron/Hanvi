#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
    class findbar;
}
QT_END_NAMESPACE

class findbar : public QWidget {
    Q_OBJECT

public:
    explicit findbar(QWidget *parent = nullptr);
    ~findbar() override;

    void set_matches_count(int current, int total) const;
    void focus_input() const;
    void set_search_text(const QString& text) const;
    [[nodiscard]] QString search_text() const;

signals:
    void search_requested(const QString& text);
    void next_requested();
    void previous_requested();
    void closed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Ui::findbar *ui;
};
