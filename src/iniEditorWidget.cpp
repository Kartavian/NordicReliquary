#include "iniEditorWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

IniEditorWidget::IniEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);

    auto *contentLayout = new QHBoxLayout();
    fileList = new QListWidget(this);
    fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    contentLayout->addWidget(fileList, 1);

    editor = new QPlainTextEdit(this);
    editor->setPlaceholderText("Select an INI file to view or edit.");
    contentLayout->addWidget(editor, 3);

    mainLayout->addLayout(contentLayout);

    saveButton = new QPushButton("Save Changes", this);
    mainLayout->addWidget(saveButton, 0, Qt::AlignRight);

    connect(fileList, &QListWidget::itemClicked,
            this, &IniEditorWidget::onFileSelected);
    connect(saveButton, &QPushButton::clicked,
            this, &IniEditorWidget::onSaveClicked);
}

void IniEditorWidget::setIniRoots(const QList<QPair<QString, QString>> &roots)
{
    iniRoots = roots;
    refreshFiles();
}

void IniEditorWidget::refreshFiles()
{
    fileList->clear();
    editor->clear();
    currentFilePath.clear();

    for (const auto &root : iniRoots) {
        QDir base(root.first);
        if (!base.exists())
            continue;

        QDirIterator it(root.first,
                        QStringList() << "*.ini" << "*.INI",
                        QDir::Files,
                        QDirIterator::Subdirectories);

        while (it.hasNext()) {
            QString path = it.next();
            QString rel = base.relativeFilePath(path);
            QString label = QString("%1: %2").arg(root.second, rel);
            auto *item = new QListWidgetItem(label, fileList);
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
        }
    }
}

void IniEditorWidget::onFileSelected(QListWidgetItem *item)
{
    if (!item)
        return;
    QString path = item->data(Qt::UserRole).toString();
    loadFile(path);
}

void IniEditorWidget::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
                             QString("Failed to open %1").arg(path));
        return;
    }

    QTextStream in(&file);
    editor->setPlainText(in.readAll());
    currentFilePath = path;
}

void IniEditorWidget::onSaveClicked()
{
    if (currentFilePath.isEmpty())
        return;

    QFile file(currentFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
                             QString("Failed to save %1").arg(currentFilePath));
        return;
    }

    QTextStream out(&file);
    out << editor->toPlainText();
    file.close();
}
