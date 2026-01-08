#include <QSqlQuery>
#include <QInputDialog>
#include <QMessageBox>

#include "namesetsmanager.h"
#include "ui_NamesetsManager.h"
#include "core/trie.h"

NamesetsManager::NamesetsManager(QWidget* parent) :
    QDialog(parent), ui(new Ui::NamesetsManager)
{
    ui->setupUi(this);

    setWindowTitle("Namesets");
    
    ui->name_sets_list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->name_sets_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->name_sets_list->setColumnWidth(1, 80);
    ui->name_sets_list->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->name_sets_list->setColumnWidth(2, 150);

    connect(ui->add_new, &QPushButton::clicked, this, &NamesetsManager::add_new_name_set);

    load_data();
}

NamesetsManager::~NamesetsManager()
{
    delete ui;
}

void NamesetsManager::load_data()
{
    QMap<int, int> counts;
    QSqlQuery query("SELECT set_id, COUNT(*) FROM name_set_entries GROUP BY set_id");
    while (query.next()) {
        counts.insert(query.value(0).toInt(), query.value(1).toInt());
    }

    for (const auto& [index, title] : name_sets) 
    {
        const int row = ui->name_sets_list->rowCount();
        ui->name_sets_list->insertRow(row);

        auto* titleItem = new QTableWidgetItem(title);
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

    const auto edit = new QPushButton("Edit");
    edit->setObjectName("edit_set");
    connect(edit, &QPushButton::clicked, this, [this, id, current_title]() {
        bool ok;
        const QString text = QInputDialog::getText(this, "Edit Nameset",
                                             "New title:", QLineEdit::Normal,
                                             current_title, &ok);
        if (ok && !text.isEmpty()) {
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

    connect(delete_button, &QPushButton::clicked, this, [this, id]() {
        const auto reply = QMessageBox::question(this, "Confirm deletion",
                                           "Are you sure? This will delete all names inside this set.",
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            QSqlQuery q;

            q.prepare("DELETE FROM name_sets WHERE id = :id");
            q.bindValue(":id", id);
            q.exec();
            load_data();
        }
    });

    layout->addWidget(edit);
    layout->addWidget(delete_button);

    return container;
}

void NamesetsManager::add_new_name_set()
{
    bool ok;

    if (const QString text = QInputDialog::getText(this, "New Nameset", "Name:", QLineEdit::Normal, "", &ok); ok && !text.isEmpty()) {
        QSqlQuery q;
        q.prepare("INSERT INTO name_sets (title) VALUES (:title)");
        q.bindValue(":title", text);

        if (q.exec()) {
            const int new_id = q.lastInsertId().toInt();
            name_sets.push_back({new_id, text});
            load_data();
        } else {
            QMessageBox::warning(this, "Error", "Could not create nameset (Duplicate name?).");
        }
    }
}
