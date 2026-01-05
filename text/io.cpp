#include <QApplication>
#include <QClipboard>
#include <QFile>

#include "io.h"

#include "convert.h"

void load_from_clipboard()
{
    if (const auto clipboard_text = QApplication::clipboard()->text(); !clipboard_text.isEmpty())
    {
        input = clipboard_text;
    }
}

void load_from_file(const QString& name)
{
    if (!name.isEmpty())
    {
        if (QFile file(name); file.open(QIODevice::ReadOnly | QFile::Text))
        {
            QTextStream ins(&file);
            input = file.readAll();
            file.close();
        }
    }
}
