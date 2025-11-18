#include "downloadsPanel.h"
//#include "ui_downloadspanel.h"

// Core Qt includes
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>
#include <QLabel>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QCoreApplication>  // make sure this include is near the top

// Widgets and GUI components
#include <QMenu>
#include <QMessageBox>
#include <QFileDialog>
#include <QListWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QCursor>
#include <QAction>

void DownloadsPanel::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        // Check file extensions
        for (const QUrl &url : event->mimeData()->urls()) {
            QString fileName = url.toLocalFile();
            if (fileName.endsWith(".zip", Qt::CaseInsensitive) ||
                fileName.endsWith(".7z", Qt::CaseInsensitive) ||
                fileName.endsWith(".rar", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void DownloadsPanel::dropEvent(QDropEvent *event) {
    event->acceptProposedAction();  // ensure we accept multiple
    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;

    QString destDir = downloadsDir.isEmpty()
        ? QDir::currentPath() + "/Downloads"
        : downloadsDir;
    QDir().mkpath(destDir);

    for (const QUrl &url : urls) {
        QString srcPath = url.toLocalFile();
        QFileInfo info(srcPath);
        QString destPath = destDir + "/" + info.fileName();

        // Ask once before loop if you prefer
        bool success = false;
        if (!QFile::exists(destPath)) {
            success = QFile::copy(srcPath, destPath);
        }

        if (success) {
            downloadsList->addItem(info.fileName());
        }
    }
}


DownloadsPanel::DownloadsPanel(QWidget *parent)
    : QWidget(parent)
{
    qDebug() << "[DownloadsPanel] Constructor called";

    // --- TEMP: sanity check to confirm the panel constructs correctly ---
    auto *layout = new QVBoxLayout(this);
    auto *label = new QLabel("Downloads panel placeholder", this);
    layout->addWidget(label);
    setLayout(layout);

    // Download list
    downloadsList = new QListWidget(this);
    downloadsList->setAcceptDrops(true);
    downloadsList->setDragEnabled(true);
    downloadsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    downloadsList->setStyleSheet("background-color: #2b2b2b; color: white;");

    // Clear button
    clearButton = new QPushButton("Clear Downloads", this);
    clearButton->setStyleSheet("background-color: #4b0082; color: white; border-radius: 5px; padding: 5px;");

    connect(clearButton, &QPushButton::clicked, [this]() {
        downloadsList->clear();
        qDebug() << "Downloads list cleared.";
    });

    installButton = new QPushButton("Install Selected", this);
    installButton->setStyleSheet("background-color: #1c7c54; color: white; border-radius: 5px; padding: 5px;");
    connect(installButton, &QPushButton::clicked, [this]() {
        QStringList archives;
        for (QListWidgetItem *item : downloadsList->selectedItems())
            archives << item->text();
        if (!archives.isEmpty())
            emit installRequested(archives);
    });

    setAcceptDrops(true);

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        saveDownloads();
    });

    layout->addWidget(downloadsList);
    layout->addWidget(clearButton);
    layout->addWidget(installButton);
    setLayout(layout);

    qDebug() << "[DownloadsPanel] Finished setup";

    setDownloadsDirectory(QDir::currentPath() + "/Downloads");
}

void DownloadsPanel::closeEvent(QCloseEvent *event) {
    saveDownloads();
    QWidget::closeEvent(event);
}

void DownloadsPanel::setDownloadsDirectory(const QString &dir)
{
    if (downloadsDir == dir)
        return;

    downloadsDir = dir;
    QDir().mkpath(downloadsDir);
    loadExistingDownloads();
}

void DownloadsPanel::loadExistingDownloads()
{
    downloadsList->clear();

    QString dir = downloadsDir.isEmpty()
        ? QDir::currentPath() + "/Downloads"
        : downloadsDir;
    QDir downloadDir(dir);
    if (!downloadDir.exists())
        downloadDir.mkpath(".");

    QString path = downloadDir.absoluteFilePath("downloads.json");
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        for (const QJsonValue &v : doc.array())
            downloadsList->addItem(v.toString());
    }
}

void DownloadsPanel::saveDownloads() const
{
    QString dir = downloadsDir.isEmpty()
        ? QDir::currentPath() + "/Downloads"
        : downloadsDir;
    QDir downloadDir(dir);
    if (!downloadDir.exists())
        downloadDir.mkpath(".");

    QString path = downloadDir.absoluteFilePath("downloads.json");
    QJsonArray arr;
    for (int i = 0; i < downloadsList->count(); ++i)
        arr.append(downloadsList->item(i)->text());

    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson());
}
