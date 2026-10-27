#include "desktop_io.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QTextStream>

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

int save_to_file(const QString& name, const QString& text)
{
    if (!name.isEmpty())
    {
        if (QFile file(name); file.open(QIODevice::WriteOnly | QFile::Text))
        {
            QTextStream out(&file);
            out << text;
            out.flush();

            file.close();

            return 0;
        }
    }

    return 1;
}


