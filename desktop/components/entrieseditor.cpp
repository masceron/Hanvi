#include "entrieseditor.h"
#include "ui_entrieseditor.h"
#include "dictpopup.h"
#include "core/db.h"
#include "core/dict.h"
#include "core/structures.h"

#include <QSqlQuery>
#include <QInputDialog>
#include <QHeaderView>
#include <QFont>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QTimer>
#include <QScrollBar>

EntriesModel::EntriesModel(QObject *parent) : QAbstractTableModel(parent) {
}

int EntriesModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(items_.size());
}

int EntriesModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return 2;
}

QVariant EntriesModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(items_.size())) {
        return {};
    }

    const auto &item = items_[index.row()];

    if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            return item.original;
        }
        if (index.column() == 1) {
            if (is_phrases_mode_) {
                QString display = item.translated;
                display.replace(u'\x1F', QStringLiteral(" / "));
                return display;
            }
            return item.translated;
        }
    } else if (role == Qt::FontRole) {
        if (index.column() == 0) {
            QFont f(QStringLiteral("Noto Sans SC"));
            f.setPointSize(10);
            return f;
        }
        if (index.column() == 1) {
            QFont f(QStringLiteral("Tahoma"));
            f.setPointSize(10);
            return f;
        }
    } else if (role == Qt::ToolTipRole) {
        if (index.column() == 1 && is_phrases_mode_) {
            QString tooltip = item.translated;
            tooltip.replace(u'\x1F', QStringLiteral("\n• "));
            return QStringLiteral("• ") + tooltip;
        }
        return item.translated;
    }

    return {};
}

QVariant EntriesModel::headerData(const int section, const Qt::Orientation orientation, const int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0) return QStringLiteral("Original (Chinese)");
        if (section == 1) return QStringLiteral("Translation (Vietnamese)");
    }
    return {};
}

bool EntriesModel::canFetchMore(const QModelIndex &parent) const {
    if (parent.isValid()) return false;
    return has_more_;
}

void EntriesModel::fetchMore(const QModelIndex &parent) {
    if (parent.isValid() || !has_more_) return;
    emit fetch_more_requested();
}

void EntriesModel::set_items(std::vector<EntryItem> items, const bool is_phrases_mode, const bool has_more) {
    beginResetModel();
    items_ = std::move(items);
    is_phrases_mode_ = is_phrases_mode;
    has_more_ = has_more;
    endResetModel();
}

void EntriesModel::append_items(std::vector<EntryItem> items) {
    if (items.empty()) return;
    const int start_row = static_cast<int>(items_.size());
    const int count = static_cast<int>(items.size());
    beginInsertRows(QModelIndex(), start_row, start_row + count - 1);
    items_.insert(items_.end(), std::make_move_iterator(items.begin()), std::make_move_iterator(items.end()));
    endInsertRows();
}

void EntriesModel::prepend_items(std::vector<EntryItem> items) {
    if (items.empty()) return;
    const int count = static_cast<int>(items.size());
    beginInsertRows(QModelIndex(), 0, count - 1);
    items_.insert(items_.begin(), std::make_move_iterator(items.begin()), std::make_move_iterator(items.end()));
    endInsertRows();
}

void EntriesModel::trim_top(const int count) {
    if (count <= 0 || items_.empty()) return;
    const int actual_count = std::min(count, static_cast<int>(items_.size()));
    beginRemoveRows(QModelIndex(), 0, actual_count - 1);
    items_.erase(items_.begin(), items_.begin() + actual_count);
    endRemoveRows();
}

void EntriesModel::trim_bottom(const int count) {
    if (count <= 0 || items_.empty()) return;
    const int actual_count = std::min(count, static_cast<int>(items_.size()));
    const int start = static_cast<int>(items_.size()) - actual_count;
    beginRemoveRows(QModelIndex(), start, static_cast<int>(items_.size()) - 1);
    items_.erase(items_.begin() + start, items_.end());
    endRemoveRows();
}

