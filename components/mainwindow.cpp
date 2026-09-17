#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QScrollBar>
#include <QShortcut>
#include <QtConcurrentRun>
#include <QTimer>
#include <QToolButton>
#include <QFileInfo>
#include <QIcon>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWindow>
#include <QMenu>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

#include "ui_MainWindow.h"
#include "../core/io.h"
#include "mainwindow.h"
#include "rulepopup.h"
#include "dictpopup.h"
#include "namesetsmanager.h"
#include "namesetchooser.h"
#include "../core/converter.h"
#include "core/dict.h"
#include "documents/token_canvas.h"

namespace
{
    class HanviTabBar : public QTabBar
    {
    public:
        explicit HanviTabBar(QWidget* parent = nullptr) : QTabBar(parent)
        {
        }

        [[nodiscard]] QSize sizeHint() const override
        {
            if (count() == 0)
            {
                return {0, 32};
            }

            int total_width = 0;
            for (int i = 0; i < count(); ++i)
            {
                const int w = tabRect(i).isValid() ? tabRect(i).width() : 0;
                total_width += std::max(w, tabSizeHint(i).width());
            }
            return {total_width + 8, 32};
        }

        [[nodiscard]] QSize minimumSizeHint() const override
        {
            return {60, 32};
        }

    protected:
        void tabInserted(const int index) override
        {
            QTabBar::tabInserted(index);
            updateGeometry();
        }

        void tabRemoved(const int index) override
        {
            QTabBar::tabRemoved(index);
            updateGeometry();
        }

        void tabLayoutChange() override
        {
            QTabBar::tabLayoutChange();
            updateGeometry();
        }
    };
}

void NovelTab::set_text(QString text)
{
    input_text = std::move(text);
    repaginate();
}

void NovelTab::repaginate()
{
    pages = paginate(input_text, page_length);
    if (current_page >= pages.size())
    {
        current_page = std::max(0, static_cast<int>(pages.size()) - 1);
    }
}

NovelTab* MainWindow::current_tab() noexcept
{
    if (active_tab_index >= 0 && active_tab_index < static_cast<int>(tabs.size()))
    {
        return tabs[active_tab_index].get();
    }
    return nullptr;
}

const NovelTab* MainWindow::current_tab() const noexcept
{
    if (active_tab_index >= 0 && active_tab_index < static_cast<int>(tabs.size()))
    {
        return tabs[active_tab_index].get();
    }
    return nullptr;
}

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow), session(new DocumentSession(this))
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    ui->setupUi(this);
    setWindowTitle("Hanvi");

