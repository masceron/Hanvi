#include "convert.h"

#include "../convert/trie.h"

void convert()
{
    tokenize();
}

void tokenize()
{
    int i = 0;
    int tokenIndex = 0;
    QString style = "text-decoration: none; color: white; font-size: 16px";

    output.first.clear();
    output.second.clear();

    bool cap_next = true;

    while (i < input.length())
    {
        QChar ch = input[i];

        if (ch == '\n')
        {
            output.first.append("<br>");
            output.second.append("<br>");
            cap_next = true;
            i++;
            continue;
        }
        if (ch.isSpace())
        {
            output.first.append("&nbsp;");
            output.second.append("&nbsp;");
            i++;
            continue;
        }

        QString source_text;
        QString translated_text;
        int step = 0;
        bool is_punctuation = false;

        if (const Match result = dictionary.find(input, i); result.length > 0)
        {
            source_text = input.mid(i, result.length);
            translated_text = result.translation;
            step = result.length;
        }
        else if (sv_readings.contains(ch))
        {
            source_text = ch;
            translated_text = sv_readings[ch];
            step = 1;
        }
        else
        {
            source_text = ch;
            const auto mapped_value = punctuations.value(ch);
            translated_text = !mapped_value.isNull() ? mapped_value : ch;
            step = 1;

            if (QString(".!?…:;\"").contains(translated_text))
            {
                cap_next = true;
                is_punctuation = true;
            }
            else if (QString(",").contains(translated_text))
            {
                is_punctuation = true;
            }
        }

        if (!is_punctuation && cap_next && !translated_text.isEmpty())
        {
            translated_text[0] = translated_text[0].toUpper();
            cap_next = false;
        }

        QString uid = QString("token_%1").arg(tokenIndex++);
        QString safe_src = source_text.toHtmlEscaped();
        QString safe_trans = translated_text.toHtmlEscaped();

        output.first.append(QString("<a name='%1' href='%1' style='%2'>%3</a>")
            .arg(uid, style, safe_src));

        bool no_space_after = false;

        if (QString("“‘([<{").contains(ch))
        {
            no_space_after = true;
        }
        else if (i + step < input.length())
        {
            if (const QChar next_char = input[i + step]; QString(".,;:!?)]}>\"'”’，。：；！？").contains(next_char))
            {
                no_space_after = true;
            }
        }

        output.second.append(QString("<a href='%1' style='%2'>%3</a>")
            .arg(uid, style, safe_trans));

        if (!no_space_after)
        {
            output.second.append(" ");
        }

        i += step;
    }
}
