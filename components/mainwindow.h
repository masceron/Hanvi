#pragma once

#include <QMainWindow>
#include <QFutureWatcher>
#include <memory>
#include <vector>

class AlignedDocument;
class DocumentSession;
class QTabBar;
class QPushButton;
struct Rule;

QT_BEGIN_NAMESPACE

namespace Ui
{
    class MainWindow;
}

QT_END_NAMESPACE

struct SavedScroll
{
    int cn = 0;
    int sv = 0;
    int vn = 0;
};

struct NovelTab
{
    QString title = "Untitled";
    QString file_name;
    QString input_text;
    QList<QStringView> pages;
    int current_page = 0;
    int page_length = 5000;
    int name_set_id = -1;

    std::shared_ptr<AlignedDocument> current_doc = nullptr;
    SavedScroll saved_scroll;
    QString saved_token_cn;

    void set_text(QString text);
    void repaginate();
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    void load_data();
    ~MainWindow() override;

    int create_new_tab(const QString& title = "Untitled", const QString& text = "", const QString& file_path = "");
    void close_tab(int index);
    void switch_to_tab(int index);

protected:
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
#if defined(Q_OS_WIN)
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private slots:
    void update_display();
    void on_request_dict_popup(const QString& chinese_text);
    void on_request_rule_popup(const Rule* rule);

private:
    Ui::MainWindow* ui;
    QTabBar* tab_bar = nullptr;
    QWidget* tab_container = nullptr;
    QPushButton* max_btn = nullptr;
    DocumentSession* session = nullptr;
    QFutureWatcher<std::shared_ptr<AlignedDocument>> watcher;
    QFutureWatcher<QString> plain_watcher;

    std::vector<std::unique_ptr<NovelTab>> tabs;
    int active_tab_index = -1;
    int converting_tab_index = -1;

    [[nodiscard]] NovelTab* current_tab() noexcept;
    [[nodiscard]] const NovelTab* current_tab() const noexcept;
    void save_current_tab_state();
    void convert_and_display(bool scroll_back);
    void update_pagination_controls() const;
    void convert_to_file();
    void setup_tab_close_button(int index);
    void setup_hamburger_menu(QPushButton* btn);
    void update_max_restore_button() const;
};
