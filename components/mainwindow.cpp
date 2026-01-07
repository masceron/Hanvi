#include <QFileDialog>
#include <QTextBlock>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

#include "ui_MainWindow.h"
#include "../core/io.h"
#include "mainwindow.h"

#include "dictpopup.h"
#include "../core/converter.h"

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow)
{
    current_page = 0;
    setWindowState(Qt::WindowMaximized);
    ui->setupUi(this);

    const auto clipboardAction = new QAction("Reload", this);
    connect(clipboardAction, &QAction::triggered, this, [this]
    {
        if (!input_text.isEmpty())
        {
            convert_and_display();
        }
    });

    ui->menubar->addAction(clipboardAction);

    ui->left_right->setStretchFactor(0, 1);
    ui->left_right->setStretchFactor(1, 4);

    connect(ui->read_from_clipboard, &QAction::triggered, this, [this]
    {
        if (const auto input = load_from_clipboard(); input.has_value())
        {
            current_page = 0;
            input_text = input.value();
            pages = paginate(input_text, page_length);
            convert_and_display();
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setText("Cannot read text from clipboard.");
            msgBox.exec();
        }
    });

    connect(ui->read_from_file, &QAction::triggered, this, [this]
    {
        const auto name = QFileDialog::getOpenFileName(this, "Open file", "/", "Text files (*.txt)");
        if (name.isEmpty()) return;

        if (const auto input = load_from_file(name); input.has_value())
        {
            current_page = 0;
            input_text = input.value();
            pages = paginate(input_text, page_length);
            convert_and_display();
        }
        else if (input.error() == io_error::file_not_readable)
        {
            QMessageBox msgBox;
            msgBox.setText("Cannot open file.");
            msgBox.exec();
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setText("Cannot read text. Make sure the file is a text file.");
            msgBox.exec();
        }
    });

    connect(ui->vn_output, &QTextBrowser::anchorClicked, this, &MainWindow::click_token);
    connect(ui->cn_input, &QTextBrowser::anchorClicked, this, &MainWindow::click_token);

    connect(&watcher, &QFutureWatcher<std::pair<QString, QString>>::finished, this, &MainWindow::update_display);
    connect(&plain_watcher, &QFutureWatcher<QString>::finished, this, [this]
    {
        if (!save_to_file(file_name, plain_watcher.result()))
        {
            ui->statusbar->showMessage("File saved to " + file_name);
            ui->progress_bar->setValue(100);
            QDesktopServices::openUrl(QUrl("file:///" + file_name.left(file_name.lastIndexOf('/'))));
        }
        else ui->statusbar->showMessage("Some error occurred and the file were not saved.");
    });

    connect(ui->previous_page, &QPushButton::clicked, this, [this]
    {
        if (current_page > 0)
        {
            current_page--;
            convert_and_display();
        }
    });
    connect(ui->next_page, &QPushButton::clicked, this, [this]
    {
        if (current_page < pages.size() - 1) {
        current_page++;
        convert_and_display();
    }
    });

    ui->char_per_page->setValue(page_length);

    connect(ui->char_per_page, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](const int val)
    {
        current_page = 0;
        page_length = val;
        pages = paginate(input_text, page_length);
        convert_and_display();
    });

    ui->cn_input->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->cn_input, &QWidget::customContextMenuRequested, this, &MainWindow::open_popup);
    ui->vn_output->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->vn_output, &QWidget::customContextMenuRequested, this, &MainWindow::open_popup);

    connect(ui->save_to_file, &QAction::triggered, this, [this]
    {
        if (input_text.isEmpty()) return;

        file_name = QFileDialog::getSaveFileName(this, "Save to...", "/", "Text files (*.txt)");
        if (file_name.isEmpty()) return;

        convert_to_file(file_name);
    });
}

void MainWindow::update_pagination_controls() const
{
    if (const int total = static_cast<int>(pages.size()); total == 0)
    {
        ui->current_page->setText("Page 0 / 0");
        ui->previous_page->setEnabled(false);
        ui->next_page->setEnabled(false);
    }
    else
    {
        ui->current_page->setText(QString("Page %1 / %2").arg(current_page + 1).arg(total));

        ui->previous_page->setEnabled(current_page > 0);
        ui->next_page->setEnabled(current_page < total - 1);
    }
}

