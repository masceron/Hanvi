#include "aligned_document.h"
#include <algorithm>

void AlignedDocument::build_index() {
    token_lut_.clear();
    total_tokens_ = 0;

    for (size_t p = 0; p < paragraphs.size(); ++p) {
        total_tokens_ += paragraphs[p].tokens.size();
    }

    token_lut_.resize(total_tokens_ + 1);

    for (size_t p = 0; p < paragraphs.size(); ++p) {
        const auto& para = paragraphs[p];
        for (size_t t = 0; t < para.tokens.size(); ++t) {
            const uint32_t id = para.tokens[t].id;
            if (id < token_lut_.size()) {
                token_lut_[id] = DocumentPosition{p, t};
            }
        }
    }
}

const Token* AlignedDocument::find_token(uint32_t id) const noexcept {
    if (id == 0 || id >= token_lut_.size()) return nullptr;
    const auto& pos = token_lut_[id];
    if (pos.paragraph < paragraphs.size()) {
        const auto& para = paragraphs[pos.paragraph];
        if (pos.token < para.tokens.size()) {
            return &para.tokens[pos.token];
        }
    }
    return nullptr;
}

std::optional<DocumentPosition> AlignedDocument::find_token_position(uint32_t id) const noexcept {
    if (id == 0 || id >= token_lut_.size()) return std::nullopt;
    const auto& pos = token_lut_[id];
    if (pos.paragraph < paragraphs.size() && pos.token < paragraphs[pos.paragraph].tokens.size()) {
        return pos;
    }
    return std::nullopt;
}

QString AlignedDocument::get_text_range(const DocumentSelection& selection, LanguageRole role) const {
    if (selection.is_empty()) return {};

    const auto norm = selection.normalized();
    QString result;

    const size_t start_p = norm.start.paragraph;
    const size_t end_p = std::min(norm.end.paragraph, paragraphs.size() - 1);

    for (size_t p = start_p; p <= end_p && p < paragraphs.size(); ++p) {
        const auto& para = paragraphs[p];
        const size_t start_t = (p == start_p) ? norm.start.token : 0;
        const size_t end_t = (p == norm.end.paragraph) ? std::min(norm.end.token, para.tokens.size()) : para.tokens.size();

        bool in_quote = false;

        for (size_t t = start_t; t < end_t; ++t) {
            const auto& tok = para.tokens[t];
            const QString tok_str = tok.text(role);

            if (tok_str.isEmpty()) continue;

            if (role != LanguageRole::Chinese) {
                if (Typography::should_insert_space_before(result, tok_str, in_quote)) {
                    result += u' ';
                }
            }

            result += tok_str;

            if (tok.has_trailing_space && !result.endsWith(u' ')) {
                result += u' ';
            }
        }

        if (p < end_p) {
            result += u'\n';
        }
    }

    return result;
}

QString AlignedDocument::get_chinese_for_range(const DocumentSelection& selection) const {
    return get_text_range(selection, LanguageRole::Chinese);
}

void AlignedDocument::clear() noexcept {
    paragraphs.clear();
    token_lut_.clear();
    total_tokens_ = 0;
}
