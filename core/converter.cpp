#include "converter.h"
#include "trie.h"

QString get_sv(const QStringView& cn)
{
    QString out;
    QString sv_reading;
    for (const auto& ch : cn)
    {
        if (sv_readings.contains(ch))
        {
            sv_reading.append(sv_readings[ch]);
        }
        else
        {
            if (punctuations.contains(ch))
            {
                sv_reading.append(punctuations[ch]);
            }
            else sv_reading.append(ch);
        }
        sv_reading.append(" ");
    }
    sv_reading.resize(sv_reading.size() - 1);

    return sv_reading;
}

std::tuple<QString, QString, QString> convert(const QStringView& input, const std::function<void(int)>& progress_callback)
{
    QString cn_output;
    QString sv_output;
    QString vn_output;
    int i = 0;
    int token_index = 0;

    cn_output.append(R"(
    <style>
        a { text-decoration: none; color: white; font-family: "Noto Sans SC"; font-size: 18px; }
    </style>
    )");
    sv_output.append(R"(
    <style>
        a { text-decoration: none; color: white; font-family: "Tahoma"; font-size: 16px; }
    </style>
    )");
    vn_output.append(R"(
    <style>
        a { text-decoration: none; color: white; font-family: "Tahoma"; font-size: 16px; }
    </style>
    )");

    bool cap_next = true;

    while (i < input.length())
    {
        if (progress_callback && i % 2500 == 0) {
            progress_callback(i);
        }

        QChar ch = input[i];

        if (ch == '\n')
        {
            cn_output.append("<br>");
            sv_output.append("<br>");
            vn_output.append("<br>");
            cap_next = true;
            i++;
            continue;
        }
        if (ch.isSpace())
        {
            cn_output.append("&nbsp;");
            sv_output.append("&nbsp;");
            vn_output.append("&nbsp;");
            i++;
            continue;
        }

        QStringView source_text;
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
            source_text = input.mid(i, 1);
            translated_text = sv_readings[ch];
            step = 1;
        }
        else
        {
            source_text = input.mid(i, 1);
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

        QString sv = get_sv(source_text);

        if (!is_punctuation && cap_next)
        {
            if (!translated_text.isEmpty())
            {
                translated_text[0] = translated_text[0].toUpper();
            }
            sv[0] = sv[0].toUpper();
            cap_next = false;
        }

        QString uid = QString("token_%1").arg(token_index++);
        QString safe_src = source_text.toString().toHtmlEscaped();

        cn_output.append(QString("<a href='%1'>%2</a>")
            .arg(uid, safe_src));

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

        sv_output.append(QString("<a href='%1'>%2</a>")
            .arg(uid, sv.toHtmlEscaped()));

        vn_output.append(QString("<a href='%1'>%2</a>")
            .arg(uid, translated_text.toHtmlEscaped()));

        if (!no_space_after)
        {
            sv_output.append(" ");
            if (!translated_text.isEmpty())
            {
                vn_output.append(" ");
            }
        }

        i += step;
    }

    return {cn_output, sv_output, vn_output};
}

QString convert_plain(const QStringView& input, const std::function<void(int)>& progress_callback)
{
    QString output;
    output.reserve(input.length() * 2);

    int i = 0;
    bool cap_next = true;

    while (i < input.length())
    {
        if (progress_callback && i % 1000 == 0) progress_callback(i);

        QChar ch = input[i];
        if (ch == '\n')
        {
            output.append("\n");
            cap_next = true;
            i++;
            continue;
        }

        if (ch.isSpace())
        {
            output.append(" ");
            i++;
            continue;
        }

        QString translated_text;
        int step = 0;
        bool is_punctuation = false;

        if (const Match result = dictionary.find(input, i); result.length > 0)
        {
            translated_text = result.translation;
            step = result.length;
        }
        else if (sv_readings.contains(ch))
        {
            translated_text = sv_readings[ch];
            step = 1;
        }
        else
        {
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
            if (translated_text[0].isLower()) {
                translated_text[0] = translated_text[0].toUpper();
            }
            cap_next = false;
        }

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

        output.append(translated_text);

        if (!no_space_after && !translated_text.isEmpty())
        {
            output.append(" ");
        }

        i += step;
    }

    return output;
}