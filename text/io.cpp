#include <QApplication>
#include <QClipboard>
#include <QFile>

#include "io.h"

#include "db.h"

QList<QStringView> paginate(const QString& input_text, const int min_length)
{
    QList<QStringView> pages;
    const QStringView full_view(input_text);

    int cursor = 0;
    const int length = static_cast<int>(full_view.length());

    while (cursor < length) {
        const int target_end = cursor + min_length;

        if (target_end >= length) {
            pages.append(full_view.mid(cursor));
            break;
        }
        if (const int cutoff = static_cast<int>(full_view.indexOf('\n', target_end)); cutoff == -1) {
            pages.append(full_view.mid(cursor));
            break;
        } else {
            const int chunk_len = (cutoff + 1) - cursor;
            pages.append(full_view.mid(cursor, chunk_len));

            cursor = cutoff + 1;
        }
    }

    return pages;
}

std::expected<QString, io_error> load_from_clipboard()
{
    if (auto clipboard_text = QApplication::clipboard()->text(); !clipboard_text.isEmpty())
    {
        return clipboard_text;
    }
    return std::unexpected(io_error::not_text);
}

std::expected<QString, io_error> load_from_file(const QString& name)
{
    if (!name.isEmpty())
    {
        if (QFile file(name); file.open(QIODevice::ReadOnly | QFile::Text))
        {
            QTextStream ins(&file);
            auto input = file.readAll();
            file.close();

            if (input.isEmpty()) return std::unexpected(io_error::file_not_readable);

            return input;
        }
        return std::unexpected(io_error::not_text);
    }

    return std::unexpected(io_error::file_not_readable);
}

void io_insert(const QString& key, const QString& value, Priority priority)
{
    dictionary.insert(key, value, priority);
    db_insert(key, value, priority);
}
void io_reorder(const QString& key, const QStringList& new_order, Priority priority)
{
    dictionary.reorder(key, new_order, priority);
    db_reorder(key, new_order, priority);
}
void io_remove(const QString& key, Priority priority)
{
    dictionary.remove(key, priority);
    db_remove(key, priority);
}
void io_remove_meaning(const QString& key, const QString& value, Priority priority)
{
    dictionary.remove_meaning(key, value, priority);
    db_remove_meaning(key, value, priority);
}