#if defined(Q_OS_WIN)
    const auto hwnd = reinterpret_cast<HWND>(winId());
    const DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    SetWindowLong(hwnd, GWL_STYLE, WS_THICKFRAME | style | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
    constexpr MARGINS shadow = {.cxLeftWidth = 1, .cxRightWidth = 1, .cyTopHeight = 1, .cyBottomHeight = 1};
    DwmExtendFrameIntoClientArea(hwnd, &shadow);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
#endif

    setWindowState(Qt::WindowMaximized);

    ui->verticalLayout->setContentsMargins(0, 0, 0, 0);
    ui->verticalLayout->setSpacing(0);
    ui->bottom_bar->setContentsMargins(8, 4, 8, 6);

    tab_container = new QWidget(this);
    tab_container->setObjectName("tab_container");
    tab_container->setFixedHeight(40);
    tab_container->installEventFilter(this);
    qApp->installEventFilter(this);
    auto* tab_layout = new QHBoxLayout(tab_container);
    tab_layout->setContentsMargins(8, 0, 0, 0);
    tab_layout->setSpacing(2);

    auto* hamburger_btn = new QPushButton(tab_container);
    hamburger_btn->setObjectName("hamburger_btn");
    hamburger_btn->setIcon(QIcon(":/resources/hamburger.svg"));
    hamburger_btn->setIconSize(QSize(16, 16));
    hamburger_btn->setFixedSize(32, 32);
    hamburger_btn->setCursor(Qt::PointingHandCursor);
    hamburger_btn->setToolTip("Menu");
    setup_hamburger_menu(hamburger_btn);

    tab_bar = new HanviTabBar(tab_container);
    tab_bar->setTabsClosable(false);
    tab_bar->setMovable(true);
    tab_bar->setDrawBase(false);
    tab_bar->setExpanding(false);
    tab_bar->setUsesScrollButtons(true);
    tab_bar->setElideMode(Qt::ElideRight);
    tab_bar->setFixedHeight(32);
    tab_bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* add_tab_btn = new QPushButton(tab_container);
    add_tab_btn->setObjectName("add_tab_btn");
    QIcon add_icon;
    add_icon.addFile(":/resources/add.svg", QSize(14, 14), QIcon::Normal);
    add_icon.addFile(":/resources/add_hover.svg", QSize(14, 14), QIcon::Active);
    add_tab_btn->setIcon(add_icon);
    add_tab_btn->setIconSize(QSize(14, 14));
    add_tab_btn->setFixedSize(32, 32);
    add_tab_btn->setCursor(Qt::PointingHandCursor);
    add_tab_btn->setToolTip("New tab (Ctrl+T)");

    auto* min_btn = new QPushButton(tab_container);
    min_btn->setObjectName("window_min_btn");
    min_btn->setIcon(QIcon(":/resources/window_min.svg"));
    min_btn->setIconSize(QSize(16, 16));
    min_btn->setFixedSize(44, 40);
    min_btn->setToolTip("Minimize");
    connect(min_btn, &QPushButton::clicked, this, &MainWindow::showMinimized);

    max_btn = new QPushButton(tab_container);
    max_btn->setObjectName("window_max_btn");
    max_btn->setIconSize(QSize(16, 16));
    max_btn->setFixedSize(44, 40);
    update_max_restore_button();
    connect(max_btn, &QPushButton::clicked, this, [this]
    {
        if (isMaximized()) showNormal();
        else showMaximized();
    });

    auto* close_win_btn = new QPushButton(tab_container);
    close_win_btn->setObjectName("window_close_btn");
    QIcon win_close_icon;
    win_close_icon.addFile(":/resources/window_close.svg", QSize(16, 16), QIcon::Normal);
    win_close_icon.addFile(":/resources/window_close_hover.svg", QSize(16, 16), QIcon::Active);
    close_win_btn->setIcon(win_close_icon);
    close_win_btn->setIconSize(QSize(16, 16));
    close_win_btn->setFixedSize(44, 40);
    close_win_btn->setToolTip("Close");
    connect(close_win_btn, &QPushButton::clicked, this, &MainWindow::close);

    tab_layout->addWidget(hamburger_btn);
    tab_layout->addWidget(tab_bar, 0, Qt::AlignVCenter);
    tab_layout->addWidget(add_tab_btn);
    tab_layout->addStretch(1);
    tab_layout->addWidget(min_btn);
    tab_layout->addWidget(max_btn);
    tab_layout->addWidget(close_win_btn);

    ui->verticalLayout->insertWidget(0, tab_container);

    connect(tab_bar, &QTabBar::currentChanged, this, [this](int index)
    {
        switch_to_tab(index);
    });

    connect(tab_bar, &QTabBar::tabMoved, this, [this](int from, int to)
    {
        if (from == to || from < 0 || to < 0 ||
            from >= static_cast<int>(tabs.size()) || to >= static_cast<int>(tabs.size()))
        {
            return;
        }

        auto moving = std::move(tabs[from]);
        tabs.erase(tabs.begin() + from);
        tabs.insert(tabs.begin() + to, std::move(moving));

        active_tab_index = tab_bar->currentIndex();

        if (converting_tab_index == from)
        {
            converting_tab_index = to;
        }
        else if (from < converting_tab_index && to >= converting_tab_index)
        {
            converting_tab_index--;
        }
        else if (from > converting_tab_index && to <= converting_tab_index)
        {
            converting_tab_index++;
        }
    });

    connect(add_tab_btn, &QPushButton::clicked, this, [this]
    {
        create_new_tab("Untitled");
    });

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

    ui->left_right->setStretchFactor(0, 1);
    ui->left_right->setStretchFactor(1, 4);

    connect(ui->read_from_clipboard, &QAction::triggered, this, [this]
    {
        if (const auto input = load_from_clipboard(); input.has_value())
        {
            auto* cur = current_tab();
            if (!cur)
            {
                create_new_tab("Clipboard", input.value(), "");
                return;
            }
            cur->title = "Clipboard";
            cur->file_name.clear();
            cur->current_page = 0;
            cur->current_doc = nullptr;
            cur->set_text(input.value());
            tab_bar->setTabText(active_tab_index, cur->title);
            tab_bar->setTabToolTip(active_tab_index, "");
            tab_bar->updateGeometry();
            setWindowTitle(cur->title + " - Hanvi");
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
            const QFileInfo fi(name);
            const QString title = fi.fileName();
            if (auto* cur = current_tab(); cur && cur->input_text.isEmpty())
            {
                cur->title = title;
                cur->file_name = name;
                cur->current_page = 0;
                cur->current_doc = nullptr;
                cur->set_text(input.value());
                tab_bar->setTabText(active_tab_index, title);
                tab_bar->setTabToolTip(active_tab_index, name);
                tab_bar->updateGeometry();
                setWindowTitle(title + " - Hanvi");
                convert_and_display(false);
            }
            else
            {
                create_new_tab(title, input.value(), name);
            }
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
        auto* cur = current_tab();
        const QString save_path = cur ? cur->file_name : "";
        if (!save_path.isEmpty() && !save_to_file(save_path, plain_watcher.result()))
        {
            ui->statusbar->showMessage("File saved to " + save_path);
            ui->progress_bar->setValue(100);
            QDesktopServices::openUrl(QUrl("file:///" + save_path.left(save_path.lastIndexOf('/'))));
        }
        else ui->statusbar->showMessage("Some error occurred and the file were not saved.");
    });

    connect(ui->previous_page, &QPushButton::clicked, this, [this]
    {
        auto* cur = current_tab();
        if (cur && cur->current_page > 0)
        {
            cur->current_page--;
            cur->current_doc = nullptr;
            convert_and_display(false);
        }
    });
    connect(ui->next_page, &QPushButton::clicked, this, [this]
    {
        auto* cur = current_tab();
        if (cur && cur->current_page < cur->pages.size() - 1)
        {
            cur->current_page++;
            cur->current_doc = nullptr;
            convert_and_display(false);
        }
    });

    connect(ui->char_per_page, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](const int val)
    {
        if (auto* cur = current_tab())
        {
            cur->current_page = 0;
            cur->page_length = val;
            cur->repaginate();
            cur->current_doc = nullptr;
            convert_and_display(false);
        }
    });

    connect(ui->save_to_file, &QAction::triggered, this, [this]
    {
        auto* cur = current_tab();
        if (!cur || cur->input_text.isEmpty()) return;

        const QString initial_dir = cur->file_name.isEmpty() ? "/" : cur->file_name;
        const QString name = QFileDialog::getSaveFileName(this, "Save to...", initial_dir, "Text files (*.txt)");
        if (name.isEmpty()) return;

        cur->file_name = name;
        convert_to_file();
    });

    connect(ui->current_name_set, &QPushButton::clicked, this, [this]
    {
        auto* cur = current_tab();
        if (!cur) return;

        auto* chooser = new namesetchooser(this);
        chooser->setAttribute(Qt::WA_DeleteOnClose);
        if (chooser->exec() == QDialog::Accepted)
        {
            if (const int change_to = chooser->get_chosen_id(); change_to != -2 && change_to != cur->name_set_id)
            {
                cur->name_set_id = change_to;
                load_name_set(change_to);
                cur->current_doc = nullptr;
                convert_and_display(true);
                load_data();
            }
        }
    });

    ui->current_page->setValidator(new QIntValidator(1, 9999, this));
    connect(ui->current_page, &QLineEdit::editingFinished, this, [this]
    {
        auto* cur = current_tab();
        if (cur && !cur->pages.isEmpty())
        {
            const auto target = std::clamp(ui->current_page->text().toInt(), 1, static_cast<int>(cur->pages.size()));
            cur->current_page = target - 1;
            cur->current_doc = nullptr;
            convert_and_display(false);
        }
    });

    ui->read_from_file->setShortcut(QKeySequence());
    ui->read_from_clipboard->setShortcut(QKeySequence());
    ui->save_to_file->setShortcut(QKeySequence());


    create_new_tab("Untitled");
}

