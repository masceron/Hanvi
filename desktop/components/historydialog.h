#pragma once

#include <QDialog>
#include <vector>
#include "core/history.h"

class QLabel;
class QPushButton;
class QTableWidget;

// Recent dictionary edits, newest first, each undoable while its entry still holds what the
// edit left there.
class HistoryDialog : public QDialog {
    Q_OBJECT

public:
    explicit HistoryDialog(QWidget *parent = nullptr);

signals:
    void entries_changed();

private:
    void reload(int select_id);

    void undo_selected();

    void clear_history();

    void update_undo_button() const;

    [[nodiscard]] const HistoryAction *selected_action() const;

    QTableWidget *table_ = nullptr;
    QPushButton *undo_button_ = nullptr;
    QPushButton *clear_button_ = nullptr;
    QLabel *status_ = nullptr;
    std::vector<HistoryAction> actions_;
};