void EntriesModel::update_translation(const int row, const QString &new_trans) {
    if (row < 0 || row >= static_cast<int>(items_.size())) return;
    items_[row].translated = new_trans;
    const auto idx = index(row, 1);
    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::ToolTipRole});
}

void EntriesModel::remove_row(const int row) {
    if (row < 0 || row >= static_cast<int>(items_.size())) return;
    beginRemoveRows(QModelIndex(), row, row);
    items_.erase(items_.begin() + row);
    endRemoveRows();
}

const EntryItem *EntriesModel::item_at(const int row) const {
    if (row < 0 || row >= static_cast<int>(items_.size())) return nullptr;
    return &items_[row];
}

entrieseditor::entrieseditor(QWidget *parent) : QDialog(parent), ui(new Ui::entrieseditor) {
    ui->setupUi(this);

    model_ = new EntriesModel(this);
    ui->entries_table->setModel(model_);
    ui->entries_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->entries_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->entries_table->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);

    connect(model_, &EntriesModel::fetch_more_requested, this, &entrieseditor::fetch_down);
    connect(ui->entries_table->verticalScrollBar(), &QScrollBar::valueChanged, this, &entrieseditor::handle_scroll);
    connect(ui->entries_table->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int /*min*/, const int max) {
        if (!is_updating_window_ && max == 0 && has_more_below_) {
            fetch_down();
        }
    });

    search_timer_ = new QTimer(this);
    search_timer_->setSingleShot(true);
    search_timer_->setInterval(150);
    connect(search_timer_, &QTimer::timeout, this, &entrieseditor::reload_data);

    connect(ui->search_box, &QLineEdit::textChanged, this, &entrieseditor::search_text_changed);
    connect(ui->clear_search, &QPushButton::clicked, this, &entrieseditor::clear_search);
    connect(ui->add_button, &QPushButton::clicked, this, &entrieseditor::add_entry);
    connect(ui->add_translation, &QLineEdit::returnPressed, this, &entrieseditor::add_entry);
    connect(ui->add_original, &QLineEdit::returnPressed, this, [this] {
        ui->add_translation->setFocus();
    });

    connect(ui->edit_button, &QPushButton::clicked, this, &entrieseditor::edit_entry);
    connect(ui->delete_button, &QPushButton::clicked, this, &entrieseditor::delete_entry);
    connect(ui->close_button, &QPushButton::clicked, this, &QDialog::accept);

    connect(ui->entries_table, &QTableView::doubleClicked, this, &entrieseditor::table_double_clicked);
    connect(ui->entries_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &entrieseditor::update_button_states);

    update_button_states();
}

entrieseditor::~entrieseditor() {
    delete ui;
}

void entrieseditor::set_nameset(const int set_id, const QString &title) {
    mode_ = EditorMode::Nameset;
    set_id_ = set_id;
    set_title_ = title;
    cached_total_count_ = -1;
    setWindowTitle(QStringLiteral("Nameset: %1").arg(title));
    ui->add_translation->setPlaceholderText(QStringLiteral("Translation (Vietnamese)..."));
    reload_data();
}

void entrieseditor::set_phrases() {
    mode_ = EditorMode::Phrases;
    set_id_ = -1;
    set_title_.clear();
    cached_total_count_ = -1;
    setWindowTitle(QStringLiteral("Dictionary: Phrases"));
    ui->add_translation->setPlaceholderText(QStringLiteral("Translation (use / for multiple meanings)..."));
    reload_data();
}

void entrieseditor::set_global_names() {
    mode_ = EditorMode::GlobalNames;
    set_id_ = -1;
    set_title_.clear();
    cached_total_count_ = -1;
    setWindowTitle(QStringLiteral("Dictionary: Global Names"));
    ui->add_translation->setPlaceholderText(QStringLiteral("Translation (Vietnamese)..."));
    reload_data();
}

void entrieseditor::reload_data() {
    execute_query(ui->search_box->text().trimmed());
}

void entrieseditor::update_button_states() const {
    const bool has_sel = ui->entries_table->selectionModel() &&
                         !ui->entries_table->selectionModel()->selectedRows().isEmpty();
    ui->edit_button->setEnabled(has_sel);
    ui->delete_button->setEnabled(has_sel);
}

void entrieseditor::update_match_count_label() const {
    const QString filter = ui->search_box->text().trimmed();
    const int count = model_->rowCount();

    if (count == 0) {
        ui->match_count->setText(QStringLiteral("0 entries"));
        return;
    }

    const int start_idx = window_offset_ + 1;
    const int end_idx = window_offset_ + count;

    if (filter.isEmpty()) {
        const int total = (cached_total_count_ >= 0) ? cached_total_count_ : end_idx;
        if (total > count) {
            ui->match_count->setText(QStringLiteral("Entries %1 – %2 of %3")
                .arg(start_idx).arg(end_idx).arg(total));
        } else {
            ui->match_count->setText(QStringLiteral("%1 entries").arg(total));
        }
    } else {
        if (has_more_below_) {
            ui->match_count->setText(QStringLiteral("Matches %1 – %2 (scroll for more)")
                .arg(start_idx).arg(end_idx));
        } else if (window_offset_ > 0) {
            ui->match_count->setText(QStringLiteral("Matches %1 – %2")
                .arg(start_idx).arg(end_idx));
        } else {
            ui->match_count->setText(QStringLiteral("%1 matches").arg(count));
        }
    }
}

