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
    ~MainWindow() override;

private slots:
    void update_display() const;

private:
    int current_page;
    int page_length = 5000;
    QString input_text;
    QList<QStringView> pages;
    Ui::MainWindow* ui;
    void convert_and_display();
    void update_pagination_controls() const;
    void click_token(const QUrl &link) const;
    static void highlight_token(QTextBrowser* browser, const QString& token);
    static QTextCursor find_token(QTextDocument* document, const QString& token);
    QFutureWatcher<std::pair<QString, QString>> watcher;
};