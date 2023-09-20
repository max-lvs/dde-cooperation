﻿#include "fileselectwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QToolButton>
#include <QStackedWidget>
#include <QCheckBox>
#include <QTreeView>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QUrl>
#include <QDir>

#include <utils/optionsmanager.h>
#include <utils/transferhepler.h>
#include <gui/mainwindow_p.h>
#pragma execution_character_set("utf-8")

const QList<QString> directories = {
    QString::fromLocal8Bit(Directory::kMovie), QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
    QString::fromLocal8Bit(Directory::kPicture), QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
    QString::fromLocal8Bit(Directory::kDocuments), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
    QString::fromLocal8Bit(Directory::kDownload), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
    QString::fromLocal8Bit(Directory::kMusic), QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
    QString::fromLocal8Bit(Directory::kDesktop), QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
};

FileSelectWidget::FileSelectWidget(QListView *siderbarWidget, QWidget *parent)
    : QFrame(parent)
{
    initSiderBar(siderbarWidget);
    initUI();
    initConnect(sidebar);
    initConnect(fileview);
}

FileSelectWidget::~FileSelectWidget()
{
}

void FileSelectWidget::initUI()
{
    setStyleSheet("background-color: white; border-radius: 10px;");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);
    mainLayout->setSpacing(0);
    mainLayout->addSpacing(30);

    QLabel *titileLabel = new QLabel("请选择要传输的文件", this);
    titileLabel->setFixedHeight(40);
    QFont font;
    font.setPointSize(16);
    font.setWeight(QFont::DemiBold);
    titileLabel->setFont(font);
    titileLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    initFileView();

    QLabel *tipLabel1 = new QLabel("传输完成的数据，将被存放在用户的 home 目录下", this);
    tipLabel1->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    tipLabel1->setFixedHeight(80);
    font.setPointSize(10);
    font.setWeight(QFont::Thin);
    tipLabel1->setFont(font);

    QToolButton *determineButton = new QToolButton(this);
    QPalette palette = determineButton->palette();
    palette.setColor(QPalette::ButtonText, Qt::white);
    determineButton->setPalette(palette);
    determineButton->setText("确定");
    determineButton->setFixedSize(120, 35);
    determineButton->setStyleSheet("background-color: #0098FF;");
    QObject::connect(determineButton, &QToolButton::clicked, this, &FileSelectWidget::nextPage);

    QToolButton *cancelButton = new QToolButton(this);
    cancelButton->setText("取消");
    cancelButton->setFixedSize(120, 35);
    cancelButton->setStyleSheet("background-color: lightgray;");
    QObject::connect(cancelButton, &QToolButton::clicked, this, &FileSelectWidget::backPage);

    QHBoxLayout *buttonLayout = new QHBoxLayout(this);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addSpacing(15);
    buttonLayout->addWidget(determineButton);
    buttonLayout->setAlignment(Qt::AlignHCenter);

    mainLayout->addWidget(titileLabel);
    mainLayout->addWidget(tipLabel1);
    mainLayout->addWidget(fileview);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addSpacing(20);
}

void FileSelectWidget::initConnect(QAbstractItemView *view)
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(view->model());
    connect(model, &QStandardItemModel::itemChanged, [this](QStandardItem *changedItem) {
        if (!aync)
            return;
        QString filepath = changedItem->data(Qt::UserRole).toString();
        if (changedItem->checkState() == Qt::Checked) {
            if (!seletFileList.contains(filepath)) {
                seletFileList.append(filepath);
                qInfo() << filepath << "add";
            }
        } else {
            if (seletFileList.contains(filepath)) {
                seletFileList.removeOne(filepath);
                qInfo() << filepath << "del";
            }
        }
    });
}

void FileSelectWidget::initFileView()
{
    QStandardItemModel *model = new QStandardItemModel(this);

    for (int i = 0; model && i < 20; i++) {
        QStandardItem *item = new QStandardItem();
        item->setCheckable(true);
        item->setCheckState(Qt::Unchecked);
        model->appendRow(item);
    }

    fileview = new QTreeView(this);
    fileview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fileview->setModel(model);
    fileview->setStyleSheet("QTreeView { border: none; }");
    updateFileView();
}

void FileSelectWidget::updateFileView()
{
    if (!sidebar || sidebar->selectionModel()->selectedIndexes().isEmpty())
        return;

    QModelIndex index = sidebar->selectionModel()->selectedIndexes().first();
    QString itemText = sidebar->model()->data(index, Qt::DisplayRole).toString();

    QString path = sidebar->model()->data(index, Qt::UserRole).toString();

    QFileInfoList fileinfos = QDir(path).entryInfoList();
    aync = false;
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(fileview->model());
    for (int i = 0; model && i < fileinfos.count() && i < 20; i++) {
        auto item = model->item(i);
        if (!seletFileList.contains(fileinfos[i].filePath()))
            item->setCheckState(Qt::Unchecked);
        else
            item->setCheckState(Qt::Checked);
        item->setData(fileinfos[i].filePath(), Qt::UserRole);
        item->setData(fileinfos[i].fileName(), Qt::DisplayRole);
    }
    aync = true;
}

void FileSelectWidget::initSiderBar(QListView *siderbarWidget)
{
    if (!siderbarWidget) {
        qWarning() << "Sidebar init failed";
        return;
    }
    sidebar = siderbarWidget;
    connect(sidebar->selectionModel(), &QItemSelectionModel::selectionChanged, this, &FileSelectWidget::updateFileView);
}

void FileSelectWidget::nextPage()
{
    //send useroptions
    sendOptions();

    //nextpage
    QStackedWidget *stackedWidget = qobject_cast<QStackedWidget *>(this->parent());
    if (stackedWidget) {
        stackedWidget->setCurrentIndex(data_transfer_core::PageName::selectmainwidget);
    } else {
        qWarning() << "Jump to next page failed, qobject_cast<QStackedWidget *>(this->parent()) = nullptr";
    }
}

void FileSelectWidget::backPage()
{
    QStackedWidget *stackedWidget = qobject_cast<QStackedWidget *>(this->parent());
    if (stackedWidget) {
        stackedWidget->setCurrentIndex(data_transfer_core::PageName::selectmainwidget);
    } else {
        qWarning() << "Jump to next page failed, qobject_cast<QStackedWidget *>(this->parent()) = nullptr";
    }
}

void FileSelectWidget::update()
{
}

void FileSelectWidget::sendOptions()
{
    qInfo() << "select file :" << seletFileList;
    OptionsManager::instance()->addUserOption(Options::kFile, seletFileList);
}

SidebarWidget::SidebarWidget(QWidget *parent)
    : QListView(parent)
{
    setStyleSheet("background-color: white;");
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    initData();
}

SidebarWidget::~SidebarWidget()
{
}

void SidebarWidget::initData()
{
    QStandardItemModel *model = new QStandardItemModel(this);
    setModel(model);
    int row = 0;
    for (int i = 0; i < 6; i++) {
        QStandardItem *item = new QStandardItem();
        item->setCheckable(true);
        item->setCheckState(Qt::Unchecked);
        item->setData(directories[row], Qt::DisplayRole);
        item->setData(directories[row + 1], Qt::UserRole);
        model->appendRow(item);
        row += 2;
    }
}
