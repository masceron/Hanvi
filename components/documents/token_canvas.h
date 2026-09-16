#pragma once

#include <QAbstractScrollArea>
#include <QTextLayout>
#include <vector>
#include <memory>
#include <optional>
#include "aligned_document.h"
#include "document_session.h"

struct CharPosition {
    size_t paragraph = 0;
    int char_index = 0;

    auto operator<=>(const CharPosition&) const = default;
};

struct CharSelection {
    CharPosition start;
    CharPosition end;

    [[nodiscard]] bool is_empty() const noexcept { return start == end; }
    [[nodiscard]] CharSelection normalized() const noexcept {
        return start <= end ? *this : CharSelection{.start = end, .end = start};
    }
};

class TokenCanvas : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit TokenCanvas(QWidget* parent = nullptr);
    ~TokenCanvas() override = default;

    void set_session(DocumentSession* session);
    [[nodiscard]] DocumentSession* session() const noexcept { return session_; }

    void set_role(LanguageRole role);
    [[nodiscard]] LanguageRole role() const noexcept { return role_; }

    void set_line_height_percent(int percent);
    void set_font(const QFont& font);

    [[nodiscard]] int scroll_value() const;
    void set_scroll_value(int val) const;
    void scroll_to_token(uint32_t token_id) const;

    [[nodiscard]] QString full_text() const;
    void copy_all_to_clipboard() const;

    void copy_selection_to_clipboard() const;

    [[nodiscard]] bool has_selection() const noexcept { return !normalized_selection().is_empty(); }
    [[nodiscard]] CharSelection normalized_selection() const noexcept { return CharSelection{.start = sel_start_, .end = sel_end_}.normalized(); }
    void clear_selection() { sel_start_ = {}; sel_end_ = {}; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    struct TokenSpan {
        uint32_t token_id = 0;
        int start_char = 0;
        int length = 0;
        size_t token_index = 0;
    };

    struct ParagraphLayout {
        QString text;
        std::vector<TokenSpan> spans;
        std::unique_ptr<QTextLayout> text_layout;
        qreal y = 0.0;
        qreal height = 0.0;
        bool layout_valid = false;
    };

    DocumentSession* session_ = nullptr;
    LanguageRole role_ = LanguageRole::Chinese;
    int line_height_percent_ = 100;
    QFont font_;
    qreal margin_ = 8.0;
    int last_layout_width_ = 0;

    std::vector<ParagraphLayout> layouts_;
    qreal total_height_ = 0.0;

    CharPosition sel_start_;
    CharPosition sel_end_;
    bool is_selecting_ = false;
    QPoint mouse_down_pos_;
    bool has_dragged_ = false;

    void rebuild_paragraph_data();
    void layout_all();
    void update_scroll_range() const;
    [[nodiscard]] size_t find_paragraph_at_y(qreal doc_y) const;
    [[nodiscard]] std::optional<std::pair<size_t, int>> char_at_pos(const QPoint& viewport_pos, bool clamp) const;
    [[nodiscard]] const TokenSpan* token_at_char(size_t p_idx, int char_pos) const;

private slots:
    void on_document_changed(const std::shared_ptr<const AlignedDocument>& doc);
    void on_active_token_changed(uint32_t token_id) const;
    void on_hovered_token_changed(uint32_t token_id) const;
};