void MainWindow::setup_hamburger_menu(QPushButton* btn)
{
    auto* menu = new QMenu(this);
    menu->setObjectName("hamburger_menu");

    auto* open_action = menu->addAction("Open File...");
    open_action->setShortcut(QKeySequence("Ctrl+O"));
    open_action->setShortcutContext(Qt::WindowShortcut);
    this->addAction(open_action);
    connect(open_action, &QAction::triggered, ui->read_from_file, &QAction::trigger);

    auto* paste_action = menu->addAction("Read from Clipboard");
    paste_action->setShortcut(QKeySequence("Ctrl+V"));
    paste_action->setShortcutContext(Qt::WindowShortcut);
    this->addAction(paste_action);
    connect(paste_action, &QAction::triggered, ui->read_from_clipboard, &QAction::trigger);

    auto* save_action = menu->addAction("Save to File...");
    save_action->setShortcut(QKeySequence("Ctrl+S"));
    save_action->setShortcutContext(Qt::WindowShortcut);
    this->addAction(save_action);
    connect(save_action, &QAction::triggered, ui->save_to_file, &QAction::trigger);

    auto* copy_vn_action = menu->addAction("Copy Vietnamese");
    copy_vn_action->setShortcut(QKeySequence("Ctrl+Shift+C"));
    copy_vn_action->setShortcutContext(Qt::WindowShortcut);
    this->addAction(copy_vn_action);
    connect(copy_vn_action, &QAction::triggered, this, [this]
    {
        ui->vn_output->copy_all_to_clipboard();
        ui->statusbar->showMessage("Vietnamese text copied to clipboard.", 2000);
    });

    menu->addSeparator();


    const auto* nameset_action = menu->addAction("Namesets Manager...");
    connect(nameset_action, &QAction::triggered, this, [this]
    {
        auto* manager = new NamesetsManager(this);
        manager->setAttribute(Qt::WA_DeleteOnClose);
        manager->exec();
        load_data();
    });

    auto* reconvert_action = menu->addAction("Re-convert");
    reconvert_action->setShortcut(QKeySequence("Ctrl+R"));
    reconvert_action->setShortcutContext(Qt::WindowShortcut);
    this->addAction(reconvert_action);
    connect(reconvert_action, &QAction::triggered, this, [this]
    {
        if (auto* cur = current_tab(); cur && !cur->input_text.isEmpty())
        {
            cur->current_doc = nullptr;
            convert_and_display(true);
        }
    });

    auto* reload_dict_action = menu->addAction("Reload Dictionary");
    reload_dict_action->setShortcut(QKeySequence("Ctrl+Shift+R"));
    reload_dict_action->setShortcutContext(Qt::WindowShortcut);
    this->addAction(reload_dict_action);
    connect(reload_dict_action, &QAction::triggered, this, [this, reload_dict_action]
    {
        ui->progress_bar->setValue(0);
        ui->statusbar->showMessage("Reloading dictionary...");
        reload_dict_action->setEnabled(false);
        QCoreApplication::processEvents();
        reload_dict([this, reload_dict_action]
        {
            if (auto* cur = current_tab())
            {
                cur->current_doc = nullptr;
            }
            convert_and_display(true);
            reload_dict_action->setEnabled(true);
        });
    });

    menu->addSeparator();

    auto* new_tab_action = menu->addAction("New Tab");
    new_tab_action->setShortcut(QKeySequence("Ctrl+T"));
    new_tab_action->setShortcutContext(Qt::WindowShortcut);
    this->addAction(new_tab_action);
    connect(new_tab_action, &QAction::triggered, this, [this]
    {
        create_new_tab("Untitled");
    });

    auto* close_tab_action = menu->addAction("Close Tab");
    close_tab_action->setShortcut(QKeySequence("Ctrl+W"));
    close_tab_action->setShortcutContext(Qt::WindowShortcut);
    this->addAction(close_tab_action);
    connect(close_tab_action, &QAction::triggered, this, [this]
    {
        close_tab(active_tab_index);
    });

    menu->addSeparator();

    auto* exit_action = menu->addAction("Exit");
    exit_action->setShortcut(QKeySequence("Alt+F4"));
    exit_action->setShortcutContext(Qt::WindowShortcut);
    this->addAction(exit_action);
    connect(exit_action, &QAction::triggered, this, &MainWindow::close);

    connect(btn, &QPushButton::clicked, this, [btn, menu]
    {
        menu->popup(btn->mapToGlobal(QPoint(0, btn->height())));
    });
}

