#include "token_canvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QScrollBar>
#include <QGuiApplication>
#include <QClipboard>
#include <algorithm>

TokenCanvas::TokenCanvas(QWidget* parent)
    : QAbstractScrollArea(parent)
{
    viewport()->setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    font_ = font();
    font_.setPixelSize(16);
}

void TokenCanvas::set_session(DocumentSession* session)
{
    if (session_ == session) return;

    if (session_)
    {
        disconnect(session_, nullptr, this, nullptr);
    }

    session_ = session;

    if (session_)
    {
        connect(session_, &DocumentSession::document_changed, this, &TokenCanvas::on_document_changed);
        connect(session_, &DocumentSession::active_token_changed, this, &TokenCanvas::on_active_token_changed);
        connect(session_, &DocumentSession::hovered_token_changed, this, &TokenCanvas::on_hovered_token_changed);
        connect(session_, &DocumentSession::scroll_to_token_requested, this, &TokenCanvas::scroll_to_token);

        if (session_->has_document())
        {
            on_document_changed(session_->document());
        }
    }
    else
    {
        layouts_.clear();
        total_height_ = 0.0;
        update_scroll_range();
        viewport()->update();
    }
}

void TokenCanvas::set_role(const LanguageRole role)
{
    if (role_ == role) return;
    role_ = role;
    rebuild_paragraph_data();
}

void TokenCanvas::set_line_height_percent(const int percent)
{
    if (line_height_percent_ == percent) return;
    line_height_percent_ = percent;
    layout_all();
}

void TokenCanvas::set_font(const QFont& font)
{
    font_ = font;
    layout_all();
}

int TokenCanvas::scroll_value() const
{
    return verticalScrollBar()->value();
}

void TokenCanvas::set_scroll_value(const int val) const
{
    verticalScrollBar()->setValue(val);
}

QString TokenCanvas::full_text() const
{
    QString result;
    for (size_t p = 0; p < layouts_.size(); ++p)
    {
        result += layouts_[p].text;
        if (p + 1 < layouts_.size())
        {
            result += u'\n';
        }
    }
    return result;
}

void TokenCanvas::copy_all_to_clipboard() const
{
    const QString text = full_text();
    if (!text.isEmpty())
    {
        QGuiApplication::clipboard()->setText(text);
    }
}

void TokenCanvas::copy_selection_to_clipboard() const
{
    if (!has_selection()) return;
    const auto [start, end] = normalized_selection();
    QString result;

    for (size_t p = start.paragraph; p <= end.paragraph && p < layouts_.size(); ++p)
    {
        const auto& pl = layouts_[p];
        const int start_c = (p == start.paragraph) ? start.char_index : 0;
        const int end_c = (p == end.paragraph) ? end.char_index : static_cast<int>(pl.text.length());

        if (start_c < end_c && start_c < pl.text.length())
        {
            result += pl.text.mid(start_c, end_c - start_c);
        }
        if (p < end.paragraph)
        {
            result += u'\n';
        }
    }

    if (!result.isEmpty())
    {
        QGuiApplication::clipboard()->setText(result);
    }
}

void TokenCanvas::on_document_changed(const std::shared_ptr<const AlignedDocument>& /*doc*/)
{
    clear_selection();
    rebuild_paragraph_data();
}

void TokenCanvas::on_active_token_changed(uint32_t /*token_id*/) const
{
    viewport()->update();
}

void TokenCanvas::on_hovered_token_changed(uint32_t /*token_id*/) const
{
    viewport()->update();
}

