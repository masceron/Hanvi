#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QScrollBar>
#include <QShortcut>
#include <QtConcurrentRun>
#include <QTimer>

#include "ui_MainWindow.h"
#include "../core/io.h"
#include "mainwindow.h"
#include "rulepopup.h"
#include "dictpopup.h"
#include "namesetsmanager.h"
#include "namesetchooser.h"
#include "../core/converter.h"
#include "app/app.h"
#include "core/dict.h"

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow), session(new DocumentSession(this))
{
    current_page = 0;
    setWindowState(Qt::WindowMaximized);
    ui->setupUi(this);
    setWindowTitle("Hanvi");

    ui->cn_input->set_session(session);
    ui->cn_input->set_role(LanguageRole::Chinese);
    QFont cn_font("Noto Sans SC");
    cn_font.setStyleHint(QFont::SansSerif);
    cn_font.setPixelSize(18);
    ui->cn_input->set_font(cn_font);
    ui->cn_input->set_line_height_percent(100);

    ui->sv_output->set_session(session);
    ui->sv_output->set_role(LanguageRole::SinoVietnamese);
    QFont sv_font("Tahoma");
    sv_font.setStyleHint(QFont::SansSerif);
    sv_font.setPixelSize(16);
    ui->sv_output->set_font(sv_font);
    ui->sv_output->set_line_height_percent(110);

    ui->vn_output->set_session(session);
    ui->vn_output->set_role(LanguageRole::Vietnamese);
    QFont vn_font("Tahoma");
    vn_font.setStyleHint(QFont::SansSerif);
    vn_font.setPixelSize(16);
    ui->vn_output->set_font(vn_font);
    ui->vn_output->set_line_height_percent(125);

    connect(session, &DocumentSession::request_dict_popup, this, &MainWindow::on_request_dict_popup);
    connect(session, &DocumentSession::request_rule_popup, this, &MainWindow::on_request_rule_popup);

    auto* name_set_manager = ui->menubar->addAction("Namesets");
    connect(name_set_manager, &QAction::triggered, this, [this]
    {
        auto* manager = new NamesetsManager(this);
        manager->setAttribute(Qt::WA_DeleteOnClose);
        manager->exec();
        load_data();
    });

    ui->menubar->addAction(name_set_manager);

    auto* reload_action = ui->menubar->addAction("Re-convert");
    reload_action->setShortcut(QKeySequence("Ctrl+R"));
    connect(reload_action, &QAction::triggered, this, [this]
    {
        if (!input_text.isEmpty())
        {
            convert_and_display(true);
        }
    });
    ui->menubar->addAction(reload_action);

    auto* reload_data_action = ui->menubar->addAction("Reload dict");
    reload_data_action->setShortcut(QKeySequence("Ctrl+Shift+R"));
    connect(reload_data_action, &QAction::triggered, this, [this, reload_data_action]
    {
        ui->progress_bar->setValue(0);
        ui->statusbar->showMessage("Reloading dictionary...");
        reload_data_action->setEnabled(false);
        QCoreApplication::processEvents();
        reload_dict([this, reload_data_action]
        {
            convert_and_display(true);
            reload_data_action->setEnabled(true);
        });
    });
    ui->menubar->addAction(reload_data_action);

    ui->left_right->setStretchFactor(0, 1);
    ui->left_right->setStretchFactor(1, 4);

    connect(ui->read_from_clipboard, &QAction::triggered, this, [this]
    {
        if (const auto input = load_from_clipboard(); input.has_value())
        {
            current_page = 0;
            input_text = input.value();
            pages = paginate(input_text, page_length);
            convert_and_display(false);
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
            convert_and_display(false);
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

    connect(&watcher, &QFutureWatcher<std::shared_ptr<AlignedDocument>>::finished, this,
            &MainWindow::update_display);
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
            convert_and_display(false);
        }
    });
    connect(ui->next_page, &QPushButton::clicked, this, [this]
    {
        if (current_page < pages.size() - 1)
        {
            current_page++;
            convert_and_display(false);
        }
    });

    ui->char_per_page->setValue(page_length);

    connect(ui->char_per_page, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](const int val)
    {
        current_page = 0;
        page_length = val;
        pages = paginate(input_text, page_length);
        convert_and_display(false);
    });

    connect(ui->save_to_file, &QAction::triggered, this, [this]
    {
        if (input_text.isEmpty()) return;

        file_name = QFileDialog::getSaveFileName(this, "Save to...", "/", "Text files (*.txt)");
        if (file_name.isEmpty()) return;

        convert_to_file();
    });

    connect(ui->current_name_set, &QPushButton::clicked, this, [this]
    {
        auto* chooser = new namesetchooser(this);
        chooser->setAttribute(Qt::WA_DeleteOnClose);
        if (chooser->exec() == QDialog::Accepted)
        {
            if (const int change_to = chooser->get_chosen_id(); change_to != -2 && change_to != current_name_set_id)
            {
                load_name_set(change_to);
                convert_and_display(true);
                load_data();
            }
        }
    });

    ui->current_page->setValidator(new QIntValidator(1, 9999, this));
    connect(ui->current_page, &QLineEdit::editingFinished, this, [&]
    {
        const auto target = std::clamp(ui->current_page->text().toInt(), 1, static_cast<int>(pages.size()));
        current_page = target - 1;
        convert_and_display(false);
    });

    const auto quit_shortcut = new QShortcut(QKeySequence("Ctrl+W"), this);
    connect(quit_shortcut, &QShortcut::activated, this, []
    {
        QApplication::quit();
    });
}

