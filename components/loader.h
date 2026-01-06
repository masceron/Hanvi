#pragma once

#include <QDialog>
#include <QDirIterator>
#include <QLabel>
#include <QMovie>
#include <QVBoxLayout>

class Loader : public QDialog {
    Q_OBJECT
public:
    explicit Loader(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);

        const auto layout = new QVBoxLayout(this);

        const auto loadingLabel = new QLabel(this);
        qDebug() << "Listing embedded resources:";
        QDirIterator it(":", QDirIterator::Subdirectories);
        while (it.hasNext()) {
            qDebug() << it.next();
        }
        const auto movie = new QMovie(":/resources/loader.gif");
        loadingLabel->setMovie(movie);
        movie->setScaledSize(QSize(400, 225));
        movie->start();

        const auto textLabel = new QLabel("Loading...", this);
        textLabel->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");

        layout->addWidget(loadingLabel, 0, Qt::AlignCenter);
        layout->addWidget(textLabel, 0, Qt::AlignCenter);

        setStyleSheet("QDialog { border: 1px solid #555; border-radius: 10px; padding: 20px; }");
    }
};