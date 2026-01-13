#include <QStringBuilder>
#include <optional>

#include "converter.h"
#include "structures.h"
#include "dict.h"

struct Progress
{
    const std::function<void(int)>& progress_callback = nullptr;
    int next_val = 2500;
    int current = 0;

    void update(const int n)
    {
        current += n;
        if (progress_callback && current >= next_val)
        {
            progress_callback(current);
            next_val += 2500;
        }
    }
};

QString get_sv(const QStringView& cn)
{
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
    if (!sv_reading.isEmpty()) sv_reading.resize(sv_reading.size() - 1);

    return sv_reading;
}

static bool should_append_space(const QStringView& input, const int current_end_idx,
                                const QChar current_char_source = QChar())
{
    static constexpr QStringView openers(u"“‘([<{");
    if (!current_char_source.isNull())
    {
        if (openers.contains(current_char_source))
        {
            return false;
        }
    }

    static constexpr QStringView closers(u".,，;:!?)]}>\"'”’，。：；！？");

    if (current_end_idx < input.length())
    {
        if (const QChar next_char = input[current_end_idx]; closers.contains(next_char))
        {
            return false;
        }
    }

    return true;
}

static int is_optimal_phrase(const QStringView& text, const int current_pos, const int current_len)
{
    const int threshold = std::max(current_len, 3);

    const int limit = current_pos + current_len;

    for (int next_start = current_pos + 1; next_start < limit; ++next_start)
    {
        if (const Match match = dictionary.find(text, next_start); match.length > threshold)
        {
            return next_start;
        }
    }
    return -1;
}

struct ConversionResult
{
    QString cn;
    QString sv;
    QString vn;
    int length_consumed = 0;
};

struct RuleMatch
{
    const Rule* rule;
    int abs_start_of_end_token; // Where the end token starts in the text
    int total_end_pos; // Where the entire rule ends (start_of_end + length)
};

std::optional<RuleMatch> find_matching_rule(const QStringView& text, const int current_pos,
                                            const std::vector<Rule>& rules)
{
    static constexpr QStringView stoppers(u"，。：；！？“”’.,，;:!?)]}>\"'");
    int limit = std::min(static_cast<int>(text.length()), current_pos + 25);

    for (int i = current_pos; i < limit; ++i)
    {
        if (const QChar ch = text[i]; stoppers.contains(ch))
        {
            limit = i;
            break;
        }
    }

    const QStringView search_area = text.mid(current_pos, limit - current_pos);
    std::optional<RuleMatch> best_match = std::nullopt;

    for (const auto& rule : rules)
    {
        const int start_len = static_cast<int>(rule.original_start.length());

        if (search_area.length() <= start_len) continue;

        if (const int relative_end_idx = static_cast<int>(search_area.indexOf(rule.original_end, start_len));
            relative_end_idx != -1)
        {
            const int abs_start_of_end = current_pos + relative_end_idx;
            const int total_end = abs_start_of_end + static_cast<int>(rule.original_end.length());

            if (!best_match.has_value())
            {
                best_match = RuleMatch{&rule, abs_start_of_end, total_end};
                continue;
            }

            if (total_end > best_match->total_end_pos)
            {
                best_match = RuleMatch{&rule, abs_start_of_end, total_end};
            }
            else if (total_end == best_match->total_end_pos)
            {
                if (rule.original_end.length() > best_match->rule->original_end.length())
                {
                    best_match = RuleMatch{&rule, abs_start_of_end, total_end};
                }
            }
        }
    }

    return best_match;
}

