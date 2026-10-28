#include "dictpopup.h"
#include "chiplist.h"
#include "ui_dictpopup.h"
#include "qt_utils.h"
#include "../core/structures.h"
#include "../core/io.h"
#include "../core/dict.h"
#include "../core/unicode_utils.h"


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
        io_remove(ui->use_current_nameset->isChecked() ? current_name_set_id : -1, q_to_u16(ui->original->text()), NAME);
        accept();
    });

    connect(ui->delete_node_phrase, &QPushButton::clicked, this, [this]
    {
        io_remove(-1, q_to_u16(ui->original->text()), PHRASE);
        accept();
    });

    phrase_chip_delegate = new ChipDelegate(this);
    ui->list_phrases->setItemDelegate(phrase_chip_delegate);
    connect(phrase_chip_delegate, &ChipDelegate::request_delete, this, [this](const QModelIndex& index)
    {
        const auto item = ui->list_phrases->takeItem(index.row());
        io_remove_meaning(q_to_u16(ui->original->text()), q_to_u16(item->text()));
        delete item;
        changed = true;
    });

    connect(ui->add_phrase, &QPushButton::clicked, this, &DictPopup::add_new_phrase);
    connect(ui->add_name, &QPushButton::clicked, this, &DictPopup::add_new_name);

    connect(ui->current_name, &QLineEdit::editingFinished, this, &DictPopup::edit_finished_name);

    connect(phrase_chip_delegate, &QAbstractItemDelegate::closeEditor, this, &DictPopup::edit_finished_phrase);

    connect(phrase_chip_delegate, &ChipDelegate::editing_started, this,
            [this]() { ui->add_phrase->setEnabled(false); });
    connect(phrase_chip_delegate, &ChipDelegate::editing_finished, this,
            [this]() { ui->add_phrase->setEnabled(true); });

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
            for (int i = 0; i < ui->list_phrases->count(); ++i)
            {
                list.append(ui->list_phrases->item(i)->text());
            }
            io_reorder(q_to_u16(ui->original->text()), to_u16_vector(list));
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
        io_insert(ui->use_current_nameset->isChecked() ? current_name_set_id : -1, q_to_u16(ui->original->text()), q_to_u16(new_text),
                  NAME);

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

    io_insert(-1, q_to_u16(ui->original->text()), q_to_u16(item->text()), PHRASE);
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
    int idx = 0;
    while (idx < selected_chinese_text.length())
    {
        const QChar ch = selected_chinese_text[idx];
        const bool is_surrogate = ch.isHighSurrogate() && idx + 1 < selected_chinese_text.length() && selected_chinese_text[idx + 1].isLowSurrogate();
        const int char_len = is_surrogate ? 2 : 1;
        const char32_t cp = is_surrogate
            ? QChar::surrogateToUcs4(ch, selected_chinese_text[idx + 1])
            : static_cast<char32_t>(ch.unicode());

        bool found_sv = false;
        if (auto it = sv_readings.find(cp); it != sv_readings.end())
        {
            sv_reading.append(QString::fromUtf16(it->second.data(), static_cast<qsizetype>(it->second.size())));
            found_sv = true;
        }
        else if (auto vit = variants.find(cp); vit != variants.end())
        {
            const char32_t canon_cp = hanvi::utf16_code_point_at(vit->second, 0);
            if (auto it2 = sv_readings.find(canon_cp); it2 != sv_readings.end())
            {
                sv_reading.append(QString::fromUtf16(it2->second.data(), static_cast<qsizetype>(it2->second.size())));
                found_sv = true;
            }
        }
        if (!found_sv)
        {
            sv_reading.append(selected_chinese_text.sliced(idx, char_len));
        }

        idx += char_len;

        if (idx < selected_chinese_text.length())
        {
            const bool next_is_surrogate = selected_chinese_text[idx].isHighSurrogate() && idx + 1 < selected_chinese_text.length() && selected_chinese_text[idx + 1].isLowSurrogate();
            if (is_surrogate && !found_sv && next_is_surrogate)
            {
                const char32_t next_cp = QChar::surrogateToUcs4(selected_chinese_text[idx], selected_chinese_text[idx + 1]);
                const bool next_has_sv = sv_readings.contains(next_cp) ||
                    (variants.contains(next_cp) && sv_readings.contains(hanvi::utf16_code_point_at(variants.at(next_cp), 0)));
                if (!next_has_sv)
                {
                    continue;
                }
            }
            sv_reading.append(" ");
        }
    }
    if (!sv_reading.isEmpty() && sv_reading.endsWith(' '))
    {
        sv_reading.resize(sv_reading.size() - 1);
    }
    ui->sv_reading->setText(sv_reading);

    bool set_name_found = false;

    if (current_name_set_id != -1)
    {
        ui->use_current_nameset->setChecked(true);
        if (const auto res_set = name_set_dictionary.find_exact(q_to_u16(selected_chinese_text)); !res_set.name.isNull())
        {
            set_name_found = true;
            ui->use_current_nameset->setCheckState(Qt::CheckState::Checked);
            ui->current_name->setText(to_qstring(res_set.name));
        }
    }

    const auto res = dictionary.find_exact(q_to_u16(selected_chinese_text));
    if (!set_name_found && !res.name.isNull())
    {
        ui->current_name->setText(to_qstring(res.name));
        ui->use_current_nameset->setChecked(false);
    }

    if (!res.full_phrase.isNull())
    {
        const auto phrases = res.full_phrase.split(u'\x1F');
        for (const auto& phrase : phrases)
        {
            auto* item = new QListWidgetItem(to_qstring(phrase));
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
