#pragma once

#include <QDialog>
#include <optional>

#include "chipdelegate.h"
#include "mainwindow.h"

QT_BEGIN_NAMESPACE

namespace Ui {
    class DictPopup;
}

QT_END_NAMESPACE

class DictPopup : public QDialog {
    Q_OBJECT

public:
    explicit DictPopup(QWidget *parent = nullptr);

    void load_data(const QString &selected_chinese_text) const;

    // For a popup opened on exactly one word: enables "Only here", ticked when the word already has
    // a pick, under which edits pick that occurrence's translation instead of changing the dictionary.
    void set_occurrence(const std::optional<QString> &current_pick);

signals:
    // A translation picked for the occurrence, or nullopt to follow the dictionary again.
    void occurrence_picked(const std::optional<QString> &vn);

private:
    [[nodiscard]] bool only_here() const;

    void pick(const std::optional<QString> &vn);

    // Fits the popup to whether "Only here" is ticked.
    void update_only_here() const;

    std::optional<QString> picked;

    void close_popup();

    bool changed = false;

    void add_new_phrase() const;

    void add_new_name();

    void edit_finished_name();

    void edit_finished_phrase();

    ~DictPopup() override;

    Ui::DictPopup *ui;
    ChipDelegate *phrase_chip_delegate;

    static QString capitalize(const QString &text, int mode);

    QString original_name;
};
