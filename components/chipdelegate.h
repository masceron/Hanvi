#pragma once
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QPainter>
#include <QMouseEvent>

class ChipDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    mutable QLineEdit* current_editor = nullptr;
    QLineEdit* get_current_editor() const { return current_editor; }
    bool editing() const { return current_editor != nullptr; }
    void set_editor_text(const QString &text) const
    {
        if (current_editor) {
            current_editor->setText(text);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QString text = index.data().toString();
        if (text.isEmpty()) text = "New Meaning";
        const int width = option.fontMetrics.horizontalAdvance(text) + 50;
        return QSize(width, 36);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (!index.isValid()) return;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const QRect rect = option.rect.adjusted(2, 4, -2, -4);

        const auto bg_color = QColor("#313131");
        painter->setBrush(bg_color);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(rect, 14, 14);

        constexpr int x_icon_size = 10;
        constexpr int x_button_width = 24;

        const int x_icon_top = rect.top() + (rect.height() - x_icon_size) / 2;

        const auto x_rect = QRect(rect.right() - x_button_width, rect.top(), x_button_width, rect.height());

        const auto xDrawRect = QRect(x_rect.center().x() - x_icon_size/2, x_icon_top, x_icon_size, x_icon_size);

        painter->setPen(Qt::white);

        const QRect text_rect = rect.adjusted(12, 0, -32, 0);

        painter->drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, index.data().toString());

        QPen xPen(QColor("#ddd"));
        xPen.setWidth(2);
        painter->setPen(xPen);

        painter->drawLine(xDrawRect.topLeft(), xDrawRect.bottomRight());
        painter->drawLine(xDrawRect.topRight(), xDrawRect.bottomLeft());

        painter->restore();
    }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &/*option*/, const QModelIndex &/*index*/) const override
    {
        const auto editor = new QLineEdit(parent);

        editor->setStyleSheet("QLineEdit { "
                              "border: 2px solid #313131; "
                              "border-radius: 14px; "
                              "background-color: #313131; "
                              "color: white; "
                              "padding-left: 10px; "
                              "selection-background-color: white; "
                              "selection-color: #313131; "
                              "}");

        current_editor = editor;
        emit const_cast<ChipDelegate*>(this)->editing_started();
        return editor;
    }

    void destroyEditor(QWidget *editor, const QModelIndex &index) const override
    {
        current_editor = nullptr;
        emit const_cast<ChipDelegate*>(this)->editing_finished();
        QStyledItemDelegate::destroyEditor(editor, index);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override
    {
        const auto line = static_cast<QLineEdit*>(editor);
        line->setText(index.data().toString());
    }

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override
    {
        const QLineEdit *line = static_cast<QLineEdit*>(editor);
        const QString value = line->text();
        model->setData(index, value, Qt::EditRole);
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/*index*/) const override
    {
        const QRect rect = option.rect.adjusted(2, 4, -2, -4);
        editor->setGeometry(rect);
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            const QMouseEvent *me = static_cast<QMouseEvent*>(event);
            const QRect rect = option.rect.adjusted(2, 4, -2, -4);
            if (const auto xRect = QRect(rect.right() - 22, rect.center().y() - 10, 22, 22); xRect.contains(me->pos())) {
                emit request_delete(index);
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

signals:
    void editing_started();
    void editing_finished();
    void request_delete(const QModelIndex &index);
};