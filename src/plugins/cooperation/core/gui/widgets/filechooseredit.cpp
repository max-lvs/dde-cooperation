// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filechooseredit.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QPainter>

using namespace cooperation_core;

FileChooserEdit::FileChooserEdit(QWidget *parent)
    : QWidget(parent)
{
    initUI();
}

void FileChooserEdit::initUI()
{
    pathLabel = new QLabel(this);
    pathLabel->setContentsMargins(8, 8, 8, 8);

    fileChooserBtn = new QPushButton(this);
    fileChooserBtn->setFixedSize(36, 36);
    connect(fileChooserBtn, &QPushButton::clicked, this, &FileChooserEdit::onButtonClicked);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(10);
    setLayout(mainLayout);

    mainLayout->addWidget(pathLabel);
    mainLayout->addWidget(fileChooserBtn);
}

void FileChooserEdit::setText(const QString &text)
{
    pathLabel->setText(text);
}

void FileChooserEdit::onButtonClicked()
{
    auto dirPath = QFileDialog::getExistingDirectory(this);
    if (dirPath.isEmpty())
        return;

    QFontMetrics fontMetrices(pathLabel->font());
    QString showName = fontMetrices.elidedText(dirPath, Qt::ElideRight, pathLabel->width() - 16);
    if (showName != dirPath)
        pathLabel->setToolTip(dirPath);

    pathLabel->setText(showName);
    emit fileChoosed(dirPath);
}

void FileChooserEdit::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    QColor color(0, 0, 0, static_cast<int>(255 * 0.08));
    painter.setBrush(color);
    painter.drawRoundedRect(pathLabel->rect(), 8, 8);

    QWidget::paintEvent(event);
}