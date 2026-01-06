#pragma once
#include <QListWidget>
#include <QDropEvent>

class ChipList : public QListWidget {
    Q_OBJECT
public:
    using QListWidget::QListWidget;

    signals:
        void order_changed();

protected:
    void dropEvent(QDropEvent *event) override {
        QListWidget::dropEvent(event);

        if (event->isAccepted()) {
            emit order_changed();
        }
    }
};