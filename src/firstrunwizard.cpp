#include "firstrunwizard.h"
#include "ui_firstrunwizard.h"
#include "detectLootType.h"

#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : QWizard(parent)
    , ui(new Ui::FirstRunWizard)
{
    ui->setupUi(this);
    setWindowTitle("Nordic Reliquary - First Time Setup");

    // --------------------------------
    // Connect Page 1 buttons
    // --------------------------------
    connect(ui->browseGameButton, &QPushButton::clicked,
            this, &FirstRunWizard::onBrowseGamePath);

    connect(ui->autoDetectSteamButton, &QPushButton::clicked,
            this, &FirstRunWizard::onAutoDetectSteam);

    // --------------------------------
    // Connect Page 2 buttons
    // --------------------------------
    connect(ui->browseWorkspaceButton, &QPushButton::clicked,
            this, &FirstRunWizard::onBrowseWorkspace);

    connect(ui->suggestGameDriveButton, &QPushButton::clicked,
            this, &FirstRunWizard::onSuggestWorkspaceGameDrive);

    connect(ui->suggestHomeButton, &QPushButton::clicked,
            this, &FirstRunWizard::onSuggestWorkspaceHome);

    // --------------------------------
    // Validate pages when switching
    // --------------------------------
    connect(this, &QWizard::currentIdChanged, this, [=](int id){
        if (id == 0) validatePage1();
        if (id == 1) validatePage2();

        // Page 3 summary
        if (id == 2) {
            ui->summaryLabel->setText(
                "<b>Game Path:</b> " + ui->gamePathEdit->text() + "<br>"
                "<b>Workspace Path:</b> " + ui->workspacePathEdit->text()
            );
        }
    });

    setOption(QWizard::NoBackButtonOnStartPage);
}

FirstRunWizard::~FirstRunWizard()
{
    delete ui;
}

// --------------------------------
// Accessors
// --------------------------------
QString FirstRunWizard::gamePath() const {
    return ui->gamePathEdit->text();
}

QString FirstRunWizard::workspacePath() const {
    return ui->workspacePathEdit->text();
}

// --------------------------------
// Page 1: Game Path
// --------------------------------
void FirstRunWizard::onBrowseGamePath()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Game Folder");
    if (!dir.isEmpty())
        ui->gamePathEdit->setText(dir);

    validatePage1();
}

void FirstRunWizard::onAutoDetectSteam()
{
    QString steamPath = "/mnt/skyrimae/SteamLibrary/steamapps/common/Skyrim";

    if (QDir(steamPath).exists())
        ui->gamePathEdit->setText(steamPath);

    validatePage1();
}

// --------------------------------
// Page 2: Workspace Path
// --------------------------------
void FirstRunWizard::onBrowseWorkspace()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Reliquary Workspace");
    if (!dir.isEmpty())
        ui->workspacePathEdit->setText(dir);

    validatePage2();
}

void FirstRunWizard::onSuggestWorkspaceGameDrive()
{
    ui->workspacePathEdit->setText("/mnt/skyrimae/NordicReliquary");
    validatePage2();
}

void FirstRunWizard::onSuggestWorkspaceHome()
{
    ui->workspacePathEdit->setText(QDir::homePath() + "/NordicReliquary");
    validatePage2();
}

// --------------------------------
// Validation
// --------------------------------
bool FirstRunWizard::looksLikeSkyrim(const QString &path) const
{
    return QFileInfo(path + "/Data/Skyrim.esm").exists()
        || QFileInfo(path + "/Skyrim.exe").exists()
        || QFileInfo(path + "/SkyrimSE.exe").exists();
}

void FirstRunWizard::validatePage1()
{
    bool ok = looksLikeSkyrim(ui->gamePathEdit->text());
    ui->gamePathStatusLabel->setText(ok ? "✓ Skyrim installation detected" 
                                        : "✗ Not a Skyrim directory");
}

void FirstRunWizard::validatePage2()
{
    QString p = ui->workspacePathEdit->text();

    if (!QDir(p).exists())
        QDir().mkpath(p);

    bool ok = QDir(p).exists();

    if (!ok)
        ui->summaryLabel->setText("Workspace directory could not be created.");
}

void FirstRunWizard::initializePage(int id)
{
    QWizard::initializePage(id);

    // Page 2 is the summary page (name: page3)
    if (id == 2) {

        // Determine LOOT type from game path
        LootGameType detectedType = detectLootType(gamePath());

        // Build summary text
        QString summary =
            "Setup Complete!\n\n"
            "Game Path:\n" + gamePath() + "\n\n"
            "Workspace Path:\n" + workspacePath() + "\n\n"
            "Detected LOOT Type:\n" + QString::number((int)detectedType) + "\n";

        // Assign to the QLabel in page 3
        QLabel *summaryLbl = findChild<QLabel*>("summaryLabel");
        if (summaryLbl)
            summaryLbl->setText(summary);
    }
}