ConversionResult convert_recursive(const QStringView& input, int start_offset, int& token_counter, bool& cap_next,
                                   Progress& progress)
{
    ConversionResult out;
    int i = 0;

    while (i < input.length())
    {
        QChar ch = input[i];

        if (ch == '\n')
        {
            out.cn += u"<br>";
            out.sv += u"<br>";
            out.vn += u"<br>";
            cap_next = true;
            i++;
            out.length_consumed++;

            progress.update(1);
            continue;
        }
        if (ch.isSpace())
        {
            out.cn += u"&nbsp;";
            out.sv += u"&nbsp;";
            out.vn += u"&nbsp;";
            i++;
            out.length_consumed++;

            progress.update(1);
            continue;
        }

        if (current_name_set_id != -1)
        {
            if (Match match = name_set_dictionary.find(input, i); match.length > 0 && match.priority == NAME)
            {
                QString uid = QString::number(token_counter++);
                QString sv = get_sv(input.mid(i, match.length));
                if (cap_next)
                {
                    sv[0] = sv[0].toUpper();
                    cap_next = false;
                }

                QString trans = *match.translation;

                QString safe_src = input.mid(i, match.length).toString().toHtmlEscaped();
                out.cn += u"<a href='" % uid % u"'>" % safe_src % u"</a>";
                out.sv += u"<a href='" % uid % u"'>" % sv.toHtmlEscaped() % u"</a>";
                out.vn += u"<a href='" % uid % u"'>" % trans.toHtmlEscaped() % u"</a>";

                i += match.length;
                out.length_consumed += match.length;

                progress.update(match.length);

                if (should_append_space(input, i) && !out.vn.endsWith(' '))
                {
                    out.vn += u" ";
                    out.sv += u" ";
                }

                continue;
            }
        }

        auto [length, priority, rules, translation] = dictionary.find(input, i);

        if (length > 0 && priority == NAME)
        {
            QString uid = QString::number(token_counter++);
            QString sv = get_sv(input.mid(i, length));
            if (cap_next)
            {
                sv[0] = sv[0].toUpper();
                cap_next = false;
            }

            QString trans = *translation;

            QString safe_src = input.mid(i, length).toString().toHtmlEscaped();
            out.cn += u"<a href='" % uid % u"'>" % safe_src % u"</a>";
            out.sv += u"<a href='" % uid % u"'>" % sv.toHtmlEscaped() % u"</a>";
            out.vn += u"<a href='" % uid % u"'>" % trans.toHtmlEscaped() % u"</a>";

            i += length;
            out.length_consumed += length;

            progress.update(length);

            if (should_append_space(input, i) && !out.vn.endsWith(' '))
            {
                out.vn += u" ";
                out.sv += u" ";
            }

            continue;
        }

        if (rules != nullptr)
        {
            if (auto rule_match = find_matching_rule(input, i, *rules))
            {
                const Rule* rule = rule_match->rule;
                int rule_start_len = static_cast<int>(rule->original_start.length());

                bool phrase_overrides_rule = (length > 0 && priority == PHRASE &&
                                              length > rule_start_len);

                if (!phrase_overrides_rule) {

                    int inner_start_idx = i + rule_start_len;
                    int inner_len = rule_match->abs_start_of_end_token - inner_start_idx;
                    int rule_end_len = static_cast<int>(rule->original_end.length());

                    progress.update(rule_start_len);

                    QString uid = u"r" + QString::number(token_counter++);

                    QString t_start = rule->translation_start;
                    if (cap_next && !t_start.isEmpty())
                    {
                        if (t_start[0].isLower()) t_start[0] = t_start[0].toUpper();
                        cap_next = false;
                    }

                    ConversionResult inner = convert_recursive(input.mid(inner_start_idx, inner_len),
                                                               start_offset + inner_start_idx,
                                                               token_counter,
                                                               cap_next, progress);

                    progress.update(rule_end_len);

                    out.cn += u"<a href='" % uid % u"'>" % rule->original_start.toHtmlEscaped() % u"</a>";
                    out.cn += inner.cn;
                    out.cn += u"<a href='" % uid % u"'>" % rule->original_end.toHtmlEscaped() % u"</a>";

                    QString sv_start = get_sv(rule->original_start);
                    QString sv_end = get_sv(rule->original_end);

                    out.sv += u"<a href='" % uid % u"'>" % sv_start.toHtmlEscaped() % u" </a>";
                    out.sv += inner.sv;
                    out.sv += u"<a href='" % uid % u"'>" % sv_end.toHtmlEscaped() % u"</a> ";

                    if (!t_start.isEmpty())
                    {
                        out.vn += u"<a href='" % uid % u"'>" % t_start.toHtmlEscaped() % u" </a>";
                    }

                    out.vn += inner.vn;

                    if (!rule->translation_end.isEmpty())
                    {
                        out.vn += u"<a href='" % uid % u"'>" % rule->translation_end.toHtmlEscaped() % u"</a>";
                    }

                    i += rule_start_len + inner_len + rule_end_len;
                    out.length_consumed += rule_start_len + inner_len + rule_end_len;

                    if (should_append_space(input, i) && !out.vn.endsWith(' '))
                    {
                        out.vn += u" ";
                    }
                    continue;
                }
            }
        }

        if (length > 0 && priority == PHRASE)
        {
            if (int conflict_start = is_optimal_phrase(input, i, length); conflict_start != -1)
            {
                int max_allowed_len = conflict_start - i;

                length = 0;
                translation = nullptr;

                for (int try_len = max_allowed_len; try_len >= 1; --try_len)
                {
                    if (auto [_, exact_phrases] = dictionary.find_exact(input.mid(i, try_len).toString()); exact_phrases && !exact_phrases->isEmpty())
                    {
                        length = try_len;
                        translation = &exact_phrases->first();
                        break;
                    }
                }
            }

            if (length == 0)
            {
                goto process_single_char;
            }

            QString uid = QString::number(token_counter++);
            QString sv = get_sv(input.mid(i, length));
            QString trans = *translation;

            if (cap_next)
            {
                if (!trans.isEmpty() && trans[0].isLower()) trans[0] = trans[0].toUpper();
                sv[0] = sv[0].toUpper();
                cap_next = false;
            }

            QString safe_src = input.mid(i, length).toString().toHtmlEscaped();
            out.cn += u"<a href='" % uid % u"'>" % safe_src % u"</a>";
            out.sv += u"<a href='" % uid % u"'>" % sv.toHtmlEscaped() % u"</a>";;
            out.vn += u"<a href='" % uid % u"'>" % trans.toHtmlEscaped() % u"</a>";;

            i += length;
            out.length_consumed += length;

            progress.update(length);

            if (should_append_space(input, i) && !out.vn.endsWith(' '))
            {
                out.vn += u" ";
                out.sv += u" ";
            }

            continue;
        }

        process_single_char:
        {
            QString source_text = input[i];
            QString translated_text;
            QString sv_text;
            bool is_punct = false;

            if (sv_readings.contains(ch))
            {
                translated_text = sv_readings[ch];
                sv_text = translated_text;
            }
            else
            {
                QChar mapped = punctuations.value(ch);
                translated_text = !mapped.isNull() ? mapped : ch;
                sv_text = translated_text;

                static constexpr QStringView punct(u".!?…:;\"");
                static constexpr QStringView comma(u",");

                if (punct.contains(translated_text))
                {
                    cap_next = true;
                    is_punct = true;
                }
                else if (comma.contains(translated_text))
                {
                    is_punct = true;
                }
            }

            if (!is_punct && cap_next && !translated_text.isEmpty())
            {
                if (translated_text[0].isLower()) translated_text[0] = translated_text[0].toUpper();
                if (!sv_text.isEmpty() && sv_text[0].isLower()) sv_text[0] = sv_text[0].toUpper();
                cap_next = false;
            }

            QString uid = QString::number(token_counter++);
            out.cn += u"<a href='" % uid % u"'>" % source_text.toHtmlEscaped() % u"</a>";;
            out.sv += u"<a href='" % uid % u"'>" % sv_text.toHtmlEscaped() % u"</a>";;
            out.vn += u"<a href='" % uid % u"'>" % translated_text.toHtmlEscaped() % u"</a>";;

            i += 1;
            out.length_consumed += 1;

            progress.update(1);

            if (!translated_text.isEmpty() && should_append_space(input, i, ch) && !out.vn.endsWith(' '))
            {
                out.vn += u" ";
                out.sv += u" ";
            }
        }
    }
    return out;
}

