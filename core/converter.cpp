#include "converter.h"
#include "structures.h"
#include "dict.h"
#include "unicode_utils.h"
#include <optional>
#include <mutex>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <SimpleConverter.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

namespace hanvi {

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

static std::u16string get_sv(const std::u16string_view& cn)
{
    std::u16string sv_reading;
    size_t idx = 0;
    while (idx < cn.length())
    {
        const char16_t ch = cn[idx];
        if (is_high_surrogate(ch) && idx + 1 < cn.length() && is_low_surrogate(cn[idx + 1]))
        {
            sv_reading += cn.substr(idx, 2);
            idx += 2;
            if (idx < cn.length() && !(is_high_surrogate(cn[idx]) && idx + 1 < cn.length() && is_low_surrogate(cn[idx + 1])))
            {
                sv_reading.push_back(u' ');
            }
            continue;
        }

        if (auto it = sv_readings.find(ch); it != sv_readings.end())
        {
            sv_reading += it->second;
        }
        else
        {
            if (auto pit = punctuations.find(ch); pit != punctuations.end())
            {
                sv_reading.push_back(pit->second);
            }
            else sv_reading.push_back(ch);
        }
        sv_reading.push_back(u' ');
        idx++;
    }
    if (!sv_reading.empty() && sv_reading.back() == u' ')
    {
        sv_reading.pop_back();
    }

    return sv_reading;
}

static char16_t normalize_char(const char16_t c) noexcept
{
    if (auto it = punctuations.find(c); it != punctuations.end())
    {
        return it->second;
    }
    const uint16_t code = c;
    if (code >= 0xFF21 && code <= 0xFF3A) // Ａ-Ｚ
    {
        return static_cast<char16_t>(code - 0xFEE0);
    }
    if (code >= 0xFF41 && code <= 0xFF5A) // ａ-ｚ
    {
        return static_cast<char16_t>(code - 0xFEE0);
    }
    if (code >= 0xFF10 && code <= 0xFF19) // ０-９
    {
        return static_cast<char16_t>(code - 0xFEE0);
    }
    return c;
}

static bool should_append_space(const std::u16string_view& input, const size_t current_end_idx,
                                const char16_t current_char_source = 0,
                                const bool in_quote = false,
                                const bool in_single_quote = false)
{
    static constexpr std::u16string_view openers(u"“‘([<{");
    if (current_char_source != 0)
    {
        if (openers.find(current_char_source) != std::u16string_view::npos)
        {
            return false;
        }
        if (current_char_source == u'"' && in_quote)
        {
            return false;
        }
        if (current_char_source == u'\'' && in_single_quote)
        {
            return false;
        }
    }

    static constexpr std::u16string_view closers(u".,;:!?)]}>”’…");

    if (current_end_idx < input.length())
    {
        const char16_t next_char = input[current_end_idx];

        if (next_char == u'"')
        {
            if (in_quote)
            {
                return false;
            }
        }
        else if (next_char == u'\'')
        {
            if (in_single_quote)
            {
                return false;
            }
        }
        else if (closers.find(next_char) != std::u16string_view::npos)
        {
            return false;
        }

        char16_t prev_char = current_char_source;

        if (prev_char == 0 && current_end_idx > 0)
        {
            prev_char = input[current_end_idx - 1];
        }

        if (prev_char != 0)
        {
            if (is_low_surrogate(prev_char) && is_high_surrogate(next_char))
            {
                return false;
            }

            if (is_latin_or_digit(prev_char) && is_latin_or_digit(next_char))
            {
                return false;
            }

            if ((prev_char == u'.' || prev_char == u',') && is_digit(next_char))
            {
                if (current_end_idx >= 2 && is_digit(input[current_end_idx - 2]))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

static int is_optimal_phrase(const std::u16string_view& text, const int current_pos, const int current_len)
{
    const int threshold = std::max(current_len, 3);
    const int limit = current_pos + current_len;

    for (int next_start = current_pos + 1; next_start < limit; ++next_start)
    {
        if (current_name_set_id != -1)
        {
            if (const Match match = name_set_dictionary.find(text, next_start); match.length > 0)
            {
                return next_start;
            }
        }

        if (const Match match = dictionary.find(text, next_start); match.priority == NAME || match.length > threshold)
        {
            return next_start;
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

static std::optional<RuleMatch> find_matching_rule(const std::u16string_view& text, const int current_pos,
                                                   const std::vector<Rule>& rules)
{
    static constexpr std::u16string_view stoppers(u"，。：；！？“”’.,，;:!?)]}>\"'");
    int limit = std::min(static_cast<int>(text.length()), current_pos + 25);

    for (int i = current_pos; i < limit; ++i)
    {
        if (const char16_t ch = text[i]; stoppers.find(ch) != std::u16string_view::npos)
        {
            limit = i;
            break;
        }
    }

    const std::u16string_view search_area = text.substr(current_pos, limit - current_pos);
    std::optional<RuleMatch> best_match = std::nullopt;

    for (const auto& rule : rules)
    {
        const int start_len = static_cast<int>(rule.original_start.length());
        if (static_cast<int>(search_area.length()) <= start_len) continue;

        int search_offset = start_len;

        while (true)
        {
            const size_t found = search_area.find(rule.original_end, search_offset);
            if (found == std::u16string_view::npos) break;

            const int relative_end_idx = static_cast<int>(found);
            const int abs_start_of_end = current_pos + relative_end_idx;
            const int rule_end_len = static_cast<int>(rule.original_end.length());

            bool is_safe = true;
            const int lookback_limit = std::max(current_pos + start_len, abs_start_of_end - 6);

            for (int k = abs_start_of_end; k >= lookback_limit; --k)
            {
                auto check_overlap = [&](const auto& dict, const Priority target_prio)
                {
                    const Match m = dict.find(text, k);
                    if (m.length > 0 && m.priority == target_prio)
                    {
                        if (k + m.length > abs_start_of_end)
                        {
                            return true;
                        }
                    }
                    return false;
                };

                if (current_name_set_id != -1)
                {
                    if (check_overlap(name_set_dictionary, NAME))
                    {
                        is_safe = false;
                        break;
                    }
                }
                if (check_overlap(dictionary, NAME))
                {
                    is_safe = false;
                    break;
                }
            }

            if (!is_safe)
            {
                search_offset = relative_end_idx + 1;
                continue;
            }

            const int total_end = abs_start_of_end + rule_end_len;

            if (!best_match.has_value())
            {
                best_match = RuleMatch{
                    .rule = &rule, .abs_start_of_end_token = abs_start_of_end, .total_end_pos = total_end
                };
            }
            else
            {
                if (total_end > best_match->total_end_pos)
                {
                    best_match = RuleMatch{
                        .rule = &rule, .abs_start_of_end_token = abs_start_of_end, .total_end_pos = total_end
                    };
                }
                else if (total_end == best_match->total_end_pos)
                {
                    if (rule.original_end.length() > best_match->rule->original_end.length())
                    {
                        best_match = RuleMatch{
                            .rule = &rule, .abs_start_of_end_token = abs_start_of_end, .total_end_pos = total_end
                        };
                    }
                }
            }
            break;
        }
    }

    return best_match;
}

static void convert_recursive_aligned(const std::u16string_view& input, int start_offset, uint32_t& token_counter,
                                      bool& cap_next,
                                      Progress& progress, AlignedDocument& doc)
{
    size_t i = 0;

    while (i < input.length())
    {
        char16_t ch = input[i];

        if (ch == u'\n')
        {
            doc.paragraphs.emplace_back();
            cap_next = true;
            i++;
            progress.update(1);
            continue;
        }
        if (ch == u' ' || ch == u'\t' || ch == u'\r' || ch == u'\v' || ch == u'\f' || ch == 0x3000)
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
            if (Match match = name_set_dictionary.find(input, static_cast<int>(i)); match.length > 0 && match.priority == NAME)
            {
                std::u16string sv = get_sv(input.substr(i, match.length));
                if (cap_next && !sv.empty())
                {
                    sv[0] = to_upper_char16(sv[0]);
                    cap_next = false;
                }

                Token tok;
                tok.id = ++token_counter;
                tok.cn = std::u16string(input.substr(i, match.length));
                tok.sv = std::move(sv);
                tok.vn = std::u16string(match.translation);

                i += match.length;
                progress.update(match.length);

                doc.paragraphs.back().tokens.push_back(std::move(tok));
                continue;
            }
        }

        auto [length, priority, rules, translation] = dictionary.find(input, static_cast<int>(i));

        if (length > 0 && priority == NAME)
        {
            std::u16string sv = get_sv(input.substr(i, length));
            if (cap_next && !sv.empty())
            {
                sv[0] = to_upper_char16(sv[0]);
                cap_next = false;
            }

            Token tok;
            tok.id = ++token_counter;
            tok.cn = std::u16string(input.substr(i, length));
            tok.sv = std::move(sv);
            tok.vn = std::u16string(translation);

            i += length;
            progress.update(length);

            doc.paragraphs.back().tokens.push_back(std::move(tok));
            continue;
        }

        if (rules != nullptr)
        {
            if (auto rule_match = find_matching_rule(input, static_cast<int>(i), *rules))
            {
                const Rule* rule = rule_match->rule;
                int rule_start_len = static_cast<int>(rule->original_start.length());

                bool phrase_overrides_rule = length > 0 && priority == PHRASE &&
                    length > rule_start_len;

                if (!phrase_overrides_rule)
                {
                    int inner_start_idx = static_cast<int>(i) + rule_start_len;
                    int inner_len = rule_match->abs_start_of_end_token - inner_start_idx;
                    int rule_end_len = static_cast<int>(rule->original_end.length());

                    progress.update(rule_start_len);

                    uint32_t rule_uid = ++token_counter;

                    std::u16string t_start = rule->translation_start;
                    if (cap_next && !t_start.empty())
                    {
                        if (is_lower_char16(t_start[0])) t_start[0] = to_upper_char16(t_start[0]);
                        cap_next = false;
                    }

                    Token start_tok;
                    start_tok.id = rule_uid;
                    start_tok.cn = rule->original_start;
                    start_tok.sv = get_sv(rule->original_start);
                    start_tok.vn = t_start;
                    start_tok.rule = rule;
                    doc.paragraphs.back().tokens.push_back(std::move(start_tok));

                    convert_recursive_aligned(input.substr(inner_start_idx, inner_len),
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
            if (int conflict_start = is_optimal_phrase(input, static_cast<int>(i), length); conflict_start != -1)
            {
                int max_allowed_len = conflict_start - static_cast<int>(i);

                length = 0;
                translation = {};

                for (int try_len = max_allowed_len; try_len >= 1; --try_len)
                {
                    const auto try_string = input.substr(i, try_len);
                    if (current_name_set_id != -1)
                    {
                        if (const auto res_set = name_set_dictionary.find_exact(try_string); !res_set.name.isNull())
                        {
                            length = try_len;
                            translation = res_set.name;
                            break;
                        }
                    }
                    const auto res = dictionary.find_exact(try_string);
                    if (!res.name.isNull())
                    {
                        length = try_len;
                        translation = res.name;
                        break;
                    }
                    if (const auto phrase = res.phrase(); !phrase.isNull())
                    {
                        length = try_len;
                        translation = phrase;
                        break;
                    }
                }
            }

            if (length == 0)
            {
                goto process_single_char;
            }

            std::u16string sv = get_sv(input.substr(i, length));
            std::u16string trans(translation);

            if (cap_next)
            {
                if (!trans.empty() && is_lower_char16(trans[0])) trans[0] = to_upper_char16(trans[0]);
                if (!sv.empty()) sv[0] = to_upper_char16(sv[0]);
                cap_next = false;
            }

            Token tok;
            tok.id = ++token_counter;
            tok.cn = std::u16string(input.substr(i, length));
            tok.sv = std::move(sv);
            tok.vn = std::move(trans);

            i += length;
            progress.update(length);

            doc.paragraphs.back().tokens.push_back(std::move(tok));
            continue;
        }

    process_single_char:
        {
            if (is_latin_or_digit(ch))
            {
                size_t start_idx = i;
                size_t end_idx = i + 1;
                while (end_idx < input.length())
                {
                    if (const char16_t next = input[end_idx]; is_latin_or_digit(next))
                    {
                        end_idx++;
                    }
                    else if ((next == u'.' || next == u',') &&
                        end_idx + 1 < input.length() &&
                        is_digit(input[end_idx - 1]) &&
                        is_digit(input[end_idx + 1]))
                    {
                        end_idx++;
                    }
                    else
                    {
                        break;
                    }
                }

                const size_t len = end_idx - start_idx;
                std::u16string word(input.substr(start_idx, len));
                std::u16string sv_word;
                sv_word.reserve(len);
                for (size_t k = start_idx; k < end_idx; ++k)
                {
                    sv_word.push_back(normalize_char(input[k]));
                }
                std::u16string vn_word = sv_word;

                if (cap_next && !vn_word.empty())
                {
                    if (is_lower_char16(vn_word[0])) vn_word[0] = to_upper_char16(vn_word[0]);
                    if (!sv_word.empty() && is_lower_char16(sv_word[0])) sv_word[0] = to_upper_char16(sv_word[0]);
                    cap_next = false;
                }

                Token tok;
                tok.id = ++token_counter;
                tok.cn = std::move(word);
                tok.sv = std::move(sv_word);
                tok.vn = std::move(vn_word);

                i += len;
                progress.update(static_cast<int>(len));

                doc.paragraphs.back().tokens.push_back(std::move(tok));
                continue;
            }

            const bool is_surrogate = is_high_surrogate(ch) && (i + 1 < input.length()) && is_low_surrogate(input[i + 1]);
            const size_t char_len = is_surrogate ? 2 : 1;

            std::u16string source_text(input.substr(i, char_len));
            std::u16string translated_text;
            std::u16string sv_text;
            bool is_punctuator = false;

            if (is_surrogate)
            {
                translated_text = source_text;
                sv_text = source_text;
            }
            else if (auto it = sv_readings.find(ch); it != sv_readings.end())
            {
                translated_text = it->second;
                sv_text = translated_text;
            }
            else
            {
                if (auto pit = punctuations.find(ch); pit != punctuations.end())
                {
                    translated_text = pit->second;
                }
                else
                {
                    translated_text = ch;
                }
                sv_text = translated_text;

                static constexpr std::u16string_view punctuators(u".!?…:;\"");
                static constexpr std::u16string_view comma(u",");

                if (punctuators.find(translated_text) != std::u16string_view::npos)
                {
                    cap_next = true;
                    is_punctuator = true;
                }
                else if (comma.find(translated_text) != std::u16string_view::npos)
                {
                    is_punctuator = true;
                }
            }

            if (!is_punctuator && cap_next && !translated_text.empty() && !is_surrogate)
            {
                if (is_lower_char16(translated_text[0])) translated_text[0] = to_upper_char16(translated_text[0]);
                if (!sv_text.empty() && is_lower_char16(sv_text[0])) sv_text[0] = to_upper_char16(sv_text[0]);
                cap_next = false;
            }

            Token tok;
            tok.id = ++token_counter;
            tok.cn = std::move(source_text);
            tok.sv = std::move(sv_text);
            tok.vn = std::move(translated_text);

            i += char_len;
            progress.update(static_cast<int>(char_len));

            doc.paragraphs.back().tokens.push_back(std::move(tok));
        }
    }
}

std::shared_ptr<AlignedDocument> convert(std::u16string_view input,
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
        std::u16string text;
        int length_consumed = 0;
    };
}

static PlainResult convert_recursive_plain(const std::u16string_view& input, bool& cap_next, Progress& progress, bool& in_quote, bool& in_single_quote)
{
    PlainResult out;
    size_t i = 0;

    while (i < input.length())
    {
        char16_t ch = input[i];

        if (ch == u'\n')
        {
            out.text += u"\n";
            cap_next = true;
            in_quote = false;
            in_single_quote = false;
            i++;
            out.length_consumed++;

            progress.update(1);
            continue;
        }
        if (ch == u' ' || ch == u'\t' || ch == u'\r' || ch == u'\v' || ch == u'\f' || ch == 0x3000)
        {
            out.text += u" ";
            i++;
            out.length_consumed++;

            progress.update(1);
            continue;
        }

        if (current_name_set_id != -1)
        {
            if (Match match = name_set_dictionary.find(input, static_cast<int>(i)); match.length > 0 && match.priority == NAME)
            {
                std::u16string trans(match.translation);
                if (cap_next)
                {
                    cap_next = false;
                }

                out.text += trans;
                i += match.length;
                out.length_consumed += match.length;

                progress.update(match.length);

                if (should_append_space(input, i, 0, in_quote, in_single_quote) && (out.text.empty() || out.text.back() != u' '))
                {
                    out.text += u" ";
                }

                continue;
            }
        }

        auto [length, priority, rules, translation] = dictionary.find(input, static_cast<int>(i));

        if (length > 0 && priority == NAME)
        {
            std::u16string trans(translation);
            if (cap_next)
            {
                cap_next = false;
            }
            out.text += trans;
            i += length;
            out.length_consumed += length;

            if (should_append_space(input, i, 0, in_quote, in_single_quote) && (out.text.empty() || out.text.back() != u' '))
            {
                out.text += u" ";
            }

            progress.update(length);
            continue;
        }

        if (rules != nullptr)
        {
            if (auto rule_match = find_matching_rule(input, static_cast<int>(i), *rules))
            {
                const Rule* rule = rule_match->rule;
                int start_len = static_cast<int>(rule->original_start.length());

                bool phrase_overrides_rule = (length > 0 && priority == PHRASE &&
                    length > start_len);

                if (!phrase_overrides_rule)
                {
                    int inner_start_idx = static_cast<int>(i) + start_len;
                    int inner_len = rule_match->abs_start_of_end_token - inner_start_idx;
                    int end_len = static_cast<int>(rule->original_end.length());

                    progress.update(start_len);

                    std::u16string t_start = rule->translation_start;
                    if (cap_next && !t_start.empty())
                    {
                        if (is_lower_char16(t_start[0])) t_start[0] = to_upper_char16(t_start[0]);
                        cap_next = false;
                    }

                    auto [text, _] = convert_recursive_plain(input.substr(inner_start_idx, inner_len), cap_next,
                                                             progress, in_quote, in_single_quote);

                    progress.update(end_len);

                    if (!t_start.empty())
                    {
                        out.text += t_start + u" ";
                    }

                    out.text += text;

                    if (!rule->translation_end.empty())
                    {
                        if (!out.text.empty() && out.text.back() != u' ') out.text += u" ";
                        out.text += rule->translation_end;
                    }

                    i += start_len + inner_len + end_len;
                    out.length_consumed += start_len + inner_len + end_len;

                    if (should_append_space(input, i, 0, in_quote, in_single_quote) && (out.text.empty() || out.text.back() != u' '))
                    {
                        out.text += u" ";
                    }
                    continue;
                }
            }
        }

        if (length > 0 && priority == PHRASE)
        {
            if (int conflict_start = is_optimal_phrase(input, static_cast<int>(i), length); conflict_start != -1)
            {
                int max_allowed_len = conflict_start - static_cast<int>(i);

                length = 0;
                translation = {};

                for (int try_len = max_allowed_len; try_len >= 1; --try_len)
                {
                    const auto try_string = input.substr(i, try_len);
                    if (current_name_set_id != -1)
                    {
                        if (const auto res_set = name_set_dictionary.find_exact(try_string); !res_set.name.isNull())
                        {
                            length = try_len;
                            translation = res_set.name;
                            break;
                        }
                    }
                    const auto res = dictionary.find_exact(try_string);
                    if (!res.name.isNull())
                    {
                        length = try_len;
                        translation = res.name;
                        break;
                    }
                    if (const auto phrase = res.phrase(); !phrase.isNull())
                    {
                        length = try_len;
                        translation = phrase;
                        break;
                    }
                }
            }

            if (length == 0)
            {
                goto process_single_char;
            }

            std::u16string trans(translation);
            if (cap_next)
            {
                if (!trans.empty() && is_lower_char16(trans[0])) trans[0] = to_upper_char16(trans[0]);
                cap_next = false;
            }

            out.text += trans;
            i += length;
            out.length_consumed += length;

            if (should_append_space(input, i, 0, in_quote, in_single_quote) && (out.text.empty() || out.text.back() != u' '))
            {
                out.text += u" ";
            }

            progress.update(length);
            continue;
        }

    process_single_char:
        {
            if (is_latin_or_digit(ch))
            {
                size_t start_idx = i;
                size_t end_idx = i + 1;
                while (end_idx < input.length())
                {
                    if (const char16_t next = input[end_idx]; is_latin_or_digit(next))
                    {
                        end_idx++;
                    }
                    else if ((next == u'.' || next == u',') &&
                        end_idx + 1 < input.length() &&
                        is_digit(input[end_idx - 1]) &&
                        is_digit(input[end_idx + 1]))
                    {
                        end_idx++;
                    }
                    else
                    {
                        break;
                    }
                }

                const size_t len = end_idx - start_idx;
                std::u16string word;
                word.reserve(len);
                for (size_t k = start_idx; k < end_idx; ++k)
                {
                    word.push_back(normalize_char(input[k]));
                }

                if (cap_next && !word.empty())
                {
                    if (is_lower_char16(word[0])) word[0] = to_upper_char16(word[0]);
                    cap_next = false;
                }

                out.text += word;
                i += len;
                out.length_consumed += static_cast<int>(len);

                if (should_append_space(input, i, 0, in_quote, in_single_quote) && (out.text.empty() || out.text.back() != u' '))
                {
                    out.text += u" ";
                }

                progress.update(static_cast<int>(len));
                continue;
            }

            const bool is_surrogate = is_high_surrogate(ch) && (i + 1 < input.length()) && is_low_surrogate(input[i + 1]);
            const size_t char_len = is_surrogate ? 2 : 1;

            std::u16string translated_text;
            bool is_punctuator = false;

            if (is_surrogate)
            {
                translated_text = input.substr(i, 2);
            }
            else if (auto it = sv_readings.find(ch); it != sv_readings.end())
            {
                translated_text = it->second;
            }
            else
            {
                if (auto pit = punctuations.find(ch); pit != punctuations.end())
                {
                    translated_text = pit->second;
                }
                else
                {
                    translated_text = ch;
                }
                static constexpr std::u16string_view punctuators(u".!?…:;\"");
                static constexpr std::u16string_view comma(u",");
                if (punctuators.find(translated_text) != std::u16string_view::npos)
                {
                    cap_next = true;
                    is_punctuator = true;
                }
                else if (comma.find(translated_text) != std::u16string_view::npos)
                {
                    is_punctuator = true;
                }
            }

            if (!is_punctuator && cap_next && !translated_text.empty() && !is_surrogate)
            {
                if (is_lower_char16(translated_text[0])) translated_text[0] = to_upper_char16(translated_text[0]);
                cap_next = false;
            }

            if ((translated_text == u"\"" && in_quote) || (translated_text == u"'" && in_single_quote))
            {
                if (!out.text.empty() && out.text.back() == u' ')
                {
                    out.text.pop_back();
                }
            }

            out.text += translated_text;
            i += char_len;
            out.length_consumed += static_cast<int>(char_len);

            const char16_t last_char = is_surrogate ? input[i - 1] : ch;

            if (translated_text == u"\"")
            {
                if (!in_quote)
                {
                    in_quote = true;
                }
                else
                {
                    in_quote = false;
                    if (should_append_space(input, i, last_char, in_quote, in_single_quote) && (out.text.empty() || out.text.back() != u' '))
                    {
                        out.text += u" ";
                    }
                }
            }
            else if (translated_text == u"'")
            {
                if (!in_single_quote)
                {
                    in_single_quote = true;
                }
                else
                {
                    in_single_quote = false;
                    if (should_append_space(input, i, last_char, in_quote, in_single_quote) && (out.text.empty() || out.text.back() != u' '))
                    {
                        out.text += u" ";
                    }
                }
            }
            else if (!translated_text.empty() && should_append_space(input, i, last_char, in_quote, in_single_quote) && (out.text.empty() || out.text.back() != u' '))
            {
                out.text += u" ";
            }

            progress.update(static_cast<int>(char_len));
        }
    }
    return out;
}

std::u16string convert_plain(std::u16string_view input, const std::function<void(int)>& progress_callback)
{
    bool cap_next = true;
    bool in_quote = false;
    bool in_single_quote = false;
    Progress progress(progress_callback);
    auto [text, _] = convert_recursive_plain(input, cap_next, progress, in_quote, in_single_quote);

    // trim whitespace
    size_t start = 0;
    while (start < text.size() && (text[start] == u' ' || text[start] == u'\t' || text[start] == u'\r' || text[start] == u'\n'))
    {
        start++;
    }
    size_t end = text.size();
    while (end > start && (text[end - 1] == u' ' || text[end - 1] == u'\t' || text[end - 1] == u'\r' || text[end - 1] == u'\n'))
    {
        end--;
    }
    return text.substr(start, end - start);
}

static std::string get_opencc_config_path()
{
    if (std::filesystem::exists("opencc/t2s.json"))
    {
        return std::filesystem::absolute("opencc/t2s.json").string();
    }
    if (std::filesystem::exists("t2s.json"))
    {
        return std::filesystem::absolute("t2s.json").string();
    }
#ifdef _WIN32
    wchar_t exe_path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) > 0)
    {
        std::filesystem::path p(exe_path);
        auto candidate = p.parent_path() / "opencc" / "t2s.json";
        if (std::filesystem::exists(candidate))
        {
            return candidate.string();
        }
        candidate = p.parent_path() / "t2s.json";
        if (std::filesystem::exists(candidate))
        {
            return candidate.string();
        }
    }
#endif
    return {};
}

static std::mutex g_opencc_mutex;
static std::string g_opencc_custom_config_path;
static std::unique_ptr<opencc::SimpleConverter> g_opencc_converter;

void init_opencc(const std::string& config_path)
{
    std::lock_guard<std::mutex> lock(g_opencc_mutex);
    g_opencc_custom_config_path = config_path;
    try
    {
        g_opencc_converter = std::make_unique<opencc::SimpleConverter>(config_path);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to initialize OpenCC with path " << config_path << ": " << e.what() << std::endl;
        g_opencc_converter.reset();
    }
}

std::u16string to_simplified_chinese(std::u16string_view input)
{
    if (input.empty()) return {};

    std::lock_guard<std::mutex> lock(g_opencc_mutex);
    if (!g_opencc_converter)
    {
        std::string config_path = g_opencc_custom_config_path;
        if (config_path.empty())
        {
            config_path = get_opencc_config_path();
        }
        if (!config_path.empty())
        {
            try
            {
                g_opencc_converter = std::make_unique<opencc::SimpleConverter>(config_path);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to initialize OpenCC: " << e.what() << std::endl;
                g_opencc_converter.reset();
            }
        }
        else
        {
            std::cerr << "OpenCC t2s.json not found in application directory or search paths." << std::endl;
        }
    }

    if (!g_opencc_converter)
    {
        return std::u16string(input);
    }

    try
    {
        std::string utf8_in = to_utf8(input);
        std::string utf8_out = g_opencc_converter->Convert(utf8_in);
        return to_utf16(utf8_out);
    }
    catch (const std::exception& e)
    {
        std::cerr << "OpenCC conversion error: " << e.what() << std::endl;
        return std::u16string(input);
    }
}

std::u16string normalize_text(std::u16string_view input)
{
    if (input.empty()) return {};

    std::u16string text = to_simplified_chinese(input);

    for (size_t i = 0; i < text.size(); ++i)
    {
        const char16_t c = text[i];
        if (is_high_surrogate(c) && i + 1 < text.size() && is_low_surrogate(text[i + 1]))
        {
            ++i;
            continue;
        }

        if (auto it = punctuations.find(c); it != punctuations.end())
        {
            text[i] = it->second;
            continue;
        }

        const uint16_t code = c;
        if (code >= 0xFF21 && code <= 0xFF3A) // Ａ-Ｚ
        {
            text[i] = static_cast<char16_t>(code - 0xFEE0);
        }
        else if (code >= 0xFF41 && code <= 0xFF5A) // ａ-ｚ
        {
            text[i] = static_cast<char16_t>(code - 0xFEE0);
        }
        else if (code >= 0xFF10 && code <= 0xFF19) // ０-９
        {
            text[i] = static_cast<char16_t>(code - 0xFEE0);
        }
        else if (code == 0x3000) // fullwidth space
        {
            text[i] = u' ';
        }
    }

    return text;
}

std::string convert_plain_utf8(std::string_view input, const std::function<void(int)>& progress_callback)
{
    std::u16string u16_in = to_utf16(input);
    std::u16string u16_out = convert_plain(u16_in, progress_callback);
    return to_utf8(u16_out);
}

std::string to_simplified_chinese_utf8(std::string_view input)
{
    std::u16string u16_in = to_utf16(input);
    std::u16string u16_out = to_simplified_chinese(u16_in);
    return to_utf8(u16_out);
}

std::string normalize_text_utf8(std::string_view input)
{
    std::u16string u16_in = to_utf16(input);
    std::u16string u16_out = normalize_text(u16_in);
    return to_utf8(u16_out);
}

} // namespace hanvi