void TokenCanvas::rebuild_paragraph_data()
{
    layouts_.clear();
    total_height_ = 0.0;

    if (!session_ || !session_->has_document())
    {
        update_scroll_range();
        viewport()->update();
        return;
    }

    const auto& doc = session_->document();
    layouts_.reserve(doc->paragraphs.size());

    for (const auto & [tokens] : doc->paragraphs)
    {
        ParagraphLayout pl;

        if (role_ == LanguageRole::Chinese)
        {
            for (size_t t = 0; t < tokens.size(); ++t)
            {
                const auto& tok = tokens[t];
                const QString tok_str = tok.cn;

                TokenSpan span;
                span.token_id = tok.id;
                span.start_char = static_cast<int>(pl.text.length());
                span.length = static_cast<int>(tok_str.length());
                span.token_index = t;

                pl.text += tok_str;
                if (tok.has_trailing_space)
                {
                    pl.text += u' ';
                }

                pl.spans.push_back(span);
            }
        }
        else
        {
            bool in_quote = false;
            for (size_t t = 0; t < tokens.size(); ++t)
            {
                const auto& tok = tokens[t];
                const QString tok_str = tok.text(role_);

                if (tok_str.isEmpty())
                {
                    TokenSpan span;
                    span.token_id = tok.id;
                    span.start_char = static_cast<int>(pl.text.length());
                    span.length = 0;
                    span.token_index = t;
                    pl.spans.push_back(span);
                    continue;
                }

                if (Typography::should_insert_space_before(pl.text, tok_str, in_quote))
                {
                    pl.text += u' ';
                }

                TokenSpan span;
                span.token_id = tok.id;
                span.start_char = static_cast<int>(pl.text.length());
                span.length = static_cast<int>(tok_str.length());
                span.token_index = t;

                pl.text += tok_str;

                if (tok.has_trailing_space && !pl.text.endsWith(u' '))
                {
                    pl.text += u' ';
                }

                pl.spans.push_back(span);
            }
        }

        layouts_.push_back(std::move(pl));
    }

    layout_all();
}

void TokenCanvas::layout_all()
{
    const int avail_width = viewport()->width() - static_cast<int>(margin_ * 2.0);
    if (avail_width <= 0) return;

    last_layout_width_ = avail_width;
    qreal current_y = margin_;
    const qreal line_scale = line_height_percent_ / 100.0;
    const QFontMetricsF fm(font_);
    const qreal empty_line_h = fm.lineSpacing() * line_scale;

    for (auto& pl : layouts_)
    {
        if (pl.text.isEmpty())
        {
            pl.y = current_y;
            pl.height = empty_line_h;
            current_y += empty_line_h;
            pl.layout_valid = true;
            continue;
        }

        pl.text_layout = std::make_unique<QTextLayout>(pl.text, font_);
        pl.text_layout->beginLayout();

        qreal line_y = 0.0;
        while (true)
        {
            QTextLine line = pl.text_layout->createLine();
            if (!line.isValid()) break;

            line.setLineWidth(avail_width);
            const qreal line_h = line.height() * line_scale;
            line.setPosition(QPointF(0.0, line_y));
            line_y += line_h;
        }

        pl.text_layout->endLayout();

        pl.y = current_y;
        pl.height = line_y;
        current_y += line_y;
        pl.layout_valid = true;
    }

    total_height_ = current_y + margin_;
    update_scroll_range();
    viewport()->update();
}

void TokenCanvas::update_scroll_range() const
{
    const int max_scroll = std::max(0, static_cast<int>(total_height_) - viewport()->height());
    verticalScrollBar()->setRange(0, max_scroll);
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(30);
}

void TokenCanvas::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    const int current_avail_width = viewport()->width() - static_cast<int>(margin_ * 2.0);
    if (current_avail_width != last_layout_width_)
    {
        layout_all();
    }
    else
    {
        update_scroll_range();
    }
}

size_t TokenCanvas::find_paragraph_at_y(const qreal doc_y) const
{
    if (layouts_.empty()) return 0;

    const auto it = std::lower_bound(layouts_.begin(), layouts_.end(), doc_y,
                               [](const ParagraphLayout& pl, const qreal y)
                               {
                                   return (pl.y + pl.height) < y;
                               });

    if (it == layouts_.end()) return layouts_.size() - 1;
    return std::distance(layouts_.begin(), it);
}