void MainWindow::click_token(const QUrl& link) const
{
    const QString token = link.toString();

    highlight_token(ui->cn_input, token);
    highlight_token(ui->vn_output, token);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::convert_and_display()
{
    if (!pages[current_page].isEmpty())
    {
        ui->statusbar->showMessage("Converting...");

        auto reporter = [this](int progress)
        {
            QMetaObject::invokeMethod(this, [this, progress]
            {
                ui->progress_bar->setValue(static_cast<int>((progress * 100) / pages[current_page].length()));
            });
        };

        const QFuture<std::pair<QString, QString>> future = QtConcurrent::run(
            convert, pages[current_page], reporter);
        watcher.setFuture(future);
    }
}

void MainWindow::convert_to_file(const QString& name)
{
    if (!input_text.isEmpty())
    {
        ui->statusbar->showMessage("Saving to file...");

        auto reporter = [this](int progress)
        {
            QMetaObject::invokeMethod(this, [this, progress]
            {
                ui->progress_bar->setValue(static_cast<int>((progress * 100) / input_text.length()));
            });
        };

        const QFuture<QString> future = QtConcurrent::run(
            convert_plain, input_text, reporter);
        plain_watcher.setFuture(future);
    }
}

void MainWindow::highlight_token(QTextBrowser* browser, const QString& token)
{
    browser->setCursorWidth(0);

    QTextCursor clear_cursor = browser->textCursor();
    clear_cursor.clearSelection();
    browser->setTextCursor(clear_cursor);

    const QTextCursor cursor = find_token(browser->document(), token);

    if (cursor.isNull())
    {
        browser->setExtraSelections({});
        return;
    }

    QTextEdit::ExtraSelection selection;

    selection.format.setForeground(Qt::red);
    selection.format.setFontWeight(QFont::Bold);

    selection.format.setBackground(Qt::transparent);
    selection.cursor = cursor;

    browser->setExtraSelections({selection});

    auto pan = QTextCursor(browser->document());
    pan.setPosition(cursor.position());
    browser->setTextCursor(pan);
}

QTextCursor MainWindow::find_token(QTextDocument* document, const QString& token)
{
    QTextCursor cursor(document);
    int start_pos = -1;
    int end_pos = -1;
    bool found = false;

    QTextBlock block = document->begin();
    while (block.isValid())
    {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it)
        {
            QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) continue;
            QTextCharFormat format = fragment.charFormat();

            if (!found)
            {
                if (format.isAnchor() && format.anchorHref() == token)
                {
                    start_pos = fragment.position();
                    found = true;
                    end_pos = fragment.position() + fragment.length();
                }
            }
            else
            {
                if (format.anchorHref() == token)
                {
                    end_pos = fragment.position() + fragment.length();
                }
                else
                {
                    cursor.setPosition(start_pos);
                    cursor.setPosition(end_pos, QTextCursor::KeepAnchor);
                    return cursor;
                }
            }
        }

        if (found)
        {
            cursor.setPosition(start_pos);
            cursor.setPosition(end_pos, QTextCursor::KeepAnchor);
            return cursor;
        }

        block = block.next();
    }
    return {};
}

QString MainWindow::token_id_at(const QTextBrowser* browser, const int position)
{
    QTextCursor cursor = browser->textCursor();
    cursor.setPosition(position);

    const QTextCharFormat format = cursor.charFormat();
    return format.anchorHref();
}

