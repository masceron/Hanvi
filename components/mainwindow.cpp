#include "mainwindow.h"

#include <QFileDialog>
#include <QTextBlock>

#include "ui_MainWindow.h"
#include "../text/convert.h"
#include "../text/io.h"

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow)
{
    setWindowState(Qt::WindowMaximized);
    ui->setupUi(this);

    const auto clipboardAction = new QAction("Reload", this);
    connect(clipboardAction, &QAction::triggered, this, [this]
    {
        convert_and_display();
    });

    ui->menubar->addAction(clipboardAction);

    ui->main->setStretchFactor(0, 1);
    ui->main->setStretchFactor(1, 4);

    ui->left_col->setStretchFactor(0, 2);
    ui->left_col->setStretchFactor(1, 1);

    connect(ui->actionFrom_clipboard, &QAction::triggered, this, [this]
    {
        load_from_clipboard();
        convert_and_display();
    });

    connect(ui->actionFrom_file, &QAction::triggered, this, [this]
    {
        const auto name = QFileDialog::getOpenFileName(this, "Open file", "/", "Text files (*.txt)");
        load_from_file(name);
        convert_and_display();
    });

    connect(ui->vn_output, &QTextBrowser::anchorClicked, this, &MainWindow::click_token);
    connect(ui->cn_input, &QTextBrowser::anchorClicked, this, &MainWindow::click_token);
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

void MainWindow::convert_and_display() const
{
    if (!input.isEmpty())
    {
        convert();
        ui->cn_input->setHtml(output.first);
        ui->vn_output->setHtml(output.second);
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
    return QTextCursor();
}
