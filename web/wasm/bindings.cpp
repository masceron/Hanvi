#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>

#include "core/converter.h"
#include "core/db.h"
#include "core/dict.h"
#include "core/io.h"
#include "core/unicode_utils.h"

using emscripten::val;

namespace
{

// Whether a space goes before each token when the paragraph is laid out in `role`,
// following AlignedDocument::get_text_range.
std::vector<bool> spaces_before(const Paragraph& paragraph, const LanguageRole role)
{
    std::vector<bool> spaces(paragraph.tokens.size(), false);
    std::u16string text;
    bool in_quote = false;
    bool in_single_quote = false;
    bool trailing_space = false;
    const Token* last_tok = nullptr;
    const Token* prev_last_tok = nullptr;

    for (size_t t = 0; t < paragraph.tokens.size(); ++t)
    {
        const auto& tok = paragraph.tokens[t];
        const std::u16string& tok_str = tok.text(role);
        if (tok_str.empty()) continue;

        bool space = trailing_space;
        if (role != LanguageRole::Chinese &&
            Typography::should_insert_space_before(text, tok_str, in_quote, in_single_quote, last_tok, &tok, prev_last_tok))
        {
            text += u' ';
            space = true;
        }
        spaces[t] = space;
        text += tok_str;

        trailing_space = tok.has_trailing_space && text.back() != u' ';
        if (trailing_space) text += u' ';

        prev_last_tok = last_tok;
        last_tok = &tok;
    }
    return spaces;
}

val document_to_js(const AlignedDocument& document)
{
    val paragraphs = val::array();
    for (const auto& paragraph : document.paragraphs)
    {
        const auto cn_spaces = spaces_before(paragraph, LanguageRole::Chinese);
        const auto sv_spaces = spaces_before(paragraph, LanguageRole::SinoVietnamese);
        const auto vn_spaces = spaces_before(paragraph, LanguageRole::Vietnamese);

        val tokens = val::array();
        for (size_t t = 0; t < paragraph.tokens.size(); ++t)
        {
            const auto& token = paragraph.tokens[t];
            val js_token = val::object();
            js_token.set("id", token.id);
            js_token.set("cn", token.cn);
            js_token.set("sv", token.sv);
            js_token.set("vn", token.vn);
            js_token.set("cnSpace", bool{cn_spaces[t]});
            js_token.set("svSpace", bool{sv_spaces[t]});
            js_token.set("vnSpace", bool{vn_spaces[t]});
            js_token.set("isRule", token.is_rule());
            if (const Rule* rule = token.rule)
            {
                val js_rule = val::object();
                js_rule.set("originalStart", rule->original_start);
                js_rule.set("originalEnd", rule->original_end);
                js_rule.set("translationStart", rule->translation_start);
                js_rule.set("translationEnd", rule->translation_end);
                js_token.set("rule", js_rule);
            }
            tokens.call<void>("push", js_token);
        }
        paragraphs.call<void>("push", tokens);
    }
    return paragraphs;
}

// embind marshals std::u16string to and from JS strings (both UTF-16) but not string views.
std::u16string normalize(const std::u16string& text)
{
    return normalize_text(text);
}

val convert_document(const std::u16string& text)
{
    return document_to_js(*convert(text));
}

std::u16string convert_plain_text(const std::u16string& text)
{
    return convert_plain(text);
}

val list_name_sets()
{
    val sets = val::array();
    for (const auto& [index, title] : name_sets)
    {
        val set = val::object();
        set.set("id", index);
        set.set("title", title);
        sets.call<void>("push", set);
    }
    return sets;
}

int active_name_set()
{
    return current_name_set_id;
}

// The module is single-threaded, so loading runs inline and returns once the dictionaries are ready.
void load_dictionaries(const std::string& db_path)
{
    load_dict(db_path);
}

// Opens a database file that replaced the previous one (closed with closeDb) and reloads everything from it.
void reopen_dictionaries(const std::string& db_path)
{
    init_db(db_path);
    reload_dict();
}

void reload_dictionaries()
{
    reload_dict();
}

val optional_string(const StringView text)
{
    return text.isNull() ? val::null() : val(std::u16string(text));
}

// Hán Việt reading of arbitrary text, one syllable per character, falling back to a character's
// canonical variant and then to the character itself (as the desktop dictionary popup does).
std::u16string sv_reading_of(const std::u16string_view text)
{
    const auto reading_of = [](const char32_t cp) -> const std::u16string*
    {
        if (const auto it = sv_readings.find(cp); it != sv_readings.end()) return &it->second;
        if (const auto vit = variants.find(cp); vit != variants.end())
        {
            if (const auto it = sv_readings.find(hanvi::utf16_code_point_at(vit->second, 0)); it != sv_readings.end())
                return &it->second;
        }
        return nullptr;
    };

    std::u16string reading;
    size_t idx = 0;
    while (idx < text.size())
    {
        const char32_t cp = hanvi::utf16_code_point_at(text, idx);
        const size_t char_len = cp > 0xFFFF ? 2 : 1;
        const std::u16string* sv = reading_of(cp);
        reading += sv ? std::u16string_view(*sv) : text.substr(idx, char_len);
        idx += char_len;

        if (idx < text.size())
        {
            // Runs of unknown astral characters (e.g. emoji) stay together.
            const char32_t next_cp = hanvi::utf16_code_point_at(text, idx);
            if (char_len == 2 && !sv && next_cp > 0xFFFF && !reading_of(next_cp)) continue;
            reading += u' ';
        }
    }
    return reading;
}

// Everything the dictionary popup shows for a key: its reading, its name (from the active
// nameset when it has one, else the global one) and its phrase meanings in order.
val lookup(const std::u16string& key)
{
    val result = val::object();
    result.set("svReading", sv_reading_of(key));

    const ExactResult global = dictionary.find_exact(key);
    StringView name = global.name;
    bool from_name_set = false;
    if (current_name_set_id != -1)
    {
        if (const StringView set_name = name_set_dictionary.find_exact(key).name; !set_name.isNull())
        {
            name = set_name;
            from_name_set = true;
        }
    }
    result.set("name", optional_string(name));
    result.set("nameFromNameSet", from_name_set);

    val phrases = val::array();
    if (!global.full_phrase.isNull())
    {
        for (const auto& meaning : global.full_phrase.split(u'\x1F'))
        {
            phrases.call<void>("push", std::u16string(meaning));
        }
    }
    result.set("phrases", phrases);
    return result;
}

void insert_entry(const int set_id, const std::u16string& key, const std::u16string& value, const int priority)
{
    io_insert(set_id, key, value, static_cast<Priority>(priority));
}

void remove_entry(const int set_id, const std::u16string& key, const int priority)
{
    io_remove(set_id, key, static_cast<Priority>(priority));
}

void remove_meaning(const std::u16string& key, const std::u16string& value)
{
    io_remove_meaning(key, value);
}

void reorder_meanings(const std::u16string& key, const val& order)
{
    io_reorder(key, emscripten::vecFromJSArray<std::u16string>(order));
}

val list_entries(const int set_id, const int priority, const std::u16string& filter, const int offset, const int limit)
{
    val entries = val::array();
    for (const auto& [original, translated] : db_list_entries(set_id, static_cast<Priority>(priority), filter, offset, limit))
    {
        val entry = val::object();
        entry.set("original", original);
        entry.set("translated", translated);
        entries.call<void>("push", entry);
    }
    return entries;
}

int count_entries(const int set_id, const int priority)
{
    return db_count_entries(set_id, static_cast<Priority>(priority));
}

val get_translation(const std::u16string& key, const int priority)
{
    const auto translated = db_get_translation(key, static_cast<Priority>(priority));
    return translated ? val(*translated) : val::null();
}

// Entry counts keyed by nameset id.
val name_set_entry_counts()
{
    val counts = val::object();
    for (const auto& [id, count] : nameset_db_entry_counts())
    {
        counts.set(id, count);
    }
    return counts;
}

// New nameset ids, or -1 when the title is taken (or an import holds duplicate keys).
int create_name_set(const std::u16string& title)
{
    return io_create_name_set(title).value_or(-1);
}

int import_name_set(const std::u16string& title, const val& js_entries)
{
    std::vector<Entry> entries;
    const auto length = js_entries["length"].as<unsigned>();
    entries.reserve(length);
    for (unsigned i = 0; i < length; ++i)
    {
        const val entry = js_entries[i];
        entries.push_back(Entry{
            .original = entry["original"].as<std::u16string>(),
            .translated = entry["translated"].as<std::u16string>(),
        });
    }
    return io_import_name_set(title, entries).value_or(-1);
}

bool rename_name_set(const int id, const std::u16string& title)
{
    return io_rename_name_set(id, title);
}

} // namespace