void MainWindow::update_display()
{
    update_pagination_controls();
    ui->progress_bar->setValue(100);
    const auto [cn_out, vn_out] = watcher.result();
    ui->statusbar->showMessage("Conversion completed.");
    ui->cn_input->setHtml(cn_out);
    ui->vn_output->setHtml(vn_out);

    if (saved_cursor_pos != -1)
    {
        QTextDocument* doc = ui->cn_input->document();
        int target_pos = qMin(saved_cursor_pos, doc->characterCount() - 1);
        if (target_pos < 0) target_pos = 0;

        QTextCursor restoration_cursor(doc);
        restoration_cursor.setPosition(target_pos);
        restoration_cursor.movePosition(QTextCursor::NextCharacter);

        if (const QString anchor_name = restoration_cursor.charFormat().anchorHref(); !anchor_name.isEmpty())
        {
            highlight_token(ui->cn_input, anchor_name);
            highlight_token(ui->vn_output, anchor_name);
        }
        saved_cursor_pos = -1;
    }
}

void MainWindow::snap_selection_to_token(QTextBrowser* browser)
{
    QTextCursor cursor = browser->textCursor();
    if (!cursor.hasSelection()) return;

    const int start_pos = cursor.selectionStart();
    const int end_pos = cursor.selectionEnd();

    const QString start_id = token_id_at(browser, start_pos);

    const QString end_id = token_id_at(browser, end_pos - 1);

    int new_start = start_pos;
    int new_end = end_pos;

    if (!start_id.isEmpty()) {
        if (const QTextCursor token_cursor = find_token(browser->document(), start_id); !token_cursor.isNull()) {
            new_start = token_cursor.selectionStart();
        }
    }

    if (!end_id.isEmpty()) {
        if (const QTextCursor token_cursor = find_token(browser->document(), end_id); !token_cursor.isNull()) {
            new_end = token_cursor.selectionEnd();
        }
    }

    if (new_start != start_pos || new_end != end_pos) {
        cursor.setPosition(new_start);
        cursor.setPosition(new_end, QTextCursor::KeepAnchor);
        browser->setTextCursor(cursor);
    }
}

QString MainWindow::get_chinese_text_from_ids(const QStringList& ids) const
{
    QString result;

    for (const QString& id : ids) {
        if (QTextCursor cursor = find_token(ui->cn_input->document(), id); !cursor.isNull()) {
            result.append(cursor.selectedText());
        }
    }
    return result;
}

void MainWindow::open_popup()
{
    const auto sender_browser = qobject_cast<QTextBrowser*>(sender());
    if (!sender_browser) return;

    if (sender_browser == ui->cn_input)
    {
        saved_cursor_pos = sender_browser->textCursor().selectionStart();
    }
    else if (sender_browser == ui->vn_output)
    {
        const QTextCursor vn_cursor = sender_browser->textCursor();
        const int click_pos = vn_cursor.selectionStart();
        QTextCursor hit_cursor = sender_browser->textCursor();
        hit_cursor.setPosition(click_pos);

        if (const QString token_id = hit_cursor.charFormat().anchorHref(); !token_id.isEmpty())
        {
            if (const QTextCursor cn_cursor = find_token(ui->cn_input->document(), token_id); !cn_cursor.isNull()) {
                saved_cursor_pos = cn_cursor.selectionStart();
            }
        }
    }

    QString selected_chinese_text;

    if (sender_browser == ui->vn_output)
    {
        snap_selection_to_token(sender_browser);

        const QTextCursor cursor = sender_browser->textCursor();
        const int start = cursor.selectionStart();
        const int end = cursor.selectionEnd();

        QStringList token_ids;

        QTextBlock block = sender_browser->document()->findBlock(start);
        while (block.isValid() && block.position() < end) {
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                QTextFragment fragment = it.fragment();
                const int frag_start = fragment.position();

                if (const int frag_end = frag_start + fragment.length(); frag_end > start && frag_start < end) {
                    if (QString id = fragment.charFormat().anchorHref(); !id.isEmpty() && !token_ids.contains(id)) {
                        token_ids.append(id);
                    }
                }
            }
            block = block.next();
        }

        selected_chinese_text = get_chinese_text_from_ids(token_ids);
    }
    else
    {
        selected_chinese_text = sender_browser->textCursor().selectedText();
    }

    if (selected_chinese_text.isEmpty()) return;

    auto *popup = new DictPopup(this);

    popup->load_data(selected_chinese_text);

    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->exec();
    convert_and_display();
}
