#pragma once
#include <QListWidget>
#include <QDropEvent>
#include <QScrollBar>

class ChipList : public QListWidget {
    Q_OBJECT
public:
    using QListWidget::QListWidget;

    signals:
        void order_changed();

protected:
    void wheelEvent(QWheelEvent *event) override {
        if (const int delta = event->angleDelta().y(); delta != 0) {
            QScrollBar *hBar = horizontalScrollBar();
            hBar->setValue(hBar->value() - delta);
        }

        event->accept();
    }
    void dropEvent(QDropEvent *event) override {
        QListWidget::dropEvent(event);

        if (event->isAccepted()) {
            emit order_changed();
        }
    }
};