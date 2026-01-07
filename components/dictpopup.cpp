#include <QListWidgetItem>

#include "dictpopup.h"

#include "chiplist.h"
#include "ui_DictPopup.h"
#include "../core/trie.h"
#include "../core/io.h"


DictPopup::DictPopup(QWidget* parent) :
    QDialog(parent), ui(new Ui::DictPopup)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    setWindowTitle("Change name/phrase");

    connect(ui->close_popup, &QPushButton::clicked, this, [this]
    {
       close();
    });

    connect(ui->delete_node_name, &QPushButton::clicked, this, [this]
    {
        io_remove(ui->original->text(), NAME);
        close();
    });

    connect(ui->delete_node_phrase, &QPushButton::clicked, this, [this]
    {
        io_remove(ui->original->text(), PHRASE);
        close();
    });

    phrase_chip_delegate = new ChipDelegate(this);
    ui->list_phrases->setItemDelegate(phrase_chip_delegate);
    connect(phrase_chip_delegate, &ChipDelegate::request_delete, this, [this](const QModelIndex& index)
    {
        const auto item = ui->list_phrases->takeItem(index.row());
        io_remove_meaning(ui->original->text(), item->text(), PHRASE);
        delete item;
    });

    name_chip_delegate = new ChipDelegate(this);
    ui->list_names->setItemDelegate(name_chip_delegate);
    connect(name_chip_delegate, &ChipDelegate::request_delete, this, [this](const QModelIndex& index)
    {
        const auto item = ui->list_names->takeItem(index.row());
        io_remove_meaning(ui->original->text(), item->text(), NAME);
        delete item;
    });

    connect(ui->add_phrase, &QPushButton::clicked, this, &DictPopup::add_new_phrase);
    connect(ui->add_name, &QPushButton::clicked, this, &DictPopup::add_new_name);

    connect(phrase_chip_delegate, &QAbstractItemDelegate::closeEditor, this, &DictPopup::edit_finished_phrase);
    connect(name_chip_delegate, &QAbstractItemDelegate::closeEditor, this, &DictPopup::edit_finished_name);

    connect(phrase_chip_delegate, &ChipDelegate::editing_started, this, [this]() { ui->add_phrase->setEnabled(false); });
    connect(phrase_chip_delegate, &ChipDelegate::editing_finished, this, [this]() { ui->add_phrase->setEnabled(true); });

    connect(name_chip_delegate, &ChipDelegate::editing_started, this, [this]() { ui->add_name->setEnabled(false); });
    connect(name_chip_delegate, &ChipDelegate::editing_finished, this, [this]() { ui->add_name->setEnabled(true); });

    connect(ui->use_sv_reading_phrase, &QPushButton::clicked, this, [this]()
    {
        if (phrase_chip_delegate->get_current_editor())
        {
            phrase_chip_delegate->set_editor_text(ui->sv_reading->text());
        }
    });
    connect(ui->use_sv_reading_name, &QPushButton::clicked, this, [this]()
    {
        if (name_chip_delegate->get_current_editor())
        {
            name_chip_delegate->set_editor_text(ui->sv_reading->text());
        }
    });

    connect(ui->cap_none_name, &QPushButton::clicked, this, [this]
    {
        capitalize(0, 0);
    });
    connect(ui->cap_1_name, &QPushButton::clicked, this, [this]
    {
        capitalize(0, 1);
    });
    connect(ui->cap_2_name, &QPushButton::clicked, this, [this]
    {
        capitalize(0, 2);
    });
    connect(ui->cap_3_name, &QPushButton::clicked, this, [this]
    {
        capitalize(0, 3);
    });
    connect(ui->cap_all_name, &QPushButton::clicked, this, [this]
    {
        capitalize(0, 4);
    });

    connect(ui->cap_none_phrase, &QPushButton::clicked, this, [this]
    {
        capitalize(1, 0);
    });
    connect(ui->cap_1_phrase, &QPushButton::clicked, this, [this]
    {
        capitalize(1, 1);
    });
    connect(ui->cap_2_phrase, &QPushButton::clicked, this, [this]
    {
        capitalize(1, 2);
    });
    connect(ui->cap_3_phrase, &QPushButton::clicked, this, [this]
    {
        capitalize(1, 3);
    });
    connect(ui->cap_all_phrase, &QPushButton::clicked, this, [this]
    {
        capitalize(1, 4);
    });

    connect(name_chip_delegate, &ChipDelegate::editing_started, this, [this]
    {
        ui->list_names->setDragEnabled(false);
    });
    connect(name_chip_delegate, &ChipDelegate::editing_finished, this, [this]
    {
        ui->list_names->setDragEnabled(true);
    });

    connect(phrase_chip_delegate, &ChipDelegate::editing_started, this, [this]
    {
        ui->list_phrases->setDragEnabled(false);
    });
    connect(phrase_chip_delegate, &ChipDelegate::editing_finished, this, [this]
    {
        ui->list_phrases->setDragEnabled(true);
    });

    connect(ui->list_names, &ChipList::order_changed, this, [this]
    {
        if (ui->list_names->count() > 0)
        {
            QStringList list;
            for(int i = 0; i < ui->list_names->count(); ++i)
            {
                list.append(ui->list_names->item(i)->text());
            }
            io_reorder(ui->original->text(), list, NAME);
        }
    });

    connect(ui->list_phrases, &ChipList::order_changed, this, [this]
    {
        if (ui->list_phrases->count() > 0)
        {
            QStringList list;
            for(int i = 0; i < ui->list_phrases->count(); ++i)
            {
                list.append(ui->list_phrases->item(i)->text());
            }
            io_reorder(ui->original->text(), list, PHRASE);
        }
    });
}

