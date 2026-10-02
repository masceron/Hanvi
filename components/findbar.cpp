#include "findbar.h"
#include "ui_findbar.h"
#include <QKeyEvent>

findbar::findbar(QWidget *parent) : QWidget(parent), ui(new Ui::findbar) {
    setAttribute(Qt::WA_StyledBackground, true);
    ui->setupUi(this);

    ui->search_box->installEventFilter(this);

    connect(ui->search_box, &QLineEdit::textChanged, this, [this](const QString& text) {
        emit search_requested(text);
    });

    connect(ui->next_match, &QPushButton::clicked, this, &findbar::next_requested);
    connect(ui->previous_match, &QPushButton::clicked, this, &findbar::previous_requested);

    connect(ui->search_button, &QPushButton::clicked, this, [this] {
        emit search_requested(ui->search_box->text());
    });

    connect(ui->close, &QPushButton::clicked, this, [this] {
        hide();
        emit closed();
    });

    set_matches_count(-1, 0);
}

findbar::~findbar() {
    delete ui;
}

void findbar::set_matches_count(const int current, const int total) const {
    if (total <= 0) {
        ui->matches->setText(QStringLiteral("0/0"));
        ui->next_match->setEnabled(false);
        ui->previous_match->setEnabled(false);
    } else {
        ui->matches->setText(QString(QStringLiteral("%1/%2")).arg(current + 1).arg(total));
        ui->next_match->setEnabled(total > 1);
        ui->previous_match->setEnabled(total > 1);
    }
}

void findbar::focus_input() const {
    ui->search_box->setFocus();
    ui->search_box->selectAll();
}

void findbar::set_search_text(const QString& text) const {
    ui->search_box->setText(text);
    ui->search_box->selectAll();
}

QString findbar::search_text() const {
    return ui->search_box->text();
}

bool findbar::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->search_box && event->type() == QEvent::KeyPress) {
        const auto *ke = dynamic_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            hide();
            emit closed();
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers().testFlag(Qt::ShiftModifier)) {
                emit previous_requested();
            } else {
                emit next_requested();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
