#include "historydialog.h"
#include "qt_utils.h"
#include "core/dict.h"
#include "core/io.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
    // Phrases hold several meanings separated by U+001F.
    QString shown(const std::optional<std::u16string> &value) {
        QString text = value ? to_qstring(*value) : QString();
        text.replace(u'\x1F', QStringLiteral(" / "));
        return text;
    }

    QString where(const HistoryAction &action) {
        switch (action.kind) {
            case HistoryKind::Phrase:
                return QStringLiteral("Phrase");
            case HistoryKind::Variant:
                return QStringLiteral("Variant");
            case HistoryKind::Name:
                break;
        }
        if (action.set_id == -1) return QStringLiteral("Name");
        for (const auto &[index, title]: name_sets) {
            if (index == action.set_id) return QStringLiteral("Nameset “%1”").arg(to_qstring(title));
        }
        return QStringLiteral("Deleted nameset");
    }

    QString describe(const HistoryAction &action) {
        switch (action.change) {
            case HistoryChange::Add:
                return QStringLiteral("Added: %1").arg(shown(action.after));
            case HistoryChange::Delete:
                return QStringLiteral("Deleted: %1").arg(shown(action.before));
            case HistoryChange::Reorder:
                return QStringLiteral("Reordered: %1").arg(shown(action.after));
            case HistoryChange::AddMeaning:
                return QStringLiteral("Added meaning: %1").arg(to_qstring(action.meaning));
            case HistoryChange::RemoveMeaning:
                return QStringLiteral("Removed meaning: %1").arg(to_qstring(action.meaning));
            case HistoryChange::Edit:
                break;
        }
        return QStringLiteral("%1 → %2").arg(shown(action.before), shown(action.after));
    }

    QString state_text(const HistoryAction &action) {
        switch (action.state) {
            case UndoState::Undoable:
                return {};
            case UndoState::Undone:
                return QStringLiteral("Undone");
            case UndoState::ChangedLater:
                return QStringLiteral("Changed again by #%1").arg(action.changed_by);
            case UndoState::ChangedOutside:
                return QStringLiteral("Changed outside Hanvi");
            case UndoState::NameSetDeleted:
                return QStringLiteral("Nameset deleted");
        }
        return {};
    }

    QString when(const int64_t time) {
        const QDateTime local = QDateTime::fromSecsSinceEpoch(time).toLocalTime();
        return local.date() == QDate::currentDate()
                   ? local.toString(QStringLiteral("HH:mm"))
                   : local.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    }
} // namespace

HistoryDialog::HistoryDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("Recent Changes"));
    resize(820, 520);

    table_ = new QTableWidget(0, 5, this);
    table_->setObjectName(QStringLiteral("history_table"));
    table_->setHorizontalHeaderLabels({
        QStringLiteral("#"), QStringLiteral("When"), QStringLiteral("Entry"), QStringLiteral("Change"),
        QStringLiteral("Status")
    });
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setWordWrap(false);
    auto *header = table_->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::Stretch);

    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("history_status"));
    clear_button_ = new QPushButton(QStringLiteral("Clear History"), this);
    clear_button_->setObjectName(QStringLiteral("clear_history_button"));
    undo_button_ = new QPushButton(QStringLiteral("Undo"), this);
    undo_button_->setObjectName(QStringLiteral("undo_button"));
    auto *close_button = new QPushButton(QStringLiteral("Close"), this);
    close_button->setObjectName(QStringLiteral("close_button"));

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(status_, 1);
    buttons->addWidget(clear_button_);
    buttons->addWidget(undo_button_);
    buttons->addWidget(close_button);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(table_);
    layout->addLayout(buttons);

    connect(undo_button_, &QPushButton::clicked, this, &HistoryDialog::undo_selected);
    connect(clear_button_, &QPushButton::clicked, this, &HistoryDialog::clear_history);
    connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
    connect(table_, &QTableWidget::itemSelectionChanged, this, &HistoryDialog::update_undo_button);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &HistoryDialog::undo_selected);

    reload(0);
}

void HistoryDialog::reload(const int select_id) {
    actions_ = history_list();
    table_->setRowCount(static_cast<int>(actions_.size()));
    int select_row = actions_.empty() ? -1 : 0;
    for (int row = 0; row < static_cast<int>(actions_.size()); ++row) {
        const auto &action = actions_[row];
        const QStringList cells = {
            QString::number(action.id), when(action.time),
            QStringLiteral("%1  %2").arg(where(action), to_qstring(action.original)),
            describe(action), state_text(action)
        };
        for (int column = 0; column < cells.size(); ++column) {
            auto *item = new QTableWidgetItem(cells[column]);
            if (column == 3) item->setToolTip(cells[column]);
            if (action.state != UndoState::Undoable) item->setForeground(QColor(0x80, 0x80, 0x8c));
            table_->setItem(row, column, item);
        }
        if (action.id == select_id) select_row = row;
    }
    if (select_row != -1) table_->selectRow(select_row);
    status_->setText(actions_.empty()
                         ? QStringLiteral("No changes yet.")
                         : QStringLiteral("The last %1 changes are kept.").arg(hanvi::HISTORY_LIMIT));
    clear_button_->setEnabled(!actions_.empty());
    update_undo_button();
}

const HistoryAction *HistoryDialog::selected_action() const {
    const auto rows = table_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return nullptr;
    const int row = rows.first().row();
    return row < static_cast<int>(actions_.size()) ? &actions_[row] : nullptr;
}

void HistoryDialog::update_undo_button() const {
    const auto *action = selected_action();
    undo_button_->setEnabled(action && action->state == UndoState::Undoable);
}

void HistoryDialog::undo_selected() {
    const auto *action = selected_action();
    if (!action || action->state != UndoState::Undoable) return;

    const int id = action->id;
    if (io_undo(id) == UndoState::Undoable) emit entries_changed();
    // Reloaded either way: the entry may have changed since the list was shown.
    reload(id);
}

void HistoryDialog::clear_history() {
    if (QMessageBox::question(this, QStringLiteral("Clear History"),
                              QStringLiteral("Forget every recorded change? The dictionary stays as it is, but none "
                                             "of these changes can be undone afterwards."),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    history_clear();
    reload(0);
}
