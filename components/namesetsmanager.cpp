#include <QSqlQuery>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>

#include "namesetsmanager.h"
#include "ui_NamesetsManager.h"
#include "entrieseditor.h"
#include "core/dict.h"
#include "core/structures.h"

NamesetsManager::NamesetsManager(QWidget* parent) :
    QDialog(parent), ui(new Ui::NamesetsManager)
{
    ui->setupUi(this);

    setWindowTitle("Namesets");

    ui->name_sets_list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->name_sets_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->name_sets_list->setColumnWidth(1, 80);
    ui->name_sets_list->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->name_sets_list->setColumnWidth(2, 320);

    connect(ui->add_new, &QPushButton::clicked, this, &NamesetsManager::add_new_name_set);
    connect(ui->import_set, &QPushButton::clicked, this, &NamesetsManager::import_set);

    connect(ui->name_sets_list, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (column == 2) return;
        if (row < 0 || row >= ui->name_sets_list->rowCount()) return;
        auto* title_item = ui->name_sets_list->item(row, 0);
        if (!title_item) return;
        const int id = title_item->data(Qt::UserRole).toInt();
        const QString title = title_item->text();

        auto* editor = new entrieseditor(this);
        editor->setAttribute(Qt::WA_DeleteOnClose);
        editor->set_nameset(id, title);
        connect(editor, &entrieseditor::entries_changed, this, &NamesetsManager::load_data);
        editor->exec();
    });

    load_data();
}

NamesetsManager::~NamesetsManager()
{
    delete ui;
}

void NamesetsManager::load_data()
{
    ui->name_sets_list->setRowCount(0);
    QMap<int, int> counts;
    QSqlQuery query("SELECT set_id, COUNT(*) FROM name_set_entries GROUP BY set_id");
    while (query.next())
    {
        counts.insert(query.value(0).toInt(), query.value(1).toInt());
    }

    for (const auto& [index, title] : name_sets)
    {
        const int row = ui->name_sets_list->rowCount();
        ui->name_sets_list->insertRow(row);

        auto* titleItem = new QTableWidgetItem(title);
        titleItem->setData(Qt::UserRole, index);
        titleItem->setFlags(titleItem->flags() & ~Qt::ItemIsEditable);
        ui->name_sets_list->setItem(row, 0, titleItem);

        const int count = counts.value(index, 0);
        auto* count_item = new QTableWidgetItem(QString::number(count));

        count_item->setTextAlignment(Qt::AlignCenter);
        count_item->setFlags(count_item->flags() & ~Qt::ItemIsEditable);

        ui->name_sets_list->setItem(row, 1, count_item);
        ui->name_sets_list->setCellWidget(row, 2, create_action_widget(index, title));
    }
}

QWidget* NamesetsManager::create_action_widget(int id, const QString& current_title)
{
    const auto container = new QWidget();
    const auto layout = new QHBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(5);

    const auto entries_btn = new QPushButton("Entries");
    entries_btn->setObjectName("entries_set");
    connect(entries_btn, &QPushButton::clicked, this, [this, id, current_title] {
        auto* editor = new entrieseditor(this);
        editor->setAttribute(Qt::WA_DeleteOnClose);
        editor->set_nameset(id, current_title);
        connect(editor, &entrieseditor::entries_changed, this, &NamesetsManager::load_data);
        editor->exec();
    });

    const auto edit = new QPushButton("Rename");
    edit->setObjectName("edit_set");
    connect(edit, &QPushButton::clicked, this, [this, id, current_title]()
    {
        bool ok;
        const QString text = QInputDialog::getText(this, "Edit Nameset",
                                                   "New title:", QLineEdit::Normal,
                                                   current_title, &ok);
        if (ok && !text.isEmpty())
        {
            QSqlQuery q;
            q.prepare("UPDATE name_sets SET title = :title WHERE id = :id");
            q.bindValue(":title", text);
            q.bindValue(":id", id);
            q.exec();
            load_data();
        }
    });

    const auto delete_button = new QPushButton("Delete");
    delete_button->setObjectName("delete_set");

    connect(delete_button, &QPushButton::clicked, this, [this, id]()
    {
        const auto reply = QMessageBox::question(this, "Confirm deletion",
                                                 "Are you sure? This will delete all names inside this set.",
                                                 QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes)
        {
            QSqlQuery q;

            q.prepare("DELETE FROM name_sets WHERE id = :id");
            q.bindValue(":id", id);
            q.exec();
            std::erase_if(name_sets, [id](const NameSet& name_set)
            {
                return name_set.index == id;
            });
            load_data();
        }
    });

    const auto dump_button = new QPushButton("Save");
    dump_button->setObjectName("dump_set");
    connect(dump_button, &QPushButton::clicked, this, [this, id]
    {
        const auto file_name = QFileDialog::getSaveFileName(this, "Save to...", "/", "Text files (*.txt)");
        if (file_name.isEmpty()) return;

        QFile file(file_name);

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            return;
        }

        QTextStream out(&file);

        QSqlQuery q;
        q.prepare("SELECT original, translated FROM name_set_entries WHERE set_id = :id");
        q.bindValue(":id", id);
        if (!q.exec())
        {
            return;
        }

        while (q.next())
        {
            const QString original = q.value(0).toString();
            const QString translated = q.value(1).toString();

            out << original << "=" << translated << "\n";
        }

        file.close();

        QDesktopServices::openUrl(QUrl("file:///" + file_name.left(file_name.lastIndexOf('/'))));
    });

    layout->addWidget(entries_btn);
    layout->addWidget(edit);
    layout->addWidget(delete_button);
    layout->addWidget(dump_button);

    return container;
}