void MainWindow::update_max_restore_button() const
{
    if (!max_btn) return;
    QIcon icon;
    if (isMaximized())
    {
        icon.addFile(":/resources/window_restore.svg");
        max_btn->setToolTip("Restore Down");
    }
    else
    {
        icon.addFile(":/resources/window_max.svg");
        max_btn->setToolTip("Maximize");
    }
    max_btn->setIcon(icon);
    max_btn->setIconSize(QSize(16, 16));
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        update_max_restore_button();
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        if (const auto* ke = dynamic_cast<QKeyEvent*>(event); ke->modifiers().testFlag(Qt::ControlModifier) && !
            QApplication::activeModalWidget())
        {
            if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab)
            {
                if (tabs.size() > 1)
                {
                    if (ke->modifiers().testFlag(Qt::ShiftModifier) || ke->key() == Qt::Key_Backtab)
                    {
                        const int prev = (active_tab_index - 1 + static_cast<int>(tabs.size())) % static_cast<int>(tabs.
                            size());
                        tab_bar->setCurrentIndex(prev);
                    }
                    else
                    {
                        const int next = (active_tab_index + 1) % static_cast<int>(tabs.size());
                        tab_bar->setCurrentIndex(next);
                    }
                }
                return true;
            }

            if (ke->key() >= Qt::Key_1 && ke->key() <= Qt::Key_9)
            {
                const int target = ke->key() - Qt::Key_1;
                if (target < static_cast<int>(tabs.size()))
                {
                    tab_bar->setCurrentIndex(target);
                }
                return true;
            }

            if (ke->key() == Qt::Key_0)
            {
                if (9 < static_cast<int>(tabs.size()))
                {
                    tab_bar->setCurrentIndex(9);
                }
                return true;
            }
        }
    }
    else if (event->type() == QEvent::ShortcutOverride)
    {
        if (const auto* ke = dynamic_cast<QKeyEvent*>(event); ke->modifiers().testFlag(Qt::ControlModifier) && !
            QApplication::activeModalWidget())
        {
            if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab ||
                (ke->key() >= Qt::Key_0 && ke->key() <= Qt::Key_9))
            {
                event->accept();
                return true;
            }
        }
    }

    if (obj == tab_container)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            if (const auto* me = dynamic_cast<QMouseEvent*>(event); me->button() == Qt::LeftButton)
            {
                if (const QWidget* child = tab_container->childAt(me->position().toPoint()); !child || child ==
                    tab_container)
                {
                    if (windowHandle())
                    {
                        windowHandle()->startSystemMove();
                        return true;
                    }
                }
            }
        }
        else if (event->type() == QEvent::MouseButtonDblClick)
        {
            if (const auto* me = dynamic_cast<QMouseEvent*>(event); me->button() == Qt::LeftButton)
            {
                if (const QWidget* child = tab_container->childAt(me->position().toPoint()); !child || child ==
                    tab_container)
                {
                    if (isMaximized())
                    {
                        showNormal();
                    }
                    else
                    {
                        showMaximized();
                    }
                    return true;
                }
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

#if defined(Q_OS_WIN)
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (eventType == "windows_generic_MSG")
    {
        if (const auto* msg = static_cast<MSG*>(message); msg->message == WM_NCCALCSIZE)
        {
            if (msg->wParam == TRUE)
            {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
                WINDOWPLACEMENT wp;
                wp.length = sizeof(WINDOWPLACEMENT);
                GetWindowPlacement(msg->hwnd, &wp);

                if (wp.showCmd == SW_SHOWMAXIMIZED)
                {
                    const int borderX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                    const int borderY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);

                    params->rgrc[0].left += borderX;
                    params->rgrc[0].top += borderY;
                    params->rgrc[0].right -= borderX;
                    params->rgrc[0].bottom -= borderY;
                }
                *result = 0;
                return true;
            }
        }
        else if (msg->message == WM_GETMINMAXINFO)
        {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
            HMONITOR hMonitor = MonitorFromWindow(reinterpret_cast<HWND>(winId()), MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi;
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfo(hMonitor, &mi))
            {
                mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
                mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
                mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
            }
            *result = 0;
            return true;
        }
        else if (msg->message == WM_NCHITTEST)
        {
            POINT pt = {.x = GET_X_LPARAM(msg->lParam), .y = GET_Y_LPARAM(msg->lParam)};
            RECT rc;
            GetWindowRect(reinterpret_cast<HWND>(winId()), &rc);

            if (!isMaximized())
            {
                constexpr int border_width = 8;
                const bool left = pt.x >= rc.left && pt.x < rc.left + border_width;
                const bool right = pt.x < rc.right && pt.x >= rc.right - border_width;
                const bool top = pt.y >= rc.top && pt.y < rc.top + border_width;
                const bool bottom = pt.y < rc.bottom && pt.y >= rc.bottom - border_width;

                if (top && left)
                {
                    *result = HTTOPLEFT;
                    return true;
                }
                if (top && right)
                {
                    *result = HTTOPRIGHT;
                    return true;
                }
                if (bottom && left)
                {
                    *result = HTBOTTOMLEFT;
                    return true;
                }
                if (bottom && right)
                {
                    *result = HTBOTTOMRIGHT;
                    return true;
                }
                if (left)
                {
                    *result = HTLEFT;
                    return true;
                }
                if (right)
                {
                    *result = HTRIGHT;
                    return true;
                }
                if (top)
                {
                    *result = HTTOP;
                    return true;
                }
                if (bottom)
                {
                    *result = HTBOTTOM;
                    return true;
                }
            }

            POINT client_pt = pt;
            ScreenToClient(reinterpret_cast<HWND>(winId()), &client_pt);
            const qreal dpr = devicePixelRatioF();
            const QPoint local_pos(
                static_cast<int>(std::round(client_pt.x / dpr)),
                static_cast<int>(std::round(client_pt.y / dpr))
            );

            if (tab_container && tab_container->geometry().contains(local_pos))
            {
                const QPoint tab_c_pos = tab_container->mapFrom(this, local_pos);
                QWidget* child = tab_container->childAt(tab_c_pos);

                if (!child || child == tab_container)
                {
                    *result = HTCAPTION;
                    return true;
                }

                *result = HTCLIENT;
                return true;
            }

            *result = HTCLIENT;
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::setup_tab_close_button(const int index)
{
    auto* close_btn = new QToolButton(tab_bar);
    close_btn->setObjectName("tab_close_btn");
    close_btn->setFixedSize(20, 20);

    QIcon close_icon;
    close_icon.addFile(":/resources/close.svg", QSize(12, 12), QIcon::Normal);
    close_icon.addFile(":/resources/close_hover.svg", QSize(12, 12), QIcon::Active);
    close_btn->setIcon(close_icon);
    close_btn->setIconSize(QSize(12, 12));

    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setToolTip("Close tab (Ctrl+W)");

    connect(close_btn, &QToolButton::clicked, this, [this, close_btn]
    {
        for (int i = 0; i < tab_bar->count(); ++i)
        {
            if (tab_bar->tabButton(i, QTabBar::RightSide) == close_btn)
            {
                close_tab(i);
                break;
            }
        }
    });

    tab_bar->setTabButton(index, QTabBar::RightSide, close_btn);
    tab_bar->updateGeometry();
}

int MainWindow::create_new_tab(const QString& title, const QString& text, const QString& file_path)
{
    save_current_tab_state();

    auto tab = std::make_unique<NovelTab>();
    tab->title = title;
    tab->file_name = file_path;
    tab->page_length = ui->char_per_page->value();
    if (!text.isEmpty())
    {
        tab->set_text(text);
    }

    tabs.push_back(std::move(tab));
    const int new_index = static_cast<int>(tabs.size()) - 1;

    tab_bar->blockSignals(true);
    tab_bar->addTab(title);
    if (!file_path.isEmpty())
    {
        tab_bar->setTabToolTip(new_index, file_path);
    }
    tab_bar->setCurrentIndex(new_index);
    tab_bar->blockSignals(false);

    setup_tab_close_button(new_index);

    active_tab_index = -1;
    switch_to_tab(new_index);

    return new_index;
}

void MainWindow::close_tab(const int index)
{
    if (index < 0 || index >= static_cast<int>(tabs.size())) return;

    if (tabs.size() <= 1)
    {
        close();
        return;
    }

    int new_active = active_tab_index;
    if (index == active_tab_index)
    {
        new_active = index == static_cast<int>(tabs.size()) - 1 ? index - 1 : index;
    }
    else if (index < active_tab_index)
    {
        new_active = active_tab_index - 1;
    }

    if (converting_tab_index == index)
    {
        converting_tab_index = -1;
    }
    else if (converting_tab_index > index)
    {
        converting_tab_index--;
    }

    tabs.erase(tabs.begin() + index);

    tab_bar->blockSignals(true);
    tab_bar->removeTab(index);
    tab_bar->setCurrentIndex(new_active);
    tab_bar->blockSignals(false);

    active_tab_index = -1;
    switch_to_tab(new_active);
}

void MainWindow::switch_to_tab(const int index)
{
    if (index < 0 || index >= static_cast<int>(tabs.size())) return;
    if (index == active_tab_index) return;

    save_current_tab_state();
    active_tab_index = index;

    if (tab_bar->currentIndex() != index)
    {
        tab_bar->blockSignals(true);
        tab_bar->setCurrentIndex(index);
        tab_bar->blockSignals(false);
    }

    auto* cur = tabs[index].get();

    setWindowTitle(cur->title.isEmpty() ? "Hanvi" : cur->title + " - Hanvi");

    if (current_name_set_id != cur->name_set_id)
    {
        load_name_set(cur->name_set_id);
    }
    load_data();

    ui->char_per_page->blockSignals(true);
    ui->char_per_page->setValue(cur->page_length);
    ui->char_per_page->blockSignals(false);

    update_pagination_controls();

    if (cur->current_doc)
    {
        session->set_document(cur->current_doc);
        ui->cn_input->set_scroll_value(cur->saved_scroll.cn);
        ui->sv_output->set_scroll_value(cur->saved_scroll.sv);
        ui->vn_output->set_scroll_value(cur->saved_scroll.vn);
        ui->statusbar->showMessage("Ready.");
    }
    else if (!cur->input_text.isEmpty())
    {
        session->set_document(nullptr);
        convert_and_display(false);
    }
    else
    {
        session->set_document(nullptr);
        ui->cn_input->set_scroll_value(0);
        ui->sv_output->set_scroll_value(0);
        ui->vn_output->set_scroll_value(0);
        ui->statusbar->showMessage("Ready.");
    }
}

void MainWindow::save_current_tab_state()
{
    auto* cur = current_tab();
    if (!cur) return;

    cur->saved_scroll = {
        .cn = ui->cn_input->scroll_value(),
        .sv = ui->sv_output->scroll_value(),
        .vn = ui->vn_output->scroll_value()
    };
}

void MainWindow::load_data()
{
    const auto* cur = current_tab();
    const int active_set_id = cur ? cur->name_set_id : current_name_set_id;

    bool found = false;
    for (const auto& [index, title] : name_sets)
    {
        if (index == active_set_id)
        {
            ui->current_name_set->setText(title);
            found = true;
            break;
        }
    }

    if (!found)
    {
        ui->current_name_set->setText("None");
        if (active_set_id != -1)
        {
            load_name_set(-1);
            if (cur) const_cast<NovelTab*>(cur)->name_set_id = -1;
            convert_and_display(true);
        }
    }
}

void MainWindow::update_pagination_controls() const
{
    const auto* cur = current_tab();
    const int total = cur ? static_cast<int>(cur->pages.size()) : 0;
    const int page = cur ? cur->current_page : 0;

    if (total == 0)
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
        ui->current_page->setText(QString::number(page + 1));
        ui->total_page->setText(QString::number(total));
        ui->previous_page->setEnabled(page > 0);
        ui->next_page->setEnabled(page < total - 1);
    }
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    delete ui;
}

void MainWindow::convert_and_display(const bool scroll_back)
{
    auto* cur = current_tab();
    if (!cur) return;
    if (watcher.isRunning()) return;

    if (!cur->input_text.isEmpty() && cur->current_page < cur->pages.size() && !cur->pages[cur->current_page].isEmpty())
    {
        ui->statusbar->showMessage("Converting...");

        if (scroll_back)
        {
            cur->saved_scroll = {
                .cn = ui->cn_input->scroll_value(),
                .sv = ui->sv_output->scroll_value(),
                .vn = ui->vn_output->scroll_value()
            };
        }
        else cur->saved_scroll = {.cn = 0, .sv = 0, .vn = 0};

        converting_tab_index = active_tab_index;

        auto reporter = [this](int progress)
        {
            QMetaObject::invokeMethod(this, [this, progress]
            {
                if (const auto* novel_tab = current_tab(); novel_tab && novel_tab->current_page < novel_tab->pages.
                    size() && novel_tab->pages[novel_tab->current_page].length() > 0)
                {
                    ui->progress_bar->setValue(
                        static_cast<int>(progress * 100 / novel_tab->pages[novel_tab->current_page].length()));
                }
            });
        };

        const QFuture<std::shared_ptr<AlignedDocument>> future = QtConcurrent::run(
            convert, cur->pages[cur->current_page], reporter);
        watcher.setFuture(future);
    }
}

void MainWindow::convert_to_file()
{
    auto* cur = current_tab();
    if (!cur || cur->input_text.isEmpty()) return;

    ui->statusbar->showMessage("Saving to file...");

    auto reporter = [this](int progress)
    {
        QMetaObject::invokeMethod(this, [this, progress]
        {
            const auto* cur = current_tab();
            if (cur && cur->input_text.length() > 0)
            {
                ui->progress_bar->setValue(static_cast<int>(progress * 100 / cur->input_text.length()));
            }
        });
    };

    const QFuture<QString> future = QtConcurrent::run(
        convert_plain, cur->input_text, reporter);
    plain_watcher.setFuture(future);
}

void MainWindow::update_display()
{
    const auto doc = watcher.result();

    if (converting_tab_index >= 0 && converting_tab_index < static_cast<int>(tabs.size()))
    {
        tabs[converting_tab_index]->current_doc = doc;

        if (converting_tab_index == active_tab_index)
        {
            auto* cur = tabs[converting_tab_index].get();
            update_pagination_controls();
            ui->progress_bar->setValue(100);
            ui->statusbar->showMessage("Conversion completed.");

            session->set_document(doc);

            ui->cn_input->set_scroll_value(cur->saved_scroll.cn);
            ui->sv_output->set_scroll_value(cur->saved_scroll.sv);
            ui->vn_output->set_scroll_value(cur->saved_scroll.vn);
            cur->saved_scroll = {.cn = 0, .sv = 0, .vn = 0};

            if (!cur->saved_token_cn.isEmpty())
            {
                for (const auto& para : doc->paragraphs)
                {
                    for (const auto& tok : para.tokens)
                    {
                        if (tok.cn == cur->saved_token_cn || tok.cn.contains(cur->saved_token_cn))
                        {
                            session->set_active_token(tok.id);
                            goto token_found;
                        }
                    }
                }
            token_found:
                cur->saved_token_cn.clear();
            }
        }
    }
    converting_tab_index = -1;
}

void MainWindow::on_request_dict_popup(const QString& chinese_text)
{
    if (chinese_text.isEmpty()) return;

    auto* cur = current_tab();
    if (!cur) return;

    auto* popup = new DictPopup(this);
    popup->load_data(chinese_text);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    if (popup->exec())
    {
        cur->saved_token_cn = chinese_text;
        cur->current_doc = nullptr;
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
