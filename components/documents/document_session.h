#pragma once

#include <QObject>
#include <memory>
#include "aligned_document.h"

class DocumentSession : public QObject
{
    Q_OBJECT

public:
    explicit DocumentSession(QObject* parent = nullptr);

    // Document management
    void set_document(std::shared_ptr<const AlignedDocument> doc);
    [[nodiscard]] std::shared_ptr<const AlignedDocument> document() const noexcept { return doc_; }
    [[nodiscard]] bool has_document() const noexcept { return doc_ != nullptr; }

    // Active (clicked) token state
    [[nodiscard]] uint32_t active_token_id() const noexcept { return active_token_id_; }
    void set_active_token(uint32_t token_id);
    void clear_active_token();

    // Hover state
    [[nodiscard]] uint32_t hovered_token_id() const noexcept { return hovered_token_id_; }
    void set_hovered_token(uint32_t token_id);
    void clear_hover();

    // Selection state
    [[nodiscard]] const DocumentSelection& selection() const noexcept { return selection_; }
    [[nodiscard]] bool has_selection() const noexcept { return !selection_.is_empty(); }
    void set_selection(const DocumentSelection& selection);
    void select_token(uint32_t token_id);
    void select_token_range(uint32_t start_token_id, uint32_t end_token_id);
    void clear_selection();

    // Text queries
    [[nodiscard]] QString selected_text(LanguageRole role) const;
    [[nodiscard]] QString selected_chinese_text() const;

    // Interaction triggers
    void trigger_click(uint32_t token_id);
    void trigger_context_menu(uint32_t clicked_token_id);

signals:
    void document_changed(std::shared_ptr<const AlignedDocument> doc);
    void active_token_changed(uint32_t token_id);
    void hovered_token_changed(uint32_t token_id);
    void selection_changed(const DocumentSelection& selection);
    void request_dict_popup(const QString& chinese_text);
    void request_rule_popup(const Rule* rule);
    void scroll_to_token_requested(uint32_t token_id);

private:
    std::shared_ptr<const AlignedDocument> doc_;
    uint32_t active_token_id_ = 0;
    uint32_t hovered_token_id_ = 0;
    DocumentSelection selection_;
};
