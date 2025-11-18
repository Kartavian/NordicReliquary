#ifndef INIEDITORWIDGET_H
#define INIEDITORWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>

class IniEditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit IniEditorWidget(QWidget *parent = nullptr);

    void setIniRoots(const QList<QPair<QString, QString>> &roots);

public slots:
    void refreshFiles();

private slots:
    void onFileSelected(QListWidgetItem *item);
    void onSaveClicked();

private:
    QList<QPair<QString, QString>> iniRoots;
    QListWidget *fileList = nullptr;
    QPlainTextEdit *editor = nullptr;
    QPushButton *saveButton = nullptr;
    QString currentFilePath;

    void loadFile(const QString &path);
};

#endif // INIEDITORWIDGET_H
