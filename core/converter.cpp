#include "converter.h"
#include "structures.h"
#include "dict.h"
#include <optional>

#include "documents/aligned_document.h"

namespace
{
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
}

static QString get_sv(const QStringView& cn)
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
        const QChar next_char = input[current_end_idx];

        if (closers.contains(next_char))
        {
            return false;
        }

        QChar prev_char = current_char_source;

        if (prev_char.isNull() && current_end_idx > 0)
        {
            prev_char = input[current_end_idx - 1];
        }

        if (!prev_char.isNull())
        {
            auto is_ascii_alphanumeric = [](const QChar& c)
            {
                const ushort code = c.unicode();
                return (code >= '0' && code <= '9') ||
                    (code >= 'A' && code <= 'Z') ||
                    (code >= 'a' && code <= 'z');
            };

            if (is_ascii_alphanumeric(prev_char) && is_ascii_alphanumeric(next_char))
            {
                return false;
            }
        }
    }

    return true;
}

static int is_optimal_phrase(const QStringView& text, const int current_pos, const int current_len)
{
    const int threshold = std::max(current_len, 3);
    const int limit = std::min(static_cast<int>(text.length()), current_pos + threshold);

    for (int next_start = current_pos + 1; next_start < limit; ++next_start)
    {
        const auto sub = text.mid(next_start);
        int match_len = 0;
        int match_priority = 0;

        if (current_name_set_id != -1)
        {
            if (const auto [len, prio, _, trans] = name_set_dictionary.find(sub, 0); len > 0)
            {
                match_len = len;
                match_priority = prio;
            }
        }

        if (match_len == 0)
        {
            const auto [len, prio, _, trans] = dictionary.find(sub, 0);
            match_len = len;
            match_priority = prio;
        }

        if (match_len > 0)
        {
            if (const int overlap_end = next_start + match_len; overlap_end > current_pos + current_len)
            {
                if (match_priority == NAME || match_len > current_len)
                {
                    return next_start;
                }
            }
        }
    }

    return -1;
}

namespace
{
    struct RuleMatch
    {
        const Rule* rule;
        int abs_start_of_end_token;
        int total_end_pos;
    };
}

static std::optional<RuleMatch> find_matching_rule(const QStringView& text, const int current_pos,
                                                   const std::vector<Rule>& rules)
{
    const int limit = std::min(static_cast<int>(text.length()), current_pos + 50);
    const QStringView search_area = text.mid(current_pos, limit - current_pos);

    std::optional<RuleMatch> best_match = std::nullopt;

    for (const auto& rule : rules)
    {
        const int start_len = static_cast<int>(rule.original_start.length());
        const int rule_end_len = static_cast<int>(rule.original_end.length());

        if (search_area.length() <= start_len) continue;

        int search_offset = start_len;
        while (search_offset < search_area.length())
        {
            const int relative_end_idx = static_cast<int>(search_area.indexOf(rule.original_end, search_offset));
            if (relative_end_idx == -1) break;

            const int abs_start_of_end = current_pos + relative_end_idx;
            const int candidate_inner_start = current_pos + start_len;
            const int candidate_inner_len = abs_start_of_end - candidate_inner_start;

            bool is_safe = true;
            for (int scan = 0; scan < candidate_inner_len;)
            {
                auto [found_len, priority, sub_rules, _] = dictionary.find(
                    text.mid(candidate_inner_start + scan, candidate_inner_len - scan), 0);

                if (found_len > 0 && priority == PHRASE && candidate_inner_start + scan + found_len >
                    abs_start_of_end)
                {
                    is_safe = false;
                    break;
                }
                scan += found_len > 0 ? found_len : 1;
            }

            if (!is_safe)
            {
                search_offset = relative_end_idx + 1;
                continue;
            }

            const int total_end = abs_start_of_end + rule_end_len;

            if (!best_match.has_value())
            {
                best_match = RuleMatch{.rule = &rule, .abs_start_of_end_token = abs_start_of_end, .total_end_pos = total_end};
            }
            else
            {
                if (total_end > best_match->total_end_pos)
                {
                    best_match = RuleMatch{.rule = &rule, .abs_start_of_end_token = abs_start_of_end, .total_end_pos = total_end};
                }
                else if (total_end == best_match->total_end_pos)
                {
                    if (rule.original_end.length() > best_match->rule->original_end.length())
                    {
                        best_match = RuleMatch{.rule = &rule, .abs_start_of_end_token = abs_start_of_end, .total_end_pos = total_end};
                    }
                }
            }
            break;
        }
    }

    return best_match;
}

