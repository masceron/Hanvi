#include <QFileDialog>
#include <QTextBlock>
#include <QMessageBox>

#include "ui_MainWindow.h"
#include "../text/io.h"
#include "mainwindow.h"
#include "../convert/converter.h"

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

    ui->cn_vocab->setStretchFactor(0, 2);
    ui->cn_vocab->setStretchFactor(1, 1);

    connect(ui->actionFrom_clipboard, &QAction::triggered, this, [this]
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

    connect(ui->actionFrom_file, &QAction::triggered, this, [this]
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
        page_length = val;
        pages = paginate(input_text, page_length);
        convert_and_display();
    });
}

void MainWindow::update_pagination_controls() const
{
    if (const int total = pages.size(); total == 0)
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
                ui->progress_bar->setValue((progress * 100) / pages[current_page].length());
            });
        };

        const QFuture<std::pair<QString, QString>> future = QtConcurrent::run(
            convert, pages[current_page], reporter);
        watcher.setFuture(future);
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

void MainWindow::update_display() const
{
    update_pagination_controls();
    ui->progress_bar->setValue(100);
    const auto [cn_out, vn_out] = watcher.result();
    ui->statusbar->showMessage("Conversion completed.");
    ui->cn_input->setHtml(cn_out);
    ui->vn_output->setHtml(vn_out);
}
