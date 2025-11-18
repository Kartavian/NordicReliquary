#include <QApplication>
#include "firstrunwizard.h"
#include "mainWindow.h"

#include <QFile>
#include <QDir>

bool hasConfig()
{
    return QFile::exists(QDir::homePath() + "/.config/NordicReliquary/config.ini");
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString gamePath, workspacePath;

    if(!hasConfig()) {
        FirstRunWizard wizard;
        if(wizard.exec() != QDialog::Accepted)
            return 0;

        gamePath = wizard.gamePath();
        workspacePath = wizard.workspacePath();

        QDir().mkpath(QDir::homePath() + "/.config/NordicReliquary");

        QFile f(QDir::homePath() + "/.config/NordicReliquary/config.ini");
        if(f.open(QFile::WriteOnly)) {
            QTextStream out(&f);
            out << "gamePath=" << gamePath << "\n";
            out << "workspacePath=" << workspacePath << "\n";
        }
    }

    MainWindow w;
    w.show();
    return app.exec();
}
