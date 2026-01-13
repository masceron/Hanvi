#include <QListWidgetItem>

#include "dictpopup.h"
#include "chiplist.h"
#include "ui_DictPopup.h"
#include "../core/structures.h"
#include "../core/io.h"
#include "../core/dict.h"


DictPopup::DictPopup(QWidget* parent) :
    QDialog(parent), ui(new Ui::DictPopup)
{
    ui->setupUi(this);

    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    setWindowTitle("Change name/phrase");

    connect(ui->close_popup, &QPushButton::clicked, this, [this]
    {
       close_popup();
    });

    connect(ui->delete_name, &QPushButton::clicked, this, [this]
    {
        io_remove(ui->use_current_nameset->isChecked() ? current_name_set_id : -1, ui->original->text(), NAME);
        accept();
    });

    connect(ui->delete_node_phrase, &QPushButton::clicked, this, [this]
    {
        io_remove(-1, ui->original->text(), PHRASE);
        accept();
    });

    phrase_chip_delegate = new ChipDelegate(this);
    ui->list_phrases->setItemDelegate(phrase_chip_delegate);
    connect(phrase_chip_delegate, &ChipDelegate::request_delete, this, [this](const QModelIndex& index)
    {
        const auto item = ui->list_phrases->takeItem(index.row());
        io_remove_meaning(ui->original->text(), item->text());
        delete item;
        changed = true;
    });

    connect(ui->add_phrase, &QPushButton::clicked, this, &DictPopup::add_new_phrase);
    connect(ui->add_name, &QPushButton::clicked, this, &DictPopup::add_new_name);

    connect(ui->current_name, &QLineEdit::editingFinished, this, &DictPopup::edit_finished_name);

    connect(phrase_chip_delegate, &QAbstractItemDelegate::closeEditor, this, &DictPopup::edit_finished_phrase);

    connect(phrase_chip_delegate, &ChipDelegate::editing_started, this, [this]() { ui->add_phrase->setEnabled(false); });
    connect(phrase_chip_delegate, &ChipDelegate::editing_finished, this, [this]() { ui->add_phrase->setEnabled(true); });

    connect(ui->use_sv_reading_name, &QPushButton::clicked, this, [this]
    {
        if (!ui->current_name->isReadOnly())
        {
            ui->current_name->setText(ui->sv_reading->text());
        }
    });

    connect(ui->use_sv_reading_phrase, &QPushButton::clicked, this, [this]
    {
        if (phrase_chip_delegate->get_current_editor())
        {
            phrase_chip_delegate->set_editor_text(ui->sv_reading->text());
        }
    });

    connect(ui->cap_none_name, &QPushButton::clicked, this, [this]
    {
        if (!ui->current_name->isReadOnly())
        {
            ui->current_name->setText(capitalize(ui->current_name->text(), 0));
        }
    });
    connect(ui->cap_1_name, &QPushButton::clicked, this, [this]
    {
        if (!ui->current_name->isReadOnly())
        {
            ui->current_name->setText(capitalize(ui->current_name->text(), 1));
        }
    });
    connect(ui->cap_2_name, &QPushButton::clicked, this, [this]
    {
        if (!ui->current_name->isReadOnly())
        {
            ui->current_name->setText(capitalize(ui->current_name->text(), 2));
        }
    });
    connect(ui->cap_3_name, &QPushButton::clicked, this, [this]
    {
        if (!ui->current_name->isReadOnly())
        {
            ui->current_name->setText(capitalize(ui->current_name->text(), 3));
        }
    });
    connect(ui->cap_all_name, &QPushButton::clicked, this, [this]
    {
        if (!ui->current_name->isReadOnly())
        {
            ui->current_name->setText(capitalize(ui->current_name->text(), 4));
        }
    });

    connect(ui->cap_none_phrase, &QPushButton::clicked, this, [this]
    {
        if (const QLineEdit* editor = phrase_chip_delegate->get_current_editor())
        {
            phrase_chip_delegate->set_editor_text(capitalize(editor->text(), 0));
        }
    });
    connect(ui->cap_1_phrase, &QPushButton::clicked, this, [this]
    {
        if (const QLineEdit* editor = phrase_chip_delegate->get_current_editor())
        {
            phrase_chip_delegate->set_editor_text(capitalize(editor->text(), 1));
        }
    });
    connect(ui->cap_2_phrase, &QPushButton::clicked, this, [this]
    {
        if (const QLineEdit* editor = phrase_chip_delegate->get_current_editor())
        {
            phrase_chip_delegate->set_editor_text(capitalize(editor->text(), 2));
        }
    });
    connect(ui->cap_3_phrase, &QPushButton::clicked, this, [this]
    {
        if (const QLineEdit* editor = phrase_chip_delegate->get_current_editor())
        {
            phrase_chip_delegate->set_editor_text(capitalize(editor->text(), 3));
        }
    });
    connect(ui->cap_all_phrase, &QPushButton::clicked, this, [this]
    {
        if (const QLineEdit* editor = phrase_chip_delegate->get_current_editor())
        {
            phrase_chip_delegate->set_editor_text(capitalize(editor->text(), 4));
        }
    });

    connect(phrase_chip_delegate, &ChipDelegate::editing_started, this, [this]
    {
        ui->list_phrases->setDragEnabled(false);
    });
    connect(phrase_chip_delegate, &ChipDelegate::editing_finished, this, [this]
    {
        ui->list_phrases->setDragEnabled(true);
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
            io_reorder(ui->original->text(), list);
            changed = true;
        }
    });

    if (current_name_set_id != -1)
    {
        ui->use_current_nameset->setEnabled(true);
    }
}

