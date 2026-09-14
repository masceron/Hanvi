#pragma once

#include <QMainWindow>
#include <QFutureWatcher>
#include <memory>

class AlignedDocument;
class DocumentSession;
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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    void load_data();
    ~MainWindow() override;

private slots:
    void update_display();
    void on_request_dict_popup(const QString& chinese_text);
    void on_request_rule_popup(const Rule* rule);

private:
    int current_page;
    int page_length = 5000;
    QString file_name;
    QString input_text;
    QList<QStringView> pages;
    Ui::MainWindow* ui;
    DocumentSession* session = nullptr;
    QFutureWatcher<std::shared_ptr<AlignedDocument>> watcher;
    QFutureWatcher<QString> plain_watcher;
    SavedScroll saved_scroll;
    QString saved_token_cn;

    void convert_and_display(bool scroll_back);
    void update_pagination_controls() const;
    void convert_to_file();
};