void NamesetsManager::add_new_name_set()
{
    bool ok;

    if (const QString text = QInputDialog::getText(this, "New Nameset", "Name:", QLineEdit::Normal, "", &ok); ok && !
        text.isEmpty())
    {
        QSqlQuery q;
        q.prepare("INSERT INTO name_sets (title) VALUES (:title)");
        q.bindValue(":title", text);

        if (q.exec())
        {
            const int new_id = q.lastInsertId().toInt();
            name_sets.emplace_back(new_id, text);
            load_data();
        }
        else
        {
            QMessageBox::warning(this, "Error", "Could not create nameset (Duplicate name?).");
        }
    }
}

void NamesetsManager::import_set()
{
    bool ok;
    const QString text = QInputDialog::getText(this, "New Nameset", "Name:", QLineEdit::Normal, "", &ok);

    if (!ok || text.isEmpty()) return;

    const QString file_name = QFileDialog::getOpenFileName(this, "Import Nameset", "/", "Text files (*.txt)");
    if (file_name.isEmpty()) return;

    QFile file(file_name);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Error", "Could not open the selected file.");
        return;
    }

    QSqlDatabase::database().transaction();

    bool success = true;
    QString error_message;

    QSqlQuery q;
    q.prepare("INSERT INTO name_sets (title) VALUES (:title)");
    q.bindValue(":title", text);

    if (!q.exec())
    {
        success = false;
        error_message = "Could not create nameset. The name might already exist.";
    }

    int new_set_id;

    if (success)
    {
        new_set_id = q.lastInsertId().toInt();
        QTextStream in(&file);
        QSqlQuery insert_q;
        insert_q.prepare("INSERT INTO name_set_entries (original, translated, set_id) VALUES (:orig, :trans, :id)");

        int line_number = 0;

        while (!in.atEnd())
        {
            QString line = in.readLine();
            line_number++;

            if (line.trimmed().isEmpty()) continue;

            const int separator_index = static_cast<int>(line.indexOf('='));

            if (separator_index == -1)
            {
                success = false;
                error_message = QString("Format error at line %1: Missing '=' separator.").arg(line_number);
                break;
            }

            const QString original = line.left(separator_index);
            const QString translated = line.mid(separator_index + 1);

            if (original.isEmpty() || translated.isEmpty())
            {
                success = false;
                error_message = QString("Format error at line %1: Original or translated text is empty.").arg(
                    line_number);
                break;
            }

            insert_q.bindValue(":orig", original);
            insert_q.bindValue(":trans", translated);
            insert_q.bindValue(":id", new_set_id);

            if (!insert_q.exec())
            {
                success = false;
                error_message = QString("Database error at line %1: Could not insert entry.").arg(line_number);
                break;
            }
        }
    }

    if (success)
    {
        if (QSqlDatabase::database().commit())
        {
            name_sets.emplace_back(new_set_id, text);
            load_data();
            QMessageBox::information(this, "Success", "Nameset imported successfully.");
        }
        else
        {
            QSqlDatabase::database().rollback();
            QMessageBox::warning(this, "Error", "Transaction commit failed.");
        }
    }
    else
    {
        QSqlDatabase::database().rollback();
        QMessageBox::critical(this, "Import Failed", error_message + "\n\nNo changes were made to the database.");
    }

    file.close();
}
