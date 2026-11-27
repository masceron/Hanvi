#pragma once

#include <QMainWindow>
#include <QFutureWatcher>
#include <memory>
#include <vector>
#include "app/appearance.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace hanvi {
    class AlignedDocument;
    struct Rule;
    struct TokenOverride;
}

using hanvi::AlignedDocument;
using hanvi::Rule;
using hanvi::TokenOverride;
class DocumentSession;
class QDragEnterEvent;
class QDropEvent;
class QTabBar;
class QPushButton;
class findbar;


QT_BEGIN_NAMESPACE

namespace Ui {
    class MainWindow;
}

QT_END_NAMESPACE

struct SavedScroll {
    int cn = 0;
    int sv = 0;
    int vn = 0;
};

struct NovelTab {
    QString title = "Untitled";
    QString file_name;
    QString input_text;
    QList<QStringView> pages;
    int current_page = 0;
    int page_length = 5000;
    int name_set_id = -1;

    // Translations picked for single occurrences in input_text, kept until the text is replaced.
    // Every conversion applies them; one whose word a dictionary edit re-segmented is dropped.
    std::vector<TokenOverride> overrides;

    std::shared_ptr<AlignedDocument> current_doc = nullptr;
    SavedScroll saved_scroll;
    QString saved_token_cn;

    void set_text(const QString &text);

    void repaginate();

    [[nodiscard]] size_t current_page_chinese_char_count() const noexcept;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    void load_data();

    ~MainWindow() override;

    int create_new_tab(const QString &title = "Untitled", const QString &text = "", const QString &file_path = "");

    void close_tab(int index);

    void switch_to_tab(int index);

protected:
    void changeEvent(QEvent *event) override;

    void dragEnterEvent(QDragEnterEvent *event) override;

    void dropEvent(QDropEvent *event) override;

    bool eventFilter(QObject *obj, QEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private slots:
    void update_display();

    void on_request_dict_popup(const QString &chinese_text, uint32_t token_id);

    void on_request_rule_popup(const Rule *rule);

private:
    Ui::MainWindow *ui;
    QTabBar *tab_bar = nullptr;
    QWidget *tab_container = nullptr;
    QPushButton *hamburger_btn = nullptr;
    QPushButton *add_tab_btn = nullptr;
    QPushButton *min_btn = nullptr;
    QPushButton *max_btn = nullptr;
    QPushButton *close_win_btn = nullptr;
    AppearanceSettings appearance_;
    DocumentSession *session = nullptr;
    QFutureWatcher<std::shared_ptr<AlignedDocument> > watcher;
    QFutureWatcher<QString> plain_watcher;

    std::vector<std::unique_ptr<NovelTab> > tabs;
    int active_tab_index = -1;
    int converting_tab_index = -1;
    // The overrides the running conversion was given, which its stale_overrides index.
    std::vector<TokenOverride> converting_overrides;

    [[nodiscard]] NovelTab *current_tab() noexcept;

    [[nodiscard]] const NovelTab *current_tab() const noexcept;

    void save_current_tab_state();

    void convert_and_display(bool scroll_back);

    void update_pagination_controls() const;

    void convert_to_file();

    void setup_tab_close_button(int index);

    void setup_hamburger_menu(QPushButton *btn);

    void update_max_restore_button() const;

    void apply_appearance(const AppearanceSettings &settings, bool apply_stylesheet = true);

    void update_chrome_icons() const;

    void open_file_path(const QString &name);

    void open_dropped_text(const QString &text);

#ifdef Q_OS_WIN
    void triggerDpiRecovery();

    HPOWERNOTIFY m_powerNotify = nullptr;
#else
    Qt::Edges resize_cursor_edges;
    [[nodiscard]] Qt::Edges resize_edges_at(const QPointF &pos) const;
    void update_resize_cursor(Qt::Edges edges);
    bool handle_edge_resize(QEvent *event);
#endif

    findbar *find_bar = nullptr;

    void show_findbar() const;

    void reposition_findbar() const;
};