void DictPopup::add_new_phrase() const
{
    auto* item = new QListWidgetItem("");

    ui->list_phrases->insertItem(0, item);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    ui->list_phrases->editItem(item);
    ui->list_phrases->scrollToItem(item);
}

void DictPopup::add_new_name()
{
    original_name = ui->current_name->text();
    ui->current_name->clear();
    ui->current_name->setReadOnly(false);
    ui->current_name->setFocus();
}

void DictPopup::edit_finished_name()
{
    if (ui->current_name->isReadOnly()) return;

    if (const QString new_text = ui->current_name->text().trimmed(); new_text.isEmpty())
    {
        ui->current_name->setText(original_name);
    }
    else
    {
        io_insert(ui->use_current_nameset->isChecked() ? current_name_set_id : -1, ui->original->text(), new_text, NAME);

        original_name = new_text;
        changed = true;
    }

    ui->current_name->setReadOnly(true);
}

void DictPopup::edit_finished_phrase()
{
    QListWidgetItem* item = ui->list_phrases->item(0);
    if (!item) return;

    if (item->text().trimmed().isEmpty())
    {
        delete ui->list_phrases->takeItem(0);
        return;
    }
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);

    io_insert(-1, ui->original->text(), item->text(), PHRASE);
    changed = true;
}

DictPopup::~DictPopup()
{
    delete ui;
}

QString DictPopup::capitalize(const QString& text, const int mode)
{
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
    return words.join(' ');
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

    bool set_name_found = false;

    if (current_name_set_id != -1)
    {
        ui->use_current_nameset->setChecked(true);
        if (const auto [set_name, _] = name_set_dictionary.find_exact(selected_chinese_text); set_name != nullptr)
        {
            set_name_found = true;
            ui->use_current_nameset->setCheckState(Qt::CheckState::Checked);
            ui->current_name->setText(*set_name);
        }
    }

    const auto [global_name, phrases] = dictionary.find_exact(selected_chinese_text);
    if (!set_name_found && global_name != nullptr)
    {
        ui->current_name->setText(*global_name);
        ui->use_current_nameset->setChecked(false);
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

void DictPopup::close_popup()
{
    if (changed) accept();
    else close();
}
