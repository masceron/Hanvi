#pragma once

#include <QDialog>
#include <QAbstractTableModel>
#include <vector>
#include "core/structures.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class entrieseditor;
}

class QTimer;
QT_END_NAMESPACE

enum class EditorMode {
    Nameset,
    Phrases,
    GlobalNames
};

struct EntryItem {
    QString original;
    QString translated;
};

class EntriesModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit EntriesModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent) const override;

    [[nodiscard]] int rowCount() const noexcept { return static_cast<int>(items_.size()); }

    [[nodiscard]] int columnCount(const QModelIndex &parent) const override;

    static int columnCount() noexcept { return 2; }

    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] bool canFetchMore(const QModelIndex &parent) const override;

    [[nodiscard]] bool canFetchMore() const { return canFetchMore(QModelIndex()); }

    void fetchMore(const QModelIndex &parent) override;

    void fetchMore() { fetchMore(QModelIndex()); }

    void set_items(std::vector<EntryItem> items, bool is_phrases_mode, bool has_more);

    void append_items(std::vector<EntryItem> items);

    void prepend_items(std::vector<EntryItem> items);

    void trim_top(int count);

    void trim_bottom(int count);

    void update_translation(int row, const QString &new_trans);

    void remove_row(int row);

    [[nodiscard]] const EntryItem *item_at(int row) const;

    [[nodiscard]] bool has_more() const noexcept { return has_more_; }
    void set_has_more(const bool has_more) noexcept { has_more_ = has_more; }

signals:
    void fetch_more_requested();

private:
    std::vector<EntryItem> items_;
    bool is_phrases_mode_ = false;
    bool has_more_ = false;
};

class entrieseditor : public QDialog {
    Q_OBJECT

public:
    explicit entrieseditor(QWidget *parent = nullptr);

    ~entrieseditor() override;

    void set_nameset(int set_id, const QString &title);

    void set_phrases();

    void set_global_names();

    [[nodiscard]] bool has_changes() const noexcept { return changes_made_; }

signals:
    void entries_changed();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void search_text_changed(const QString &text) const;

    void clear_search();

    void add_entry();

    void edit_entry();

    void delete_entry();

    void table_double_clicked(const QModelIndex &index);

    void handle_scroll(int value);

    void fetch_down();

    void fetch_up();

    [[nodiscard]] Priority priority() const noexcept;

    [[nodiscard]] std::vector<EntryItem> query_items(const QString &filter, int offset, int limit) const;

private:
    Ui::entrieseditor *ui;
    EditorMode mode_ = EditorMode::Nameset;
    int set_id_ = -1;
    QString set_title_;
    bool changes_made_ = false;

    EntriesModel *model_ = nullptr;
    QTimer *search_timer_ = nullptr;
    int cached_total_count_ = -1;
    int window_offset_ = 0;
    bool has_more_below_ = false;
    bool is_updating_window_ = false;

    void reload_data();

    void execute_query(const QString &filter);

    void update_button_states() const;

    void update_match_count_label() const;
};
