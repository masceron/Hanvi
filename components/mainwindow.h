#pragma once

#include <QMainWindow>
#include <QTextBrowser>
#include <QtConcurrent>

QT_BEGIN_NAMESPACE

namespace Ui
{
    class MainWindow;
}

QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    void load_data();
    ~MainWindow() override;

private slots:
    void update_display();
    void open_popup();

private:
    int current_page;
    int page_length = 5000;
    QString file_name;
    QString input_text;
    QList<QStringView> pages;
    Ui::MainWindow* ui;
    QFutureWatcher<std::tuple<QString, QString, QString>> watcher;
    QFutureWatcher<QString> plain_watcher;
    int saved_cursor_pos = -1;

    void convert_and_display();
    void update_pagination_controls() const;
    void click_token(const QUrl &link) const;
    static void highlight_token(QTextBrowser* browser, const QString& token);
    static QTextCursor find_token(QTextDocument* document, const QString& token);
    static QString token_id_at(const QTextBrowser* browser, int position);
    static void snap_selection_to_token(QTextBrowser* browser);
    static void snap_selection_to_word(QTextBrowser* browser);
    QString get_chinese_text_from_ids(const QStringList& ids) const;
    void convert_to_file();
};