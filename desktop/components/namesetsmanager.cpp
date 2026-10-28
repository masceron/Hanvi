#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>

#include "namesetsmanager.h"
#include "ui_namesetsmanager.h"
#include "entrieseditor.h"
#include "qt_utils.h"
#include "core/db.h"
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

    connect(ui->name_sets_list, &QTableWidget::cellDoubleClicked, this, [this](const int row, const int column) {
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
    const auto counts = nameset_db_entry_counts();

    for (const auto& [index, title] : name_sets)
    {
        const int row = ui->name_sets_list->rowCount();
        ui->name_sets_list->insertRow(row);

        QString qtitle = QString::fromUtf16(title.data(), static_cast<qsizetype>(title.size()));
        auto* titleItem = new QTableWidgetItem(qtitle);
        titleItem->setData(Qt::UserRole, index);
        titleItem->setFlags(titleItem->flags() & ~Qt::ItemIsEditable);
        ui->name_sets_list->setItem(row, 0, titleItem);

        const auto count_it = counts.find(index);
        const int count = count_it != counts.end() ? count_it->second : 0;
        auto* count_item = new QTableWidgetItem(QString::number(count));

        count_item->setTextAlignment(Qt::AlignCenter);
        count_item->setFlags(count_item->flags() & ~Qt::ItemIsEditable);

        ui->name_sets_list->setItem(row, 1, count_item);
        ui->name_sets_list->setCellWidget(row, 2, create_action_widget(index, qtitle));
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
            if (!io_rename_name_set(id, q_to_u16(text)))
            {
                QMessageBox::warning(this, "Error", "Could not rename nameset (Duplicate name?).");
            }
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
            io_delete_name_set(id);
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

        for (const auto& [original, translated] : db_list_entries(id, NAME, {}, 0, -1))
        {
            out << to_qstring(original) << "=" << to_qstring(translated) << "\n";
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
        if (io_create_name_set(q_to_u16(text)))
        {
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

    std::vector<Entry> entries;
    QString error_message;
    QTextStream in(&file);
    int line_number = 0;

    while (!in.atEnd())
    {
        QString line = in.readLine();
        line_number++;

        if (line.trimmed().isEmpty()) continue;

        const int separator_index = static_cast<int>(line.indexOf('='));

        if (separator_index == -1)
        {
            error_message = QString("Format error at line %1: Missing '=' separator.").arg(line_number);
            break;
        }

        const QString original = line.left(separator_index);
        const QString translated = line.mid(separator_index + 1);

        if (original.isEmpty() || translated.isEmpty())
        {
            error_message = QString("Format error at line %1: Original or translated text is empty.").arg(
                line_number);
            break;
        }

        entries.push_back(Entry{.original = original.toStdU16String(), .translated = translated.toStdU16String()});
    }

    file.close();

    if (error_message.isEmpty() && !io_import_name_set(q_to_u16(text), entries))
    {
        error_message = "Could not create nameset. The name might already exist, or the file contains duplicate entries.";
    }

    if (error_message.isEmpty())
    {
        load_data();
        QMessageBox::information(this, "Success", "Nameset imported successfully.");
    }
    else
    {
        QMessageBox::critical(this, "Import Failed", error_message + "\n\nNo changes were made to the database.");
    }
}
