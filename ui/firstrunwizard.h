#ifndef FIRSTRUNWIZARD_H
#define FIRSTRUNWIZARD_H

#include <QWizard>
#include <QString>

namespace Ui {
class FirstRunWizard;
}

class FirstRunWizard : public QWizard
{
    Q_OBJECT

public:
    explicit FirstRunWizard(QWidget *parent = nullptr);
    ~FirstRunWizard();

    QString gamePath() const;
    QString workspacePath() const;

private slots:
    void onBrowseGamePath();
    void onAutoDetectSteam();
    void onBrowseWorkspace();
    void onSuggestWorkspaceGameDrive();
    void onSuggestWorkspaceHome();

    void validatePage1();
    void validatePage2();

protected:
    void initializePage(int id) override;

private:
    Ui::FirstRunWizard *ui;

    bool looksLikeSkyrim(const QString &path) const;
};

#endif // FIRSTRUNWIZARD_H
