#include <QApplication>
#include <QClipboard>
#include <QFile>

#include "io.h"

QList<QStringView> paginate(const QString& input_text, const int min_length)
{
    QList<QStringView> pages;
    const QStringView full_view(input_text);

    int cursor = 0;
    const int length = full_view.length();

    while (cursor < length) {
        const int target_end = cursor + min_length;

        if (target_end >= length) {
            pages.append(full_view.mid(cursor));
            break;
        }
        if (const int cutoff = full_view.indexOf('\n', target_end); cutoff == -1) {
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
    if (const auto clipboard_text = QApplication::clipboard()->text(); !clipboard_text.isEmpty())
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
