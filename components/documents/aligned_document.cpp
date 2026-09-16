#include "aligned_document.h"
#include <algorithm>

void AlignedDocument::build_index()
{
    token_lut_.clear();
    total_tokens_ = 0;

    for (auto& [tokens] : paragraphs)
    {
        total_tokens_ += tokens.size();
    }

    token_lut_.resize(total_tokens_ + 1);

    for (size_t p = 0; p < paragraphs.size(); ++p)
    {
        const auto& [tokens] = paragraphs[p];
        for (size_t t = 0; t < tokens.size(); ++t)
        {
            if (const uint32_t id = tokens[t].id; id < token_lut_.size())
            {
                token_lut_[id] = DocumentPosition{.paragraph = p, .token = t};
            }
        }
    }
}

const Token* AlignedDocument::find_token(const uint32_t id) const noexcept
{
    if (id == 0 || id >= token_lut_.size()) return nullptr;
    if (const auto& [paragraph, token] = token_lut_[id]; paragraph < paragraphs.size())
    {
        const auto& [tokens] = paragraphs[paragraph];
        if (token < tokens.size())
        {
            return &tokens[token];
        }
    }
    return nullptr;
}

std::optional<DocumentPosition> AlignedDocument::find_token_position(const uint32_t id) const noexcept
{
    if (id == 0 || id >= token_lut_.size()) return std::nullopt;
    if (const auto& pos = token_lut_[id]; pos.paragraph < paragraphs.size() && pos.token < paragraphs[pos.paragraph].
        tokens.size())
    {
        return pos;
    }
    return std::nullopt;
}

QString AlignedDocument::get_text_range(const DocumentSelection& selection, const LanguageRole role) const
{
    if (selection.is_empty()) return {};

    const auto [start, end] = selection.normalized();
    QString result;

    const size_t start_p = start.paragraph;
    const size_t end_p = std::min(end.paragraph, paragraphs.size() - 1);

    for (size_t p = start_p; p <= end_p && p < paragraphs.size(); ++p)
    {
        const auto& [tokens] = paragraphs[p];
        const size_t start_t = p == start_p ? start.token : 0;
        const size_t end_t = p == end.paragraph
                                 ? std::min(end.token, tokens.size())
                                 : tokens.size();

        bool in_quote = false;

        for (size_t t = start_t; t < end_t; ++t)
        {
            const auto& tok = tokens[t];
            const QString tok_str = tok.text(role);

            if (tok_str.isEmpty()) continue;

            if (role != LanguageRole::Chinese)
            {
                if (Typography::should_insert_space_before(result, tok_str, in_quote))
                {
                    result += u' ';
                }
            }

            result += tok_str;

            if (tok.has_trailing_space && !result.endsWith(u' '))
            {
                result += u' ';
            }
        }

        if (p < end_p)
        {
            result += u'\n';
        }
    }

    return result;
}

QString AlignedDocument::get_chinese_for_range(const DocumentSelection& selection) const
{
    return get_text_range(selection, LanguageRole::Chinese);
}

void AlignedDocument::clear() noexcept
{
    paragraphs.clear();
    token_lut_.clear();
    total_tokens_ = 0;
}
