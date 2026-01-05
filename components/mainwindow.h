#ifndef CONVERTER_MAINWINDOW_H
#define CONVERTER_MAINWINDOW_H

#include <QMainWindow>
#include <QTextBrowser>


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

private:
    Ui::MainWindow* ui;
    void convert_and_display() const;
    void click_token(const QUrl &link) const;
    static void highlight_token(QTextBrowser* browser, const QString& token);
    static QTextCursor find_token(QTextDocument* document, const QString& token);
};


#endif //CONVERTER_MAINWINDOW_H