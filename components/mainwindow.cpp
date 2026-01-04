#include "mainwindow.h"

#include <QFileDialog>

#include "ui_MainWindow.h"
#include "../text/include/convert.h"
#include "../text/include/io.h"

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), ui(new Ui::MainWindow)
{
    setWindowState(Qt::WindowMaximized);
    ui->setupUi(this);

    const auto clipboardAction = new QAction("Reload", this);
    connect(clipboardAction, &QAction::triggered, this, []
    {
        convert();
    });

    ui->menubar->addAction(clipboardAction);

    ui->main->setStretchFactor(0, 1);
    ui->main->setStretchFactor(1, 4);

    ui->left_col->setStretchFactor(0, 2);
    ui->left_col->setStretchFactor(1, 1);

    connect(ui->actionFrom_clipboard, &QAction::triggered, this, [this]
    {
        load_from_clipboard();
        if (!input.isEmpty())
        {
            ui->cn_input->setHtml(input);
        }
    });

    connect(ui->actionFrom_file, &QAction::triggered, this, [this]
    {
        const auto name = QFileDialog::getOpenFileName(this, "Open file", "/", "Text files (*.txt)");
        load_from_file(name);
        if (!input.isEmpty())
        {
            ui->cn_input->setPlainText(input);
        }
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}
