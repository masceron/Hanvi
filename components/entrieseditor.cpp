#include "entrieseditor.h"
#include "ui_entrieseditor.h"
#include "dictpopup.h"
#include "core/db.h"
#include "core/dict.h"
#include "core/structures.h"

#include <QSqlQuery>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QFont>
#include <QRegularExpression>
#include <QKeyEvent>
#include <QTimer>
#include <QScrollBar>

// -----------------------------------------------------------------------------
// EntriesModel Implementation
// -----------------------------------------------------------------------------

EntriesModel::EntriesModel(QObject *parent) : QAbstractTableModel(parent) {}

int EntriesModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(items_.size());
}

int EntriesModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return 2;
}

QVariant EntriesModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(items_.size())) {
        return {};
    }

    const auto& item = items_[index.row()];

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

QVariant EntriesModel::headerData(int section, Qt::Orientation orientation, int role) const {
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
    if (parent.isValid() || !has_more_ || is_fetching_) return;
    is_fetching_ = true;
    emit fetch_more_requested();
}

void EntriesModel::set_items(std::vector<EntryItem> items, const bool is_phrases_mode, const bool has_more) {
    beginResetModel();
    items_ = std::move(items);
    is_phrases_mode_ = is_phrases_mode;
    has_more_ = has_more;
    is_fetching_ = false;
    endResetModel();
}

void EntriesModel::append_items(std::vector<EntryItem> items, const bool has_more) {
    if (items.empty()) {
        has_more_ = false;
        is_fetching_ = false;
        return;
    }
    const int start_row = static_cast<int>(items_.size());
    const int count = static_cast<int>(items.size());
    beginInsertRows(QModelIndex(), start_row, start_row + count - 1);
    items_.insert(items_.end(), std::make_move_iterator(items.begin()), std::make_move_iterator(items.end()));
    has_more_ = has_more;
    is_fetching_ = false;
    endInsertRows();
}

void EntriesModel::update_translation(const int row, const QString& new_trans) {
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

const EntryItem* EntriesModel::item_at(const int row) const {
    if (row < 0 || row >= static_cast<int>(items_.size())) return nullptr;
    return &items_[row];
}

// -----------------------------------------------------------------------------
// entrieseditor Implementation
// -----------------------------------------------------------------------------

entrieseditor::entrieseditor(QWidget *parent) : QDialog(parent), ui(new Ui::entrieseditor) {
    ui->setupUi(this);

    model_ = new EntriesModel(this);
    ui->entries_table->setModel(model_);
    ui->entries_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->entries_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    connect(model_, &EntriesModel::fetch_more_requested, this, &entrieseditor::fetch_next_batch);

    connect(ui->entries_table->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        auto *sb = ui->entries_table->verticalScrollBar();
        if (sb && sb->maximum() > 0 && value >= sb->maximum() - 15) {
            if (model_->canFetchMore()) {
                model_->fetchMore();
            }
        }
    });

    connect(ui->entries_table->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int /*min*/, int max) {
        if (max == 0 && model_->canFetchMore()) {
            model_->fetchMore();
        }
    });

    search_timer_ = new QTimer(this);
    search_timer_->setSingleShot(true);
    search_timer_->setInterval(150);
    connect(search_timer_, &QTimer::timeout, this, &entrieseditor::reload_data);

    connect(ui->search_box, &QLineEdit::textChanged, this, &entrieseditor::on_search_changed);
    connect(ui->clear_search, &QPushButton::clicked, this, &entrieseditor::on_clear_search);
    connect(ui->add_button, &QPushButton::clicked, this, &entrieseditor::on_add_entry);
    connect(ui->add_translation, &QLineEdit::returnPressed, this, &entrieseditor::on_add_entry);
    connect(ui->add_original, &QLineEdit::returnPressed, this, [this] {
        ui->add_translation->setFocus();
    });

    connect(ui->edit_button, &QPushButton::clicked, this, &entrieseditor::on_edit_entry);
    connect(ui->delete_button, &QPushButton::clicked, this, &entrieseditor::on_delete_entry);
    connect(ui->close_button, &QPushButton::clicked, this, &QDialog::accept);

    connect(ui->entries_table, &QTableView::doubleClicked, this, &entrieseditor::on_table_double_clicked);
    connect(ui->entries_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &entrieseditor::update_button_states);

    update_button_states();
}

entrieseditor::~entrieseditor() {
    delete ui;
}

void entrieseditor::set_nameset(const int set_id, const QString& title) {
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

void entrieseditor::update_button_states() {
    const bool has_sel = ui->entries_table->selectionModel() &&
                         !ui->entries_table->selectionModel()->selectedRows().isEmpty();
    ui->edit_button->setEnabled(has_sel);
    ui->delete_button->setEnabled(has_sel);
}

void entrieseditor::update_match_count_label() {
    const QString filter = ui->search_box->text().trimmed();
    const int shown_count = model_->rowCount();

    if (filter.isEmpty()) {
        const int total = (cached_total_count_ >= 0) ? cached_total_count_ : shown_count;
        if (total > shown_count) {
            ui->match_count->setText(QStringLiteral("Showing %1 of %2 entries").arg(shown_count).arg(total));
        } else {
            ui->match_count->setText(QStringLiteral("%1 entries").arg(total));
        }
    } else {
        if (model_->has_more()) {
            ui->match_count->setText(QStringLiteral("Showing %1 matches (scroll for more)").arg(shown_count));
        } else {
            ui->match_count->setText(QStringLiteral("%1 matches").arg(shown_count));
        }
    }
}

void entrieseditor::execute_query(const QString& filter) {
    std::vector<EntryItem> items;
    constexpr int batch_size = 200;

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

    if (mode_ == EditorMode::Nameset) {
        QSqlQuery q;
        if (filter.isEmpty()) {
            q.prepare(QStringLiteral("SELECT original, translated FROM name_set_entries WHERE set_id = :id ORDER BY original LIMIT :limit"));
            q.bindValue(QStringLiteral(":id"), set_id_);
            q.bindValue(QStringLiteral(":limit"), batch_size);
        } else {
            q.prepare(QStringLiteral("SELECT original, translated FROM name_set_entries WHERE set_id = :id AND "
                                    "(original LIKE :filter OR translated LIKE :filter) ORDER BY original LIMIT :limit"));
            q.bindValue(QStringLiteral(":id"), set_id_);
            q.bindValue(QStringLiteral(":filter"), QStringLiteral("%%1%").arg(filter));
            q.bindValue(QStringLiteral(":limit"), batch_size);
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
            q.prepare(QStringLiteral("SELECT original, translated FROM phrases ORDER BY original LIMIT :limit"));
            q.bindValue(QStringLiteral(":limit"), batch_size);
        } else {
            q.prepare(QStringLiteral("SELECT original, translated FROM phrases WHERE original LIKE :filter OR translated LIKE :filter ORDER BY original LIMIT :limit"));
            q.bindValue(QStringLiteral(":filter"), QStringLiteral("%%1%").arg(filter));
            q.bindValue(QStringLiteral(":limit"), batch_size);
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
            q.prepare(QStringLiteral("SELECT original, translated FROM names ORDER BY original LIMIT :limit"));
            q.bindValue(QStringLiteral(":limit"), batch_size);
        } else {
            q.prepare(QStringLiteral("SELECT original, translated FROM names WHERE original LIKE :filter OR translated LIKE :filter ORDER BY original LIMIT :limit"));
            q.bindValue(QStringLiteral(":filter"), QStringLiteral("%%1%").arg(filter));
            q.bindValue(QStringLiteral(":limit"), batch_size);
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

    const bool has_more = static_cast<int>(items.size()) == batch_size;
    model_->set_items(std::move(items), mode_ == EditorMode::Phrases, has_more);

    update_match_count_label();
    update_button_states();
}

void entrieseditor::fetch_next_batch() {
    const QString filter = ui->search_box->text().trimmed();
    const int offset = model_->rowCount();
    constexpr int batch_size = 200;

    std::vector<EntryItem> new_items;

    if (mode_ == EditorMode::Nameset) {
        QSqlQuery q;
        if (filter.isEmpty()) {
            q.prepare(QStringLiteral("SELECT original, translated FROM name_set_entries WHERE set_id = :id ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":id"), set_id_);
            q.bindValue(QStringLiteral(":limit"), batch_size);
            q.bindValue(QStringLiteral(":offset"), offset);
        } else {
            q.prepare(QStringLiteral("SELECT original, translated FROM name_set_entries WHERE set_id = :id AND "
                                    "(original LIKE :filter OR translated LIKE :filter) ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":id"), set_id_);
            q.bindValue(QStringLiteral(":filter"), QStringLiteral("%%1%").arg(filter));
            q.bindValue(QStringLiteral(":limit"), batch_size);
            q.bindValue(QStringLiteral(":offset"), offset);
        }

        if (q.exec()) {
            while (q.next()) {
                new_items.push_back(EntryItem{
                    .original = q.value(0).toString(),
                    .translated = q.value(1).toString()
                });
            }
        }
    } else if (mode_ == EditorMode::Phrases) {
        QSqlQuery q;
        if (filter.isEmpty()) {
            q.prepare(QStringLiteral("SELECT original, translated FROM phrases ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":limit"), batch_size);
            q.bindValue(QStringLiteral(":offset"), offset);
        } else {
            q.prepare(QStringLiteral("SELECT original, translated FROM phrases WHERE original LIKE :filter OR translated LIKE :filter ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":filter"), QStringLiteral("%%1%").arg(filter));
            q.bindValue(QStringLiteral(":limit"), batch_size);
            q.bindValue(QStringLiteral(":offset"), offset);
        }

        if (q.exec()) {
            while (q.next()) {
                new_items.push_back(EntryItem{
                    .original = q.value(0).toString(),
                    .translated = q.value(1).toString()
                });
            }
        }
    } else if (mode_ == EditorMode::GlobalNames) {
        QSqlQuery q;
        if (filter.isEmpty()) {
            q.prepare(QStringLiteral("SELECT original, translated FROM names ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":limit"), batch_size);
            q.bindValue(QStringLiteral(":offset"), offset);
        } else {
            q.prepare(QStringLiteral("SELECT original, translated FROM names WHERE original LIKE :filter OR translated LIKE :filter ORDER BY original LIMIT :limit OFFSET :offset"));
            q.bindValue(QStringLiteral(":filter"), QStringLiteral("%%1%").arg(filter));
            q.bindValue(QStringLiteral(":limit"), batch_size);
            q.bindValue(QStringLiteral(":offset"), offset);
        }

        if (q.exec()) {
            while (q.next()) {
                new_items.push_back(EntryItem{
                    .original = q.value(0).toString(),
                    .translated = q.value(1).toString()
                });
            }
        }
    }

    const bool has_more = static_cast<int>(new_items.size()) == batch_size;
    model_->append_items(std::move(new_items), has_more);

    update_match_count_label();
}

void entrieseditor::on_search_changed(const QString& /*text*/) {
    search_timer_->start();
}

void entrieseditor::on_clear_search() {
    search_timer_->stop();
    ui->search_box->clear();
    reload_data();
    ui->search_box->setFocus();
}

void entrieseditor::on_add_entry() {
    const QString orig = ui->add_original->text().trimmed();
    const QString trans = ui->add_translation->text().trimmed();

    if (orig.isEmpty() || trans.isEmpty()) return;

    if (mode_ == EditorMode::Phrases) {
        QStringList parts;
        const auto tokens = trans.split(QRegularExpression(QStringLiteral("[/,]")), Qt::SkipEmptyParts);
        for (const auto& token : tokens) {
            const QString trimmed = token.trimmed();
            if (!trimmed.isEmpty() && !parts.contains(trimmed)) {
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
        q.prepare(QStringLiteral("INSERT INTO name_set_entries (set_id, original, translated) VALUES (:set_id, :original, :translated) "
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

void entrieseditor::on_delete_entry() {
    const auto selection = ui->entries_table->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    const int row = selection.first().row();
    const auto* item = model_->item_at(row);
    if (!item) return;

    const auto reply = QMessageBox::question(this, QStringLiteral("Confirm Deletion"),
        QStringLiteral("Are you sure you want to delete '%1'?").arg(item->original),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

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
    }
}

void entrieseditor::on_table_double_clicked(const QModelIndex& index) {
    if (!index.isValid()) return;
    on_edit_entry();
}

void entrieseditor::on_edit_entry() {
    const auto selection = ui->entries_table->selectionModel()->selectedRows();
    if (selection.isEmpty()) return;

    const int row = selection.first().row();
    const auto* item = model_->item_at(row);
    if (!item) return;

    const QString original_key = item->original;
    const QString old_trans = item->translated;

    if (mode_ == EditorMode::Phrases) {
        auto* popup = new DictPopup(this);
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
                q.prepare(QStringLiteral("UPDATE name_set_entries SET translated = :trans WHERE set_id = :id AND original = :orig"));
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
        if (ui->entries_table->hasFocus() || (!ui->search_box->hasFocus() && !ui->add_original->hasFocus() && !ui->add_translation->hasFocus())) {
            on_delete_entry();
            return;
        }
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (ui->entries_table->hasFocus()) {
            on_edit_entry();
            return;
        }
    } else if (event->key() == Qt::Key_Escape) {
        if (ui->search_box->hasFocus() && !ui->search_box->text().isEmpty()) {
            on_clear_search();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}