void DictPopup::add_new_phrase() const
{
    auto* item = new QListWidgetItem("");

    ui->list_phrases->insertItem(0, item);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    ui->list_phrases->editItem(item);
    ui->list_phrases->scrollToItem(item);
}

void DictPopup::edit_finished_phrase() const
{
    QListWidgetItem* item = ui->list_phrases->item(0);
    if (!item) return;

    if (item->text().trimmed().isEmpty())
    {
        delete ui->list_phrases->takeItem(0);
        return;
    }
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);

    io_insert(ui->original->text(), item->text(), PHRASE);
}

void DictPopup::add_new_name() const
{
    auto* item = new QListWidgetItem("");

    ui->list_names->insertItem(0, item);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    ui->list_names->editItem(item);
    ui->list_names->scrollToItem(item);
}

void DictPopup::edit_finished_name() const
{
    QListWidgetItem* item = ui->list_names->item(0);
    if (!item) return;

    if (item->text().trimmed().isEmpty())
    {
        delete ui->list_names->takeItem(0);
        return;
    }
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    io_insert(ui->original->text(), item->text(), NAME);
}

DictPopup::~DictPopup()
{
    delete ui;
}

void DictPopup::capitalize(const int type, const int mode) const
{
    const auto delegate = type ? phrase_chip_delegate : name_chip_delegate;
    const QLineEdit* editor = delegate->get_current_editor();
    if (!editor) return;

    const QString text = editor->text();
    if (text.isEmpty()) return;

    QStringList words = text.split(' ');

    for (int i = 0; i < words.size(); ++i)
    {
        if (words[i].isEmpty()) continue;

        if (mode == 0)
        {
            words[i] = words[i].toLower();
        }
        else if (mode == 4)
        {
            words[i] = words[i].toLower();
            words[i][0] = words[i][0].toUpper();
        }
        else
        {
            words[i] = words[i].toLower();
            if (i < mode)
            {
                words[i][0] = words[i][0].toUpper();
            }
        }
    }
    delegate->set_editor_text(words.join(' '));
}

void DictPopup::load_data(const QString& selected_chinese_text) const
{
    ui->original->setText(selected_chinese_text);
    QString sv_reading;
    for (const auto& ch : selected_chinese_text)
    {
        if (sv_readings.contains(ch))
        {
            sv_reading.append(sv_readings[ch]);
        }
        else
        {
            sv_reading.append(ch);
        }
        sv_reading.append(" ");
    }
    sv_reading.resize(sv_reading.size() - 1);
    ui->sv_reading->setText(sv_reading);

    const auto [names, phrases] = dictionary.find_exact(selected_chinese_text);
    if (names != nullptr && !names->empty())
    {
        for (const auto& name : *names)
        {
            auto* item = new QListWidgetItem(name);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->list_names->addItem(item);
        }
    }

    if (phrases != nullptr && !phrases->empty())
    {
        for (const auto& name : *phrases)
        {
            auto* item = new QListWidgetItem(name);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            ui->list_phrases->addItem(item);
        }
    }
}