std::optional<std::pair<size_t, int>> TokenCanvas::char_at_pos(const QPoint& viewport_pos, const bool clamp) const
{
    if (layouts_.empty()) return std::nullopt;

    const qreal doc_y = viewport_pos.y() + verticalScrollBar()->value();
    const qreal doc_x = viewport_pos.x() - margin_;

    size_t p_idx = find_paragraph_at_y(doc_y);
    if (p_idx >= layouts_.size())
    {
        if (!clamp) return std::nullopt;
        p_idx = layouts_.size() - 1;
    }

    const auto& pl = layouts_[p_idx];
    if (!pl.layout_valid || !pl.text_layout || pl.text.isEmpty())
    {
        if (clamp) return std::make_pair(p_idx, 0);
        return std::nullopt;
    }

    qreal local_y = doc_y - pl.y;
    if (clamp)
    {
        local_y = std::clamp(local_y, 0.0, pl.height);
    }
    else if (local_y < 0.0 || local_y > pl.height)
    {
        return std::nullopt;
    }

    const qreal line_scale = line_height_percent_ / 100.0;
    QTextLine matched_line;
    for (int l = 0; l < pl.text_layout->lineCount(); ++l)
    {
        QTextLine line = pl.text_layout->lineAt(l);
        if (local_y >= line.y() && local_y <= (line.y() + line.height() * line_scale))
        {
            matched_line = line;
            break;
        }
    }

    if (!matched_line.isValid() && pl.text_layout->lineCount() > 0)
    {
        if (local_y < 0.0)
        {
            matched_line = pl.text_layout->lineAt(0);
        }
        else
        {
            matched_line = pl.text_layout->lineAt(pl.text_layout->lineCount() - 1);
        }
    }
    if (!matched_line.isValid()) return std::nullopt;

    const qreal clamped_x = std::clamp(doc_x, 0.0, matched_line.width());
    const int cursor_pos = matched_line.xToCursor(clamped_x);
    return std::make_pair(p_idx, cursor_pos);
}

const TokenCanvas::TokenSpan* TokenCanvas::token_at_char(const size_t p_idx, const int char_pos) const
{
    if (p_idx >= layouts_.size()) return nullptr;
    const auto& pl = layouts_[p_idx];
    for (const auto& span : pl.spans)
    {
        if (span.length == 0) continue;
        if (char_pos >= span.start_char && char_pos < (span.start_char + span.length))
        {
            return &span;
        }
    }
    return nullptr;
}

void TokenCanvas::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.fillRect(event->rect(), QColor(0x2d, 0x2d, 0x2d));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const int scroll_y = verticalScrollBar()->value();
    const qreal view_top = scroll_y;
    const qreal view_bottom = scroll_y + viewport()->height();

    const uint32_t active_id = session_ ? session_->active_token_id() : 0;
    const uint32_t hovered_id = session_ ? session_->hovered_token_id() : 0;

    const bool has_sel = has_selection();
    const auto [start, end] = normalized_selection();

    painter.setFont(font_);

    for (size_t p = 0; p < layouts_.size(); ++p)
    {
        const auto& [text, spans, text_layout, y, height, layout_valid] = layouts_[p];
        if (y + height < view_top || y > view_bottom) continue;
        if (!layout_valid || !text_layout) continue;

        const qreal draw_y = y - scroll_y;

        QList<QTextLayout::FormatRange> format_ranges;

        // 1. Character selection range (blue highlight)
        if (has_sel && p >= start.paragraph && p <= end.paragraph)
        {
            const int start_c = (p == start.paragraph) ? start.char_index : 0;
            const int end_c = (p == end.paragraph)
                                  ? end.char_index
                                  : static_cast<int>(text.length());

            if (start_c < end_c)
            {
                QTextLayout::FormatRange fr;
                fr.start = start_c;
                fr.length = end_c - start_c;
                fr.format.setBackground(QColor(0x21, 0x42, 0x83));
                fr.format.setForeground(Qt::white);
                format_ranges.append(fr);
            }
        }

        // 2. Active / Hovered token highlight (bold red)
        if (active_id != 0 || hovered_id != 0)
        {
            for (const auto& span : spans)
            {
                if (span.length > 0 && (span.token_id == active_id || span.token_id == hovered_id))
                {
                    QTextLayout::FormatRange fr;
                    fr.start = span.start_char;
                    fr.length = span.length;
                    fr.format.setForeground(QColor(255, 68, 68));
                    fr.format.setFontWeight(QFont::Bold);
                    format_ranges.append(fr);
                }
            }
        }

        painter.setPen(Qt::white);
        text_layout->draw(&painter, QPointF(margin_, draw_y), format_ranges);
    }
}

void TokenCanvas::mousePressEvent(QMouseEvent* event)
{
    setFocus();

    if (event->button() == Qt::LeftButton)
    {
        mouse_down_pos_ = event->pos();
        has_dragged_ = false;

        if (const auto hit = char_at_pos(event->pos(), false))
        {
            is_selecting_ = true;
            sel_start_ = CharPosition{.paragraph = hit->first, .char_index = hit->second};
            sel_end_ = sel_start_;
        }
        else
        {
            clear_selection();
            if (session_) session_->clear_active_token();
        }
        viewport()->update();
    }
}

void TokenCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (is_selecting_)
    {
        if (!has_dragged_ && (event->pos() - mouse_down_pos_).manhattanLength() > 3)
        {
            has_dragged_ = true;
        }

        if (has_dragged_)
        {
            // Autoscroll when dragging outside viewport vertically
            if (event->pos().y() < 0)
            {
                verticalScrollBar()->setValue(verticalScrollBar()->value() - 25);
            }
            else if (event->pos().y() > viewport()->height())
            {
                verticalScrollBar()->setValue(verticalScrollBar()->value() + 25);
            }

            if (const auto hit = char_at_pos(event->pos(), true))
            {
                sel_end_ = CharPosition{.paragraph = hit->first, .char_index = hit->second};
                viewport()->update();
            }
            return;
        }
    }

    if (const auto hit = char_at_pos(event->pos(), false))
    {
        if (const auto* span = token_at_char(hit->first, hit->second); span && session_)
        {
            session_->set_hovered_token(span->token_id);
            setCursor(Qt::PointingHandCursor);
            return;
        }
    }

    if (session_ && session_->hovered_token_id() != 0)
    {
        session_->clear_hover();
    }
    setCursor(Qt::IBeamCursor);
}

void TokenCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        if (is_selecting_)
        {
            is_selecting_ = false;

            if (!has_dragged_)
            {
                // Clicked on a token
                clear_selection();
                if (const auto hit = char_at_pos(event->pos(), false))
                {
                    if (const auto* span = token_at_char(hit->first, hit->second); span && session_)
                    {
                        session_->trigger_click(span->token_id);
                    }
                }
                else
                {
                    if (session_) session_->clear_active_token();
                }
            }
            viewport()->update();
        }
    }
}

void TokenCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    setFocus();
    if (event->button() == Qt::LeftButton)
    {
        if (const auto hit = char_at_pos(event->pos(), false))
        {
            if (const auto* span = token_at_char(hit->first, hit->second); span && span->length > 0)
            {
                sel_start_ = CharPosition{.paragraph = hit->first, .char_index = span->start_char};
                sel_end_ = CharPosition{.paragraph = hit->first, .char_index = span->start_char + span->length};
                if (session_)
                {
                    session_->trigger_click(span->token_id);
                }
                viewport()->update();
            }
        }
    }
}

void TokenCanvas::leaveEvent(QEvent* /*event*/)
{
    if (session_ && session_->hovered_token_id() != 0)
    {
        session_->clear_hover();
    }
}

void TokenCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    setFocus();
    if (!session_ || !session_->has_document()) return;
    const auto& doc = session_->document();

    // 1. If text is selected on this canvas
    if (has_selection())
    {
        const auto norm = normalized_selection();
        QString selected_text;

        for (size_t p = norm.start.paragraph; p <= norm.end.paragraph && p < layouts_.size(); ++p)
        {
            const auto& pl = layouts_[p];
            const int start_c = (p == norm.start.paragraph) ? norm.start.char_index : 0;
            const int end_c = (p == norm.end.paragraph) ? norm.end.char_index : static_cast<int>(pl.text.length());
            if (start_c < end_c && start_c < pl.text.length())
            {
                selected_text += pl.text.mid(start_c, end_c - start_c);
            }
        }

        // Check if any rule tokens are selected
        for (size_t p = norm.start.paragraph; p <= norm.end.paragraph && p < layouts_.size(); ++p)
        {
            const auto& pl = layouts_[p];
            const int start_c = (p == norm.start.paragraph) ? norm.start.char_index : 0;
            const int end_c = (p == norm.end.paragraph) ? norm.end.char_index : static_cast<int>(pl.text.length());

            for (const auto& span : pl.spans)
            {
                if (span.length > 0 && span.start_char < end_c && (span.start_char + span.length) > start_c)
                {
                    const auto* tok = doc->find_token(span.token_id);
                    if (tok && tok->is_rule())
                    {
                        session_->set_active_token(span.token_id);
                        session_->request_rule_popup(tok->rule);
                        return;
                    }
                }
            }
        }

        if (role_ == LanguageRole::Chinese)
        {
            session_->request_dict_popup(selected_text);
            return;
        }

        // Sino-Vietnamese: 1-to-1 word correspondence mapping to Chinese characters
        if (role_ == LanguageRole::SinoVietnamese)
        {
            QString chinese_text;
            for (size_t p = norm.start.paragraph; p <= norm.end.paragraph && p < layouts_.size(); ++p)
            {
                const auto& pl = layouts_[p];
                const int start_c = (p == norm.start.paragraph) ? norm.start.char_index : 0;
                const int end_c = (p == norm.end.paragraph) ? norm.end.char_index : static_cast<int>(pl.text.length());

                for (const auto& span : pl.spans)
                {
                    if (span.length > 0 && span.start_char < end_c && (span.start_char + span.length) > start_c)
                    {
                        const auto* tok = doc->find_token(span.token_id);
                        if (!tok) continue;

                        const int inter_start = std::max(start_c, span.start_char) - span.start_char;
                        const int inter_end = std::min(end_c, span.start_char + span.length) - span.start_char;
                        const QString span_text = pl.text.mid(span.start_char, span.length);
                        const QStringList words = span_text.split(u' ', Qt::SkipEmptyParts);

                        if (words.size() == tok->cn.length())
                        {
                            int cur = 0;
                            for (int w = 0; w < words.size(); ++w)
                            {
                                const int w_len = words[w].length();
                                const int w_start = cur;
                                const int w_end = cur + w_len;
                                if (w_end > inter_start && w_start < inter_end)
                                {
                                    chinese_text += tok->cn[w];
                                }
                                cur = w_end + 1;
                            }
                        }
                        else
                        {
                            chinese_text += tok->cn;
                        }
                    }
                }
            }

            if (!chinese_text.isEmpty())
            {
                session_->request_dict_popup(chinese_text);
                return;
            }
        }

        // Vietnamese: map selected tokens to their full Chinese text
        QString chinese_text;
        for (size_t p = norm.start.paragraph; p <= norm.end.paragraph && p < layouts_.size(); ++p)
        {
            const auto& pl = layouts_[p];
            const int start_c = (p == norm.start.paragraph) ? norm.start.char_index : 0;
            const int end_c = (p == norm.end.paragraph) ? norm.end.char_index : static_cast<int>(pl.text.length());

            for (const auto& span : pl.spans)
            {
                if (span.length > 0 && span.start_char < end_c && (span.start_char + span.length) > start_c)
                {
                    if (const auto* tok = doc->find_token(span.token_id))
                    {
                        chinese_text += tok->cn;
                    }
                }
            }
        }

        if (!chinese_text.isEmpty())
        {
            session_->request_dict_popup(chinese_text);
            return;
        }
    }

    // 2. No selection: right click under cursor
    if (const auto hit = char_at_pos(event->pos(), false))
    {
        if (const auto* span = token_at_char(hit->first, hit->second))
        {
            if (const auto* tok = doc->find_token(span->token_id))
            {
                session_->set_active_token(span->token_id);
                if (tok->is_rule())
                {
                    session_->request_rule_popup(tok->rule);
                    return;
                }
                session_->request_dict_popup(tok->cn);
                return;
            }
        }
    }
}

void TokenCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Copy))
    {
        copy_selection_to_clipboard();
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void TokenCanvas::focusInEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusInEvent(event);
    update();
}

void TokenCanvas::focusOutEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusOutEvent(event);
    update();
}

void TokenCanvas::scroll_to_token(const uint32_t token_id) const
{
    if (!session_ || !session_->has_document()) return;

    for (const auto & pl : layouts_)
    {
        for (const auto& span : pl.spans)
        {
            if (span.token_id == token_id)
            {
                if (!pl.layout_valid || !pl.text_layout) return;

                const qreal line_scale = line_height_percent_ / 100.0;
                for (int l = 0; l < pl.text_layout->lineCount(); ++l)
                {
                    QTextLine line = pl.text_layout->lineAt(l);
                    if (span.start_char >= line.textStart() &&
                        span.start_char < (line.textStart() + line.textLength()))
                    {
                        const qreal target_y = pl.y + line.y() * line_scale;
                        const int view_h = viewport()->height();

                        if (const int current_scroll = verticalScrollBar()->value(); target_y < current_scroll || target_y > (current_scroll + view_h - 40))
                        {
                            verticalScrollBar()->setValue(static_cast<int>(target_y - view_h / 3));
                        }
                        viewport()->update();
                        return;
                    }
                }
                return;
            }
        }
    }
}
