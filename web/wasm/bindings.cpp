#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>

#include "core/converter.h"
#include "core/dict.h"

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

} // namespace

EMSCRIPTEN_BINDINGS(hanvi)
{
    emscripten::function("initOpencc", &hanvi::init_opencc);
    emscripten::function("loadDict", &load_dictionaries);
    emscripten::function("closeDb", &hanvi::close_db);
    emscripten::function("saveDbCopy", &hanvi::save_db_copy);
    emscripten::function("dbChangeCount", &hanvi::db_change_count);

    emscripten::function("normalize", &normalize);
    emscripten::function("convert", &convert_document);
    emscripten::function("convertPlain", &convert_plain_text);

    emscripten::function("nameSets", &list_name_sets);
    emscripten::function("activeNameSet", &active_name_set);
    emscripten::function("loadNameSet", &hanvi::load_name_set);
}