static void convert_recursive_aligned(const QStringView& input, int start_offset, uint32_t& token_counter,
                                      bool& cap_next,
                                      Progress& progress, AlignedDocument& doc)
{
    int i = 0;

    while (i < input.length())
    {
        QChar ch = input[i];

        if (ch == '\n')
        {
            doc.paragraphs.emplace_back();
            cap_next = true;
            i++;
            progress.update(1);
            continue;
        }
        if (ch.isSpace())
        {
            if (!doc.paragraphs.empty() && !doc.paragraphs.back().tokens.empty())
            {
                doc.paragraphs.back().tokens.back().has_trailing_space = true;
            }
            i++;
            progress.update(1);
            continue;
        }

        if (current_name_set_id != -1)
        {
            if (Match match = name_set_dictionary.find(input, i); match.length > 0 && match.priority == NAME)
            {
                QString sv = get_sv(input.sliced(i, match.length));
                if (cap_next)
                {
                    sv[0] = sv[0].toUpper();
                    cap_next = false;
                }

                Token tok;
                tok.id = ++token_counter;
                tok.cn = input.sliced(i, match.length).toString();
                tok.sv = std::move(sv);
                tok.vn = *match.translation;

                i += match.length;
                progress.update(match.length);

                doc.paragraphs.back().tokens.push_back(std::move(tok));
                continue;
            }
        }

        auto [length, priority, rules, translation] = dictionary.find(input, i);

        if (length > 0 && priority == NAME)
        {
            QString sv = get_sv(input.sliced(i, length));
            if (cap_next)
            {
                sv[0] = sv[0].toUpper();
                cap_next = false;
            }

            Token tok;
            tok.id = ++token_counter;
            tok.cn = input.sliced(i, length).toString();
            tok.sv = std::move(sv);
            tok.vn = *translation;

            i += length;
            progress.update(length);

            doc.paragraphs.back().tokens.push_back(std::move(tok));
            continue;
        }

        if (rules != nullptr)
        {
            if (auto rule_match = find_matching_rule(input, i, *rules))
            {
                const Rule* rule = rule_match->rule;
                int rule_start_len = static_cast<int>(rule->original_start.length());

                bool phrase_overrides_rule = length > 0 && priority == PHRASE &&
                    length > rule_start_len;

                if (!phrase_overrides_rule)
                {
                    int inner_start_idx = i + rule_start_len;
                    int inner_len = rule_match->abs_start_of_end_token - inner_start_idx;
                    int rule_end_len = static_cast<int>(rule->original_end.length());

                    progress.update(rule_start_len);

                    uint32_t rule_uid = ++token_counter;

                    QString t_start = rule->translation_start;
                    if (cap_next && !t_start.isEmpty())
                    {
                        if (t_start[0].isLower()) t_start[0] = t_start[0].toUpper();
                        cap_next = false;
                    }

                    Token start_tok;
                    start_tok.id = rule_uid;
                    start_tok.cn = rule->original_start;
                    start_tok.sv = get_sv(rule->original_start);
                    start_tok.vn = t_start;
                    start_tok.rule = rule;
                    doc.paragraphs.back().tokens.push_back(std::move(start_tok));

                    convert_recursive_aligned(input.sliced(inner_start_idx, inner_len),
                                              start_offset + inner_start_idx,
                                              token_counter,
                                              cap_next, progress, doc);

                    progress.update(rule_end_len);

                    Token end_tok;
                    end_tok.id = rule_uid;
                    end_tok.cn = rule->original_end;
                    end_tok.sv = get_sv(rule->original_end);
                    end_tok.vn = rule->translation_end;
                    end_tok.rule = rule;

                    i += rule_start_len + inner_len + rule_end_len;
                    doc.paragraphs.back().tokens.push_back(std::move(end_tok));

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
                    const auto try_string = input.sliced(i, try_len);
                    if (current_name_set_id != -1)
                    {
                        if (auto [set_name, _] = name_set_dictionary.find_exact(try_string); set_name)
                        {
                            length = try_len;
                            translation = set_name;
                            break;
                        }
                    }
                    auto [exact_name, exact_phrases] = dictionary.find_exact(try_string);
                    if (exact_name)
                    {
                        length = try_len;
                        translation = exact_name;
                        break;
                    }
                    if (exact_phrases && !exact_phrases->isEmpty())
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

            QString sv = get_sv(input.sliced(i, length));
            QString trans = *translation;

            if (cap_next)
            {
                if (!trans.isEmpty() && trans[0].isLower()) trans[0] = trans[0].toUpper();
                sv[0] = sv[0].toUpper();
                cap_next = false;
            }

            Token tok;
            tok.id = ++token_counter;
            tok.cn = input.sliced(i, length).toString();
            tok.sv = std::move(sv);
            tok.vn = std::move(trans);

            i += length;
            progress.update(length);

            doc.paragraphs.back().tokens.push_back(std::move(tok));
            continue;
        }

    process_single_char:
        {
            QString source_text = input[i];
            QString translated_text;
            QString sv_text;
            bool is_punctuator = false;

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

                static constexpr QStringView punctuators(u".!?…:;\"");
                static constexpr QStringView comma(u",");

                if (punctuators.contains(translated_text))
                {
                    cap_next = true;
                    is_punctuator = true;
                }
                else if (comma.contains(translated_text))
                {
                    is_punctuator = true;
                }
            }

            if (!is_punctuator && cap_next && !translated_text.isEmpty())
            {
                if (translated_text[0].isLower()) translated_text[0] = translated_text[0].toUpper();
                if (!sv_text.isEmpty() && sv_text[0].isLower()) sv_text[0] = sv_text[0].toUpper();
                cap_next = false;
            }

            Token tok;
            tok.id = ++token_counter;
            tok.cn = std::move(source_text);
            tok.sv = std::move(sv_text);
            tok.vn = std::move(translated_text);

            i += 1;
            progress.update(1);

            doc.paragraphs.back().tokens.push_back(std::move(tok));
        }
    }
}

std::shared_ptr<AlignedDocument> convert(const QStringView& input,
                                         const std::function<void(int)>& progress_callback)
{
    auto doc = std::make_shared<AlignedDocument>();
    doc->paragraphs.emplace_back();

    uint32_t token_counter = 0;
    bool cap_next = true;

    Progress progress(progress_callback);

    convert_recursive_aligned(input, 0, token_counter, cap_next, progress, *doc);

    if (doc->paragraphs.size() > 1 && doc->paragraphs.back().empty())
    {
        doc->paragraphs.pop_back();
    }

    doc->build_index();
    return doc;
}

namespace
{
    struct PlainResult
    {
        QString text;
        int length_consumed = 0;
    };
}

static PlainResult convert_recursive_plain(const QStringView& input, bool& cap_next, Progress& progress)
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

                auto [text, _] = convert_recursive_plain(input.sliced(inner_start_idx, inner_len), cap_next,
                                                         progress);

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

        if (length > 0 && priority == PHRASE)
        {
            if (int conflict_start = is_optimal_phrase(input, i, length); conflict_start != -1)
            {
                int max_allowed_len = conflict_start - i;

                length = 0;
                translation = nullptr;

                for (int try_len = max_allowed_len; try_len >= 1; --try_len)
                {
                    const auto try_string = input.sliced(i, try_len);
                    if (current_name_set_id != -1)
                    {
                        if (auto [set_name, _] = name_set_dictionary.find_exact(try_string); set_name)
                        {
                            length = try_len;
                            translation = set_name;
                            break;
                        }
                    }
                    auto [exact_name, exact_phrases] = dictionary.find_exact(try_string);
                    if (exact_name)
                    {
                        length = try_len;
                        translation = exact_name;
                        break;
                    }
                    if (exact_phrases && !exact_phrases->isEmpty())
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
            bool is_punctuator = false;

            if (sv_readings.contains(ch))
            {
                translated_text = sv_readings[ch];
            }
            else
            {
                QChar mapped = punctuations.value(ch);
                translated_text = !mapped.isNull() ? mapped : ch;
                static constexpr QStringView punctuators(u".!?…:;\"");
                static constexpr QStringView comma(u",");
                if (punctuators.contains(translated_text))
                {
                    cap_next = true;
                    is_punctuator = true;
                }
                else if (comma.contains(translated_text))
                {
                    is_punctuator = true;
                }
            }

            if (!is_punctuator && cap_next && !translated_text.isEmpty())
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

QString convert_plain(const QStringView& input, const std::function<void(int)>& progress_callback)
{
    bool cap_next = true;
    Progress progress(progress_callback);
    auto [text, _] = convert_recursive_plain(input, cap_next, progress);
    return text.trimmed();
}
