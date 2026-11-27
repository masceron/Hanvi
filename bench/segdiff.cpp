// Compares the Greedy and Frequency segmentations of a text.
//
//   HanviSegDiff [--dict dict.db] [--opencc t2s.json] [--out dir] [--dump] <file.txt>
//
// Texts must be UTF-8. Prints how long each conversion took and how many words and characters the
// two disagree on. In --out (default "segdiff") it writes
//   patterns.tsv  each distinct disagreement, most frequent first
//   regions.tsv   every disagreement with its context, in text order
// and with --dump also greedy_words.tsv and frequency_words.tsv, every word of each (offset, cn,
// vn), for comparing runs on different dictionaries. A disagreement is the shortest stretch of text
// both cut at its two ends but differently inside.
//
// The dictionary is copied into --out first: opening one lets the core add tables to it.
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <print>
#include <string>
#include <vector>

#include "core/converter.h"
#include "core/dict.h"
#include "core/unicode_utils.h"

namespace fs = std::filesystem;

namespace {
    struct Word {
        size_t offset;
        size_t end;
        const hanvi::Token *token;
    };

    std::vector<Word> words_of(const hanvi::AlignedDocument &doc) {
        std::vector<Word> words;
        for (const auto &paragraph: doc.paragraphs) {
            for (const auto &token: paragraph.tokens) {
                words.push_back({token.offset, token.offset + token.cn.size(), &token});
            }
        }
        std::ranges::stable_sort(words, {}, &Word::offset);
        return words;
    }

    std::string cut(const std::vector<Word> &words, const size_t from, const size_t to, const bool with_vn) {
        std::string out;
        for (size_t k = from; k < to; ++k) {
            if (k > from) out += with_vn ? " | " : "|";
            out += hanvi::to_utf8(words[k].token->cn);
            if (with_vn) out += "=" + hanvi::to_utf8(words[k].token->vn);
        }
        return out;
    }

    std::string tsv_field(std::string s) {
        std::ranges::replace(s, '\t', ' ');
        std::ranges::replace(s, '\n', ' ');
        return s;
    }
}

