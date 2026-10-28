#include "namesetchooser.h"
#include "ui_namesetchooser.h"
#include "core/db.h"
#include "core/dict.h"
#include "core/structures.h"

namesetchooser::namesetchooser(QWidget* parent) :
    QDialog(parent), ui(new Ui::namesetchooser)
{
    ui->setupUi(this);
    setWindowTitle("Choose Nameset");
    resize(500, 500);

    ui->name_set_list->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->name_set_list->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->name_set_list->setColumnWidth(1, 80);
    ui->name_set_list->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->name_set_list->setColumnWidth(2, 100);
    ui->name_set_list->verticalHeader()->setVisible(false);

    connect(ui->name_set_choose_none, &QPushButton::clicked, this, [this]
    {
        chosen_id = -1;
        accept();
    });

    connect(ui->name_set_search, &QLineEdit::textChanged, this, [this](const QString& text)
    {
        load_data(text);
    });

    load_data();
    ui->name_set_search->setFocus();
}

namesetchooser::~namesetchooser()
{
    delete ui;
}

int namesetchooser::get_chosen_id() const
{
    return chosen_id;
}

void namesetchooser::load_data(const QString& filter)
{
    ui->name_set_list->setRowCount(0);
    const auto counts = nameset_db_entry_counts();

    for (const auto& [index, title] : name_sets)
    {
        QString qtitle = QString::fromUtf16(title.data(), static_cast<qsizetype>(title.size()));
        if (!filter.isEmpty() && !qtitle.contains(filter, Qt::CaseInsensitive))
        {
            continue;
        }

        const int row = ui->name_set_list->rowCount();
        ui->name_set_list->insertRow(row);

        auto* titleItem = new QTableWidgetItem(qtitle);
        titleItem->setFlags(titleItem->flags() & ~Qt::ItemIsEditable);
        ui->name_set_list->setItem(row, 0, titleItem);

        const auto count_it = counts.find(index);
        const int count = count_it != counts.end() ? count_it->second : 0;
        auto* count_item = new QTableWidgetItem(QString::number(count));

        count_item->setTextAlignment(Qt::AlignCenter);
        count_item->setFlags(count_item->flags() & ~Qt::ItemIsEditable);

        ui->name_set_list->setItem(row, 1, count_item);

        const auto container = new QWidget();
        const auto layout = new QHBoxLayout(container);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->setSpacing(5);

        const auto choose_button = new QPushButton("Choose");
        choose_button->setObjectName("choose_set");
        connect(choose_button, &QPushButton::clicked, this, [this, index]
        {
            chosen_id = index;
            accept();
        });

        layout->addWidget(choose_button);
        ui->name_set_list->setCellWidget(row, 2, container);
    }
}