void MainWindow::load_data()
{
    bool found = false;
    for (const auto& [index, title] : name_sets)
    {
        if (index == current_name_set_id)
        {
            ui->current_name_set->setText(title);
            found = true;
            break;
        }
    }

    if (!found)
    {
        ui->current_name_set->setText("None");
        if (current_name_set_id != -1)
        {
            load_name_set(-1);
            convert_and_display(true);
        }
    }
}

void MainWindow::update_pagination_controls() const
{
    if (const int total = static_cast<int>(pages.size()); total == 0)
    {
        ui->current_page->setText("0");
        ui->current_page->setEnabled(false);
        ui->total_page->setText("0");
        ui->previous_page->setEnabled(false);
        ui->next_page->setEnabled(false);
    }
    else
    {
        ui->current_page->setEnabled(total > 1);
        ui->current_page->setText(QString::number(current_page + 1));
        ui->total_page->setText(QString::number(total));
        ui->previous_page->setEnabled(current_page > 0);
        ui->next_page->setEnabled(current_page < total - 1);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::convert_and_display(const bool scroll_back)
{
    if (watcher.isRunning())
    {
        return;
    }
    if (!input_text.isEmpty() && !pages[current_page].isEmpty())
    {
        ui->statusbar->showMessage("Converting...");

        if (scroll_back)
        {
            saved_scroll = {
                ui->cn_input->scroll_value(),
                ui->sv_output->scroll_value(),
                ui->vn_output->scroll_value()
            };
        }
        else saved_scroll = {.cn = 0, .sv = 0, .vn = 0};

        auto reporter = [this](int progress)
        {
            QMetaObject::invokeMethod(this, [this, progress]
            {
                ui->progress_bar->setValue(static_cast<int>((progress * 100) / pages[current_page].length()));
            });
        };

        const QFuture<std::shared_ptr<AlignedDocument>> future = QtConcurrent::run(
            convert, pages[current_page], reporter);
        watcher.setFuture(future);
    }
}

void MainWindow::convert_to_file()
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

void MainWindow::update_display()
{
    update_pagination_controls();
    ui->progress_bar->setValue(100);
    const auto doc = watcher.result();
    ui->statusbar->showMessage("Conversion completed.");

    session->set_document(doc);

    ui->cn_input->set_scroll_value(saved_scroll.cn);
    ui->sv_output->set_scroll_value(saved_scroll.sv);
    ui->vn_output->set_scroll_value(saved_scroll.vn);
    saved_scroll = {0, 0, 0};

    if (!saved_token_cn.isEmpty())
    {
        for (const auto& para : doc->paragraphs)
        {
            for (const auto& tok : para.tokens)
            {
                if (tok.cn == saved_token_cn || tok.cn.contains(saved_token_cn))
                {
                    session->set_active_token(tok.id);
                    goto token_found;
                }
            }
        }
    token_found:
        saved_token_cn.clear();
    }
}

void MainWindow::on_request_dict_popup(const QString& chinese_text)
{
    if (chinese_text.isEmpty()) return;

    auto* popup = new DictPopup(this);
    popup->load_data(chinese_text);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    if (popup->exec())
    {
        saved_token_cn = chinese_text;
        convert_and_display(true);
    }
}

void MainWindow::on_request_rule_popup(const Rule* rule)
{
    if (!rule) return;

    auto* popup = new RulePopup(this);
    popup->load_data(rule);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->exec();
}
