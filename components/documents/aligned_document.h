#pragma once

#include <vector>
#include <optional>
#include <cstdint>
#include "core/structures.h"

enum class LanguageRole : uint8_t
{
    Chinese,
    SinoVietnamese,
    Vietnamese
};

struct Token
{
    uint32_t id = 0;
    QString cn;
    QString sv;
    QString vn;
    const Rule* rule = nullptr;
    bool has_trailing_space = false;

    [[nodiscard]] const QString& text(const LanguageRole role) const noexcept
    {
        switch (role)
        {
        case LanguageRole::Chinese: return cn;
        case LanguageRole::SinoVietnamese: return sv;
        case LanguageRole::Vietnamese: return vn;
        }
        return cn;
    }

    [[nodiscard]] bool is_rule() const noexcept { return rule != nullptr; }
    [[nodiscard]] bool is_valid() const noexcept { return id != 0; }
};

namespace Typography
{
    inline bool is_closer_char(const QChar c) noexcept
    {
        return c == u'.' || c == u',' || c == u'!' || c == u'?' ||
            c == u':' || c == u';' || c == u'…' ||
            c == u')' || c == u']' || c == u'}' || c == u'>' ||
            c == u'”' || c == u'’' || c == u'」' || c == u'』' ||
            c == u'】' || c == u'》' || c == u'）' ||
            c == u'。' || c == u'，' || c == u'！' || c == u'？' ||
            c == u'：' || c == u'；';
    }

    inline bool is_opener_char(const QChar c) noexcept
    {
        return c == u'(' || c == u'[' || c == u'{' || c == u'<' ||
            c == u'“' || c == u'‘' || c == u'「' || c == u'『' ||
            c == u'【' || c == u'《' || c == u'（';
    }

    inline bool is_digit(const QChar c) noexcept
    {
        const ushort code = c.unicode();
        return (code >= '0' && code <= '9') ||
            (code >= 0xFF10 && code <= 0xFF19) ||
            c.isDigit();
    }

    inline bool is_latin_or_digit(const QChar c) noexcept
    {
        const ushort code = c.unicode();
        if ((code >= '0' && code <= '9') ||
            (code >= 'A' && code <= 'Z') ||
            (code >= 'a' && code <= 'z'))
        {
            return true;
        }
        if ((code >= 0xFF10 && code <= 0xFF19) ||
            (code >= 0xFF21 && code <= 0xFF3A) ||
            (code >= 0xFF41 && code <= 0xFF5A))
        {
            return true;
        }
        if (c.isDigit())
        {
            return true;
        }
        if (QChar::script(c.unicode()) == QChar::Script_Latin)
        {
            return true;
        }
        return false;
    }

    inline bool should_insert_space_before(const QString& text, const QString& tok_str, bool& in_quote,
                                           const Token* prev_tok = nullptr, const Token* cur_tok = nullptr,
                                           const Token* prev_prev_tok = nullptr) noexcept
    {
        if (text.isEmpty()) return false;
        if (text.endsWith(u' ')) return false;

        const QChar prev_char = text.back();
        const QChar first_char = tok_str[0];

        // Quotation mark handling
        if (tok_str == u"\"")
        {
            if (!in_quote)
            {
                in_quote = true;
                return !is_opener_char(prev_char);
            }
            else
            {
                in_quote = false;
                return false;
            }
        }

        // If previous character is an opener or opening quote, no space
        if (is_opener_char(prev_char) || (prev_char == u'"' && in_quote))
        {
            return false;
        }

        // If this token is a closer / punctuation, no space before it
        if (is_closer_char(first_char))
        {
            return false;
        }

        // If both adjacent tokens in source text are Latin scripts or digits, no space between them
        if (prev_tok != nullptr && cur_tok != nullptr &&
            !prev_tok->cn.isEmpty() && !cur_tok->cn.isEmpty())
        {
            if (is_latin_or_digit(prev_tok->cn.back()) && is_latin_or_digit(cur_tok->cn.front()))
            {
                return false;
            }
            // Handle decimal point or comma between digits (e.g. 3.14 or 15,000)
            if ((prev_tok->cn == u"." || prev_tok->cn == u",") &&
                prev_prev_tok != nullptr && !prev_prev_tok->cn.isEmpty() &&
                is_digit(prev_prev_tok->cn.back()) && is_digit(cur_tok->cn.front()))
            {
                return false;
            }
        }

        return true;
    }
} // namespace Typography

struct DocumentPosition
{
    size_t paragraph = 0;
    size_t token = 0;

    auto operator<=>(const DocumentPosition&) const = default;
};

struct DocumentSelection
{
    DocumentPosition start;
    DocumentPosition end;

    [[nodiscard]] bool is_empty() const noexcept { return start == end; }

    [[nodiscard]] DocumentSelection normalized() const noexcept
    {
        return start <= end ? *this : DocumentSelection{.start = end, .end = start};
    }
};

struct Paragraph
{
    std::vector<Token> tokens;
    [[nodiscard]] bool empty() const noexcept { return tokens.empty(); }
};

class AlignedDocument
{
public:
    std::vector<Paragraph> paragraphs;

    void build_index();

    [[nodiscard]] const Token* find_token(uint32_t id) const noexcept;
    [[nodiscard]] std::optional<DocumentPosition> find_token_position(uint32_t id) const noexcept;
    [[nodiscard]] QString get_text_range(const DocumentSelection& selection, LanguageRole role) const;
    [[nodiscard]] QString get_chinese_for_range(const DocumentSelection& selection) const;

    [[nodiscard]] size_t total_tokens() const noexcept { return total_tokens_; }
    void clear() noexcept;

private:
    std::vector<DocumentPosition> token_lut_;
    size_t total_tokens_ = 0;
};