EMSCRIPTEN_BINDINGS(hanvi)
{
    emscripten::function("initOpencc", &hanvi::init_opencc);
    emscripten::function("loadDict", &load_dictionaries);
    emscripten::function("reopenDict", &reopen_dictionaries);
    emscripten::function("reloadDict", &reload_dictionaries);
    emscripten::function("closeDb", &hanvi::close_db);
    emscripten::function("saveDbCopy", &hanvi::save_db_copy);
    emscripten::function("dbChangeCount", &hanvi::db_change_count);

    emscripten::function("normalize", &normalize);
    emscripten::function("convert", &convert_document);
    emscripten::function("convertPlain", &convert_plain_text);

    emscripten::function("nameSets", &list_name_sets);
    emscripten::function("activeNameSet", &active_name_set);
    emscripten::function("loadNameSet", &hanvi::load_name_set);
    emscripten::function("nameSetEntryCounts", &name_set_entry_counts);
    emscripten::function("createNameSet", &create_name_set);
    emscripten::function("importNameSet", &import_name_set);
    emscripten::function("renameNameSet", &rename_name_set);
    emscripten::function("deleteNameSet", &hanvi::io_delete_name_set);

    // Dictionary editing; priority is core's Priority (1 = phrase, 2 = name) and set id -1 is the global dictionary.
    emscripten::function("lookup", &lookup);
    emscripten::function("insertEntry", &insert_entry);
    emscripten::function("removeEntry", &remove_entry);
    emscripten::function("removeMeaning", &remove_meaning);
    emscripten::function("reorderMeanings", &reorder_meanings);
    emscripten::function("listEntries", &list_entries);
    emscripten::function("countEntries", &count_entries);
    emscripten::function("getTranslation", &get_translation);
}