std::vector<EntryItem> entrieseditor::query_items(const QString &filter, const int offset, const int limit) const {
    std::vector<EntryItem> items;

    if (mode_ == EditorMode::Nameset) {
        QSqlQuery q;
        if (filter.isEmpty()) {
            q.prepare(QStringLiteral(
                "SELECT original, translated FROM name_set_entries WHERE set_id = :id ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":id"), set_id_);
            q.bindValue(QStringLiteral(":limit"), limit);
            q.bindValue(QStringLiteral(":offset"), offset);
        } else {
            q.prepare(QStringLiteral("SELECT original, translated FROM name_set_entries WHERE set_id = :id AND "
                "(original LIKE :filter OR translated LIKE :filter) ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":id"), set_id_);
            q.bindValue(QStringLiteral(":filter"), QStringLiteral("%%1%").arg(filter));
            q.bindValue(QStringLiteral(":limit"), limit);
            q.bindValue(QStringLiteral(":offset"), offset);
        }

        if (q.exec()) {
            while (q.next()) {
                items.push_back(EntryItem{
                    .original = q.value(0).toString(),
                    .translated = q.value(1).toString()
                });
            }
        }
    } else if (mode_ == EditorMode::Phrases) {
        QSqlQuery q;
        if (filter.isEmpty()) {
            q.prepare(QStringLiteral(
                "SELECT original, translated FROM phrases ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":limit"), limit);
            q.bindValue(QStringLiteral(":offset"), offset);
        } else {
            q.prepare(QStringLiteral(
                "SELECT original, translated FROM phrases WHERE original LIKE :filter OR translated LIKE :filter ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":filter"), QStringLiteral("%%1%").arg(filter));
            q.bindValue(QStringLiteral(":limit"), limit);
            q.bindValue(QStringLiteral(":offset"), offset);
        }

        if (q.exec()) {
            while (q.next()) {
                items.push_back(EntryItem{
                    .original = q.value(0).toString(),
                    .translated = q.value(1).toString()
                });
            }
        }
    } else if (mode_ == EditorMode::GlobalNames) {
        QSqlQuery q;
        if (filter.isEmpty()) {
            q.prepare(QStringLiteral(
                "SELECT original, translated FROM names ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":limit"), limit);
            q.bindValue(QStringLiteral(":offset"), offset);
        } else {
            q.prepare(QStringLiteral(
                "SELECT original, translated FROM names WHERE original LIKE :filter OR translated LIKE :filter ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":filter"), QStringLiteral("%%1%").arg(filter));
            q.bindValue(QStringLiteral(":limit"), limit);
            q.bindValue(QStringLiteral(":offset"), offset);
        }

        if (q.exec()) {
            while (q.next()) {
                items.push_back(EntryItem{
                    .original = q.value(0).toString(),
                    .translated = q.value(1).toString()
                });
            }
        }
    }

    return items;
}

void entrieseditor::execute_query(const QString &filter) {
    is_updating_window_ = true;
    window_offset_ = 0;
    constexpr int initial_count = 200;

    if (filter.isEmpty() && cached_total_count_ < 0) {
        if (mode_ == EditorMode::Nameset) {
            QSqlQuery count_q;
            count_q.prepare(QStringLiteral("SELECT COUNT(*) FROM name_set_entries WHERE set_id = :id"));
            count_q.bindValue(QStringLiteral(":id"), set_id_);
            if (count_q.exec() && count_q.next()) {
                cached_total_count_ = count_q.value(0).toInt();
            }
        } else if (mode_ == EditorMode::Phrases) {
            QSqlQuery count_q;
            if (count_q.exec(QStringLiteral("SELECT COUNT(*) FROM phrases")) && count_q.next()) {
                cached_total_count_ = count_q.value(0).toInt();
            }
        } else if (mode_ == EditorMode::GlobalNames) {
            QSqlQuery count_q;
            if (count_q.exec(QStringLiteral("SELECT COUNT(*) FROM names")) && count_q.next()) {
                cached_total_count_ = count_q.value(0).toInt();
            }
        }
    }

    std::vector<EntryItem> items = query_items(filter, 0, initial_count);
    has_more_below_ = static_cast<int>(items.size()) == initial_count;

    model_->set_items(std::move(items), mode_ == EditorMode::Phrases, has_more_below_);

    if (auto *sb = ui->entries_table->verticalScrollBar()) {
        sb->setValue(0);
    }

    is_updating_window_ = false;

    update_match_count_label();
    update_button_states();
}

void entrieseditor::handle_scroll(const int value) {
    if (is_updating_window_) return;

    auto *sb = ui->entries_table->verticalScrollBar();
    if (!sb) return;

    if (has_more_below_ && sb->maximum() > 0 && value >= sb->maximum() - 20) {
        fetch_down();
        return;
    }

    if (window_offset_ > 0 && value <= 20) {
        fetch_up();
    }
}

void entrieseditor::fetch_down() {
    if (is_updating_window_ || !has_more_below_) return;
    is_updating_window_ = true;

    const QString filter = ui->search_box->text().trimmed();
    const int fetch_offset = window_offset_ + model_->rowCount();
    constexpr int chunk_size = 100;
    constexpr int max_window = 400;

    std::vector<EntryItem> new_items = query_items(filter, fetch_offset, chunk_size);

    if (new_items.empty()) {
        has_more_below_ = false;
        model_->set_has_more(false);
        is_updating_window_ = false;
        update_match_count_label();
        return;
    }

    if (static_cast<int>(new_items.size()) < chunk_size) {
        has_more_below_ = false;
        model_->set_has_more(false);
    }

    model_->append_items(std::move(new_items));

    if (model_->rowCount() > max_window) {
        const int trim_count = model_->rowCount() - max_window;
        auto *sb = ui->entries_table->verticalScrollBar();
        const int cur_scroll = sb ? sb->value() : 0;

        const auto sel = ui->entries_table->selectionModel()->selectedRows();
        const int sel_row = sel.isEmpty() ? -1 : sel.first().row();

        model_->trim_top(trim_count);
        window_offset_ += trim_count;

        if (sb) {
            sb->setValue(std::max(0, cur_scroll - trim_count));
        }
        if (sel_row >= trim_count) {
            ui->entries_table->selectRow(sel_row - trim_count);
        } else {
            ui->entries_table->clearSelection();
        }
    }

    is_updating_window_ = false;
    update_match_count_label();
}

void entrieseditor::fetch_up() {
    if (is_updating_window_ || window_offset_ <= 0) return;
    is_updating_window_ = true;

    const QString filter = ui->search_box->text().trimmed();
    constexpr int chunk_size = 100;
    constexpr int max_window = 400;

    const int fetch_count = std::min(chunk_size, window_offset_);
    const int fetch_offset = window_offset_ - fetch_count;

    std::vector<EntryItem> new_items = query_items(filter, fetch_offset, fetch_count);

    if (new_items.empty()) {
        window_offset_ = 0;
        is_updating_window_ = false;
        update_match_count_label();
        return;
    }

    const int prepended_count = static_cast<int>(new_items.size());
    auto *sb = ui->entries_table->verticalScrollBar();
    const int cur_scroll = sb ? sb->value() : 0;

    const auto sel = ui->entries_table->selectionModel()->selectedRows();
    const int sel_row = sel.isEmpty() ? -1 : sel.first().row();

    model_->prepend_items(std::move(new_items));
    window_offset_ -= prepended_count;
    has_more_below_ = true;
    model_->set_has_more(true);

    if (sb) {
        sb->setValue(cur_scroll + prepended_count);
    }
    if (sel_row >= 0) {
        ui->entries_table->selectRow(sel_row + prepended_count);
    }

    if (model_->rowCount() > max_window) {
        const int trim_count = model_->rowCount() - max_window;
        model_->trim_bottom(trim_count);
    }

    is_updating_window_ = false;
    update_match_count_label();
}

void entrieseditor::search_text_changed(const QString & /*text*/) const {
    search_timer_->start();
}

void entrieseditor::clear_search() {
    search_timer_->stop();
    const QSignalBlocker blocker(ui->search_box);
    ui->search_box->clear();
    reload_data();
    ui->search_box->setFocus();
}

void entrieseditor::add_entry() {
    const QString orig = ui->add_original->text().trimmed();
    const QString trans = ui->add_translation->text().trimmed();

    if (orig.isEmpty() || trans.isEmpty()) return;

    if (mode_ == EditorMode::Phrases) {
        static const auto splitter = QRegularExpression(QStringLiteral("[/,]"));
        QStringList parts;
        for (const auto tokens = trans.split(splitter, Qt::SkipEmptyParts);
             const auto &token: tokens) {
            if (const QString trimmed = token.trimmed(); !trimmed.isEmpty() && !parts.contains(trimmed)) {
                parts.append(trimmed);
            }
        }
        if (parts.isEmpty()) return;
        const QString db_val = parts.join(u'\x1F');

        db_insert(orig, db_val, PHRASE);
        dictionary.insert(orig, db_val, PHRASE);
    } else if (mode_ == EditorMode::GlobalNames) {
        db_insert(orig, trans, NAME);
        dictionary.insert(orig, trans, NAME);
    } else if (mode_ == EditorMode::Nameset) {
        QSqlQuery q;
        q.prepare(QStringLiteral(
            "INSERT INTO name_set_entries (set_id, original, translated) VALUES (:set_id, :original, :translated) "
            "ON CONFLICT(set_id, original) DO UPDATE SET translated = excluded.translated"));
        q.bindValue(QStringLiteral(":set_id"), set_id_);
        q.bindValue(QStringLiteral(":original"), orig);
        q.bindValue(QStringLiteral(":translated"), trans);
        q.exec();

        if (set_id_ == current_name_set_id) {
            name_set_dictionary.insert(orig, trans, NAME);
        }
    }

    changes_made_ = true;
    if (cached_total_count_ >= 0) ++cached_total_count_;
    emit entries_changed();

    ui->add_original->clear();
    ui->add_translation->clear();
    reload_data();
    ui->add_original->setFocus();
}

void entrieseditor::delete_entry() {
    const auto selection = ui->entries_table->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    const int row = selection.first().row();
    const auto *item = model_->item_at(row);
    if (!item) return;

    const QString original_key = item->original;

    if (mode_ == EditorMode::Phrases) {
        db_remove(original_key, PHRASE);
        dictionary.remove(original_key, PHRASE);
    } else if (mode_ == EditorMode::GlobalNames) {
        db_remove(original_key, NAME);
        dictionary.remove(original_key, NAME);
    } else if (mode_ == EditorMode::Nameset) {
        QSqlQuery q;
        q.prepare(QStringLiteral("DELETE FROM name_set_entries WHERE set_id = :id AND original = :original"));
        q.bindValue(QStringLiteral(":id"), set_id_);
        q.bindValue(QStringLiteral(":original"), original_key);
        q.exec();

        if (set_id_ == current_name_set_id) {
            name_set_dictionary.remove(original_key, NAME);
        }
    }

    changes_made_ = true;
    if (cached_total_count_ > 0) --cached_total_count_;
    emit entries_changed();

    model_->remove_row(row);
    update_match_count_label();
    update_button_states();

    const int new_count = model_->rowCount();
    if (new_count > 0) {
        const int target_row = std::min(row, new_count - 1);
        ui->entries_table->selectRow(target_row);
    } else if (cached_total_count_ > 0 || window_offset_ > 0) {
        reload_data();
    }
}

void entrieseditor::table_double_clicked(const QModelIndex &index) {
    if (!index.isValid()) return;
    edit_entry();
}

void entrieseditor::edit_entry() {
    const auto selection = ui->entries_table->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    const int row = selection.first().row();
    const auto *item = model_->item_at(row);
    if (!item) return;

    const QString original_key = item->original;
    const QString old_trans = item->translated;

    if (mode_ == EditorMode::Phrases) {
        auto *popup = new DictPopup(this);
        popup->load_data(original_key);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        if (popup->exec()) {
            changes_made_ = true;
            emit entries_changed();

            QSqlQuery q;
            q.prepare(QStringLiteral("SELECT translated FROM phrases WHERE original = :orig"));
            q.bindValue(QStringLiteral(":orig"), original_key);
            if (q.exec() && q.next()) {
                model_->update_translation(row, q.value(0).toString());
            } else {
                model_->remove_row(row);
                if (cached_total_count_ > 0) --cached_total_count_;
                update_match_count_label();
            }
            update_button_states();
            if (row < model_->rowCount()) {
                ui->entries_table->selectRow(row);
            }
        }
    } else {
        bool ok = false;
        const QString new_trans = QInputDialog::getText(this, QStringLiteral("Edit Translation"),
                                                        QStringLiteral("Translation for '%1':").arg(original_key),
                                                        QLineEdit::Normal, old_trans, &ok);

        if (ok && !new_trans.isEmpty() && new_trans != old_trans) {
            if (mode_ == EditorMode::Nameset) {
                QSqlQuery q;
                q.prepare(QStringLiteral(
                    "UPDATE name_set_entries SET translated = :trans WHERE set_id = :id AND original = :orig"));
                q.bindValue(QStringLiteral(":trans"), new_trans);
                q.bindValue(QStringLiteral(":id"), set_id_);
                q.bindValue(QStringLiteral(":orig"), original_key);
                q.exec();

                if (set_id_ == current_name_set_id) {
                    name_set_dictionary.insert(original_key, new_trans, NAME);
                }
            } else if (mode_ == EditorMode::GlobalNames) {
                db_insert(original_key, new_trans, NAME);
                dictionary.insert(original_key, new_trans, NAME);
            }

            changes_made_ = true;
            emit entries_changed();
            model_->update_translation(row, new_trans);
            update_button_states();
            if (row < model_->rowCount()) {
                ui->entries_table->selectRow(row);
            }
        }
    }
}

void entrieseditor::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Delete) {
        if (ui->entries_table->hasFocus() || (!ui->search_box->hasFocus() && !ui->add_original->hasFocus() && !ui->
                                              add_translation->hasFocus())) {
            delete_entry();
            return;
        }
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (ui->entries_table->hasFocus()) {
            edit_entry();
            return;
        }
    } else if (event->key() == Qt::Key_Escape) {
        if (ui->search_box->hasFocus() && !ui->search_box->text().isEmpty()) {
            clear_search();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}