struct PlainResult
{
    QString text;
    int length_consumed = 0;
};

PlainResult convert_recursive_plain(const QStringView& input, bool& cap_next, Progress& progress)
{
    PlainResult out;
    int i = 0;

    while (i < input.length())
    {
        QChar ch = input[i];

        if (ch == '\n')
        {
            out.text += u"\n";
            cap_next = true;
            i++;
            out.length_consumed++;

            progress.update(1);

            continue;
        }
        if (ch.isSpace())
        {
            out.text += u" ";
            i++;
            out.length_consumed++;

            progress.update(1);

            continue;
        }

        if (current_name_set_id != -1)
        {
            if (Match match = name_set_dictionary.find(input, i); match.length > 0 && match.priority == NAME)
            {
                QString trans = *match.translation;
                if (cap_next)
                {
                    cap_next = false;
                }

                out.text += trans;
                i += match.length;
                out.length_consumed += match.length;

                progress.update(match.length);

                if (should_append_space(input, i) && !out.text.endsWith(' '))
                {
                    out.text += u" ";
                }

                continue;
            }
        }

        auto [length, priority, rules, translation] = dictionary.find(input, i);

        if (length > 0 && priority == NAME)
        {
            QString trans = *translation;
            if (cap_next)
            {
                cap_next = false;
            }
            out.text += trans;
            i += length;
            out.length_consumed += length;

            if (should_append_space(input, i) && !out.text.endsWith(' '))
            {
                out.text += u" ";
            }

            progress.update(length);

            continue;
        }

        if (rules != nullptr)
        {
            if (auto rule_match = find_matching_rule(input, i, *rules))
            {
                const Rule* rule = rule_match->rule;
                int start_len = static_cast<int>(rule->original_start.length());

                bool phrase_overrides_rule = (length > 0 && priority == PHRASE &&
                                              length > start_len);

                if (!phrase_overrides_rule) {

                    int inner_start_idx = i + start_len;
                    int inner_len = rule_match->abs_start_of_end_token - inner_start_idx;
                    int end_len = static_cast<int>(rule->original_end.length());

                    progress.update(start_len);

                    QString t_start = rule->translation_start;
                    if (cap_next && !t_start.isEmpty())
                    {
                        if (t_start[0].isLower()) t_start[0] = t_start[0].toUpper();
                        cap_next = false;
                    }

                    auto [text, _] = convert_recursive_plain(input.mid(inner_start_idx, inner_len), cap_next, progress);

                    progress.update(end_len);

                    if (!t_start.isEmpty())
                    {
                        out.text += t_start + " ";
                    }

                    out.text += text;

                    if (!rule->translation_end.isEmpty())
                    {
                        if (!out.text.endsWith(' ')) out.text += u" ";
                        out.text += rule->translation_end;
                    }

                    i += start_len + inner_len + end_len;
                    out.length_consumed += start_len + inner_len + end_len;

                    if (should_append_space(input, i) && !out.text.endsWith(' '))
                    {
                        out.text += u" ";
                    }
                    continue;
                }
            }
        }

        if (length > 0 && priority == PHRASE)
        {
            if (int conflict_start = is_optimal_phrase(input, i, length); conflict_start != -1)
            {
                int max_allowed_len = conflict_start - i;

                length = 0;
                translation = nullptr;

                for (int try_len = max_allowed_len; try_len >= 1; --try_len)
                {
                    if (auto [_, exact_phrases] = dictionary.find_exact(input.mid(i, try_len).toString()); exact_phrases && !exact_phrases->isEmpty())
                    {
                        length = try_len;
                        translation = &exact_phrases->first();
                        break;
                    }
                }
            }

            if (length == 0)
            {
                goto process_single_char;
            }

            QString trans = *translation;
            if (cap_next)
            {
                if (!trans.isEmpty() && trans[0].isLower()) trans[0] = trans[0].toUpper();
                cap_next = false;
            }

            out.text += trans;
            i += length;
            out.length_consumed += length;

            if (should_append_space(input, i) && !out.text.endsWith(' '))
            {
                out.text += u" ";
            }

            progress.update(length);

            continue;
        }

        process_single_char:
        {
            QString translated_text;
            bool is_punct = false;

            if (sv_readings.contains(ch))
            {
                translated_text = sv_readings[ch];
            }
            else
            {
                QChar mapped = punctuations.value(ch);
                translated_text = !mapped.isNull() ? mapped : ch;
                static constexpr QStringView punct(u".!?…:;\"");
                static constexpr QStringView comma(u",");
                if (punct.contains(translated_text))
                {
                    cap_next = true;
                    is_punct = true;
                }
                else if (comma.contains(translated_text))
                {
                    is_punct = true;
                }
            }

            if (!is_punct && cap_next && !translated_text.isEmpty())
            {
                if (translated_text[0].isLower()) translated_text[0] = translated_text[0].toUpper();
                cap_next = false;
            }

            out.text += translated_text;
            i += 1;
            out.length_consumed += 1;

            if (!translated_text.isEmpty() && should_append_space(input, i, ch) && !out.text.endsWith(' '))
            {
                out.text += u" ";
            }

            progress.update(1);
        }
    }
    return out;
}

std::tuple<QString, QString, QString> convert(const QStringView& input,
                                              const std::function<void(int)>& progress_callback)
{
    QString cn_output;
    QString sv_output;
    QString vn_output;

    cn_output.append(R"(<style>a{text-decoration:none;color:white;font-family:"Noto Sans SC";font-size:18px}</style>)");
    sv_output.append(R"(<style>a{text-decoration:none;color:white;font-family:"Tahoma";font-size:16px}</style>)");
    vn_output.append(R"(<style>a{text-decoration:none;color:white;font-family:"Tahoma";font-size:16px}</style>)");

    int token_counter = 0;
    bool cap_next = true;

    Progress progress(progress_callback);

    const ConversionResult res = convert_recursive(input, 0, token_counter, cap_next, progress);

    cn_output.append(res.cn);
    sv_output.append(res.sv);
    vn_output.append(res.vn);

    return {cn_output, sv_output, vn_output};
}

QString convert_plain(const QStringView& input, const std::function<void(int)>& progress_callback)
{
    bool cap_next = true;
    Progress progress(progress_callback);
    auto [text, _] = convert_recursive_plain(input, cap_next, progress);
    return text.trimmed();
}
