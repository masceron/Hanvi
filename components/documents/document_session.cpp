#include "document_session.h"
#include <algorithm>

DocumentSession::DocumentSession(QObject* parent)
    : QObject(parent) {
}

void DocumentSession::set_document(std::shared_ptr<const AlignedDocument> doc) {
    doc_ = std::move(doc);
    active_token_id_ = 0;
    hovered_token_id_ = 0;
    selection_ = {};

    emit document_changed(doc_);
    emit active_token_changed(0);
    emit hovered_token_changed(0);
    emit selection_changed({});
}

void DocumentSession::set_active_token(uint32_t token_id) {
    if (active_token_id_ == token_id) return;
    active_token_id_ = token_id;
    emit active_token_changed(active_token_id_);
}

void DocumentSession::clear_active_token() {
    set_active_token(0);
}

void DocumentSession::set_hovered_token(uint32_t token_id) {
    if (hovered_token_id_ == token_id) return;
    hovered_token_id_ = token_id;
    emit hovered_token_changed(hovered_token_id_);
}

void DocumentSession::clear_hover() {
    set_hovered_token(0);
}

void DocumentSession::set_selection(const DocumentSelection& selection) {
    if (selection_.start == selection.start && selection_.end == selection.end) return;
    selection_ = selection;
    emit selection_changed(selection_);
}

void DocumentSession::select_token(uint32_t token_id) {
    if (!doc_ || token_id == 0) {
        clear_selection();
        return;
    }

    const auto pos = doc_->find_token_position(token_id);
    if (!pos) {
        clear_selection();
        return;
    }

    set_selection({*pos, {pos->paragraph, pos->token + 1}});
}

void DocumentSession::select_token_range(uint32_t start_token_id, uint32_t end_token_id) {
    if (!doc_ || start_token_id == 0 || end_token_id == 0) {
        clear_selection();
        return;
    }

    const auto start_pos = doc_->find_token_position(start_token_id);
    const auto end_pos = doc_->find_token_position(end_token_id);

    if (!start_pos || !end_pos) {
        clear_selection();
        return;
    }

    if (*start_pos <= *end_pos) {
        set_selection({*start_pos, {end_pos->paragraph, end_pos->token + 1}});
    } else {
        set_selection({*end_pos, {start_pos->paragraph, start_pos->token + 1}});
    }
}

void DocumentSession::clear_selection() {
    if (selection_.is_empty()) return;
    selection_ = {};
    emit selection_changed(selection_);
}

QString DocumentSession::selected_text(LanguageRole role) const {
    if (!doc_ || selection_.is_empty()) return {};
    return doc_->get_text_range(selection_, role);
}

QString DocumentSession::selected_chinese_text() const {
    if (!doc_ || selection_.is_empty()) return {};
    return doc_->get_chinese_for_range(selection_);
}

void DocumentSession::trigger_click(uint32_t token_id) {
    if (token_id != 0) {
        set_active_token(token_id);
        clear_selection();
        emit scroll_to_token_requested(token_id);
    } else {
        clear_active_token();
        clear_selection();
    }
}

void DocumentSession::trigger_context_menu(uint32_t clicked_token_id) {
    if (!doc_) return;

    // Check if the current selection contains any rule tokens
    if (has_selection()) {
        const auto norm = selection_.normalized();
        for (size_t p = norm.start.paragraph; p <= norm.end.paragraph && p < doc_->paragraphs.size(); ++p) {
            const auto& para = doc_->paragraphs[p];
            const size_t start_t = (p == norm.start.paragraph) ? norm.start.token : 0;
            const size_t end_t = (p == norm.end.paragraph) ? std::min(norm.end.token, para.tokens.size()) : para.tokens.size();

            for (size_t t = start_t; t < end_t; ++t) {
                const auto& tok = para.tokens[t];
                if (tok.is_rule()) {
                    emit request_rule_popup(tok.rule);
                    return;
                }
            }
        }

        const QString cn_text = selected_chinese_text();
        if (!cn_text.isEmpty()) {
            emit request_dict_popup(cn_text);
            return;
        }
    }

    // If no multi-token selection, inspect the clicked token
    if (clicked_token_id != 0) {
        set_active_token(clicked_token_id);
        const Token* tok = doc_->find_token(clicked_token_id);
        if (!tok) return;

        if (tok->is_rule()) {
            emit request_rule_popup(tok->rule);
        } else if (!tok->cn.isEmpty()) {
            emit request_dict_popup(tok->cn);
        }
    }
}