int main(int argc, char *argv[]) {
    fs::path dict_path = "dict.db";
    std::string opencc_path = "opencc/t2s.json";
    fs::path out_dir = "segdiff";
    fs::path input_path;
    bool dump = false;
    for (int k = 1; k < argc; ++k) {
        const std::string_view arg = argv[k];
        if (arg == "--dict" && k + 1 < argc) dict_path = argv[++k];
        else if (arg == "--opencc" && k + 1 < argc) opencc_path = argv[++k];
        else if (arg == "--out" && k + 1 < argc) out_dir = argv[++k];
        else if (arg == "--dump") dump = true;
        else input_path = arg;
    }
    if (input_path.empty()) {
        std::println(stderr, "usage: HanviSegDiff [--dict dict.db] [--opencc t2s.json] [--out dir] [--dump] <file.txt>");
        return 1;
    }

    fs::create_directories(out_dir);
    const fs::path dict_copy = out_dir / "dict.db";
    fs::copy_file(dict_path, dict_copy, fs::copy_options::overwrite_existing);
    hanvi::load_dict(dict_copy.string());
    hanvi::init_opencc(opencc_path);
    if (!hanvi::dictionary.has_weights()) {
        std::println(stderr, "{} has no segment_weights: both sides would be greedy.", dict_path.string());
        return 1;
    }

    std::ifstream file(input_path, std::ios::binary);
    std::string raw((std::istreambuf_iterator(file)), std::istreambuf_iterator<char>());
    if (raw.starts_with("\xEF\xBB\xBF")) raw.erase(0, 3);
    std::erase(raw, '\r');
    const std::u16string text = hanvi::normalize_text(hanvi::to_utf16(raw));

    const auto timed = [&text](const hanvi::Segmentation mode) {
        hanvi::segmentation = mode;
        const auto begin = std::chrono::steady_clock::now();
        auto doc = hanvi::convert(text);
        const std::chrono::duration<double, std::milli> ms = std::chrono::steady_clock::now() - begin;
        return std::pair{doc, ms.count()};
    };
    const auto [greedy_doc, greedy_ms] = timed(hanvi::Segmentation::Greedy);
    const auto [frequency_doc, frequency_ms] = timed(hanvi::Segmentation::Frequency);
    std::println("convert: greedy {:.0f} ms, frequency {:.0f} ms", greedy_ms, frequency_ms);
    const auto greedy = words_of(*greedy_doc);
    const auto frequency = words_of(*frequency_doc);

    struct Pattern {
        size_t count = 0;
        std::string greedy_vn;
        std::string frequency_vn;
        std::string example;
    };
    std::map<std::pair<std::string, std::string>, Pattern> patterns;
    std::ofstream regions(out_dir / "regions.tsv", std::ios::binary);
    std::println(regions, "offset\tcontext\tgreedy\tfrequency");

    size_t regions_count = 0, region_chars = 0, region_words = 0;
    size_t a = 0, b = 0;
    while (a < greedy.size() && b < frequency.size()) {
        if (greedy[a].offset == frequency[b].offset && greedy[a].end == frequency[b].end) {
            ++a;
            ++b;
            continue;
        }
        const size_t start = std::min(greedy[a].offset, frequency[b].offset);
        const size_t from_a = a, from_b = b;
        size_t end_a = greedy[a].end, end_b = frequency[b].end;
        while (end_a != end_b) {
            if (end_a < end_b) {
                if (++a == greedy.size()) break;
                end_a = greedy[a].end;
            } else {
                if (++b == frequency.size()) break;
                end_b = frequency[b].end;
            }
        }
        if (a == greedy.size() || b == frequency.size()) break;
        ++a;
        ++b;

        ++regions_count;
        region_chars += end_a - start;
        region_words += a - from_a;

        size_t ctx_from = start >= 12 ? start - 12 : 0;
        size_t ctx_to = std::min(text.size(), end_a + 12);
        if (ctx_from > 0 && hanvi::is_low_surrogate(text[ctx_from])) --ctx_from;
        if (ctx_to < text.size() && hanvi::is_low_surrogate(text[ctx_to])) ++ctx_to;
        std::u16string context = text.substr(ctx_from, ctx_to - ctx_from);
        std::ranges::replace(context, u'\n', u' ');
        const std::string context8 = hanvi::to_utf8(context);

        auto &pattern = patterns[{cut(greedy, from_a, a, false), cut(frequency, from_b, b, false)}];
        if (pattern.count++ == 0) {
            pattern.greedy_vn = cut(greedy, from_a, a, true);
            pattern.frequency_vn = cut(frequency, from_b, b, true);
            pattern.example = context8;
        }
        std::println(regions, "{}\t{}\t{}\t{}", start, tsv_field(context8),
                     tsv_field(cut(greedy, from_a, a, true)), tsv_field(cut(frequency, from_b, b, true)));
    }

    if (dump) {
        for (const auto &[name, words]: {std::pair{"greedy", &greedy}, std::pair{"frequency", &frequency}}) {
            std::ofstream out(out_dir / std::format("{}_words.tsv", name), std::ios::binary);
            for (const Word &w: *words) {
                std::println(out, "{}\t{}\t{}", w.offset, hanvi::to_utf8(w.token->cn),
                             tsv_field(hanvi::to_utf8(w.token->vn)));
            }
        }
    }

    std::vector<const decltype(patterns)::value_type *> sorted;
    for (const auto &entry: patterns) sorted.push_back(&entry);
    std::ranges::stable_sort(sorted, std::greater{}, [](const auto *e) { return e->second.count; });
    std::ofstream pattern_file(out_dir / "patterns.tsv", std::ios::binary);
    std::println(pattern_file, "count\tgreedy\tfrequency\texample");
    for (const auto *entry: sorted) {
        std::println(pattern_file, "{}\t{}\t{}\t{}", entry->second.count, tsv_field(entry->second.greedy_vn),
                     tsv_field(entry->second.frequency_vn), tsv_field(entry->second.example));
    }

    size_t han_chars = 0;
    for (const char16_t c: text) han_chars += hanvi::is_han_unit(c);
    std::println("text: {} UTF-16 units, {} Han characters", text.size(), han_chars);
    std::println("words: greedy {}, frequency {}", greedy.size(), frequency.size());
    std::println("disagreements: {} ({} distinct), covering {} characters ({:.2f}%) and {} greedy words ({:.2f}%)",
                 regions_count, patterns.size(), region_chars, 100.0 * region_chars / std::max<size_t>(han_chars, 1),
                 region_words, 100.0 * region_words / std::max<size_t>(greedy.size(), 1));
    hanvi::close_db();
    return 0;
}
