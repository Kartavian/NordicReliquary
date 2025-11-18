#ifndef DOWNLOADSPANEL_H
#define DOWNLOADSPANEL_H

#include <QtWidgets/QWidget>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtCore/QDir>
#include <QtCore/QDebug>

class DownloadsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DownloadsPanel(QWidget *parent = nullptr);

    void setDownloadsDirectory(const QString &dir);
    QString downloadsDirectory() const { return downloadsDir; }

signals:
    void installRequested(const QStringList &archives);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void loadExistingDownloads();
    void saveDownloads() const;

    QListWidget *downloadsList;
    QPushButton *clearButton;
    QPushButton *installButton;
    QString downloadsDir;
};

#endif // DOWNLOADSPANEL_H
