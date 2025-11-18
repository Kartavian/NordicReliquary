#include "../ui/mainWindow.h"
#include "ui_mainWindow.h"
#include "../ui/pluginManager.h"
#include "downloadsPanel.h"
#include "detectLootType.h"
#include "lootManager.h"
#include "modManager.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>      // <-- REQUIRED
#include <QtWidgets/QStackedWidget>   // <-- REQUIRED
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QHeaderView>
#include <QIcon>
#include <QSize>
#include <QSizePolicy>
#include <QPushButton>
#include <QPixmap>
#include <QTransform>
#include <QTreeView>
#include <QFileSystemModel>
#include <QPlainTextEdit>
#include <QTabBar>
#include <QHoverEvent>
#include <QFileInfo>
#include <QEasingCurve>
#include <QSignalBlocker>
#include <QTextStream>
#include <QProcess>
#include <QProcessEnvironment>
#include <QHBoxLayout>
#include <QStandardPaths>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QSet>
#include <QVector>
#include <QDesktopServices>
#include <QUrl>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>
#include <QFile>
#include <memory>
#include <algorithm>
#include <functional>

#include <QtCore/QSettings>
#include <QtCore/QDir>
#include <QtCore/QDebug>

namespace {
QPixmap loadRotatedPixmap(const QString &path)
{
    QPixmap pix(path);
    if (pix.isNull())
        return pix;

    QTransform rotation;
    rotation.rotate(90); // rotate clockwise so icon aligns with vertical tabs

    return pix.transformed(rotation, Qt::SmoothTransformation);
}
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      activeGame(LootGameType_SkyrimSE)
{
    loadWorkspaceConfig();

    ui->setupUi(this);
    setupStyle();
    setupDataViews();

    if (ui->pluginListWidget) {
        if (QWidget *pluginsContainer = ui->pluginListWidget->parentWidget()) {
            if (auto layout = qobject_cast<QVBoxLayout*>(pluginsContainer->layout())) {
                QHBoxLayout *toolLayout = new QHBoxLayout();
                QLabel *toolLabel = new QLabel("Run with:", pluginsContainer);
                runToolCombo = new QComboBox(pluginsContainer);
                toolLayout->addWidget(toolLabel);
                toolLayout->addWidget(runToolCombo, 1);
                layout->insertLayout(0, toolLayout);
                connect(runToolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                        this, &MainWindow::onRunToolChanged);
            }
        }
    }

    // Ensure downloads panel has a layout
    if (ui->downloadsPanel && !ui->downloadsPanel->layout()) {
        ui->downloadsPanel->setLayout(new QVBoxLayout());
    }

    loadDataPath();
    activeGame = determineGameType(gameInstallPath.isEmpty() ? dataPath : gameInstallPath);
    qDebug() << "[INIT] Active game after detection:" << activeGame << "install path:" << installPath;
    refreshDataRoots();
    qDebug() << "[INIT] Data model roots refreshed for path:" << dataPath;

    if (ui->downloadsPanel) {
        ui->downloadsPanel->setDownloadsDirectory(downloadsPath);
        connect(ui->downloadsPanel, &DownloadsPanel::installRequested,
                this, &MainWindow::onInstallArchivesRequested);
    }

    connect(ui->changeFolderButton, &QPushButton::clicked,
            this, &MainWindow::onChangeFolderClicked);
    connect(ui->runButton, &QPushButton::clicked,
            this, &MainWindow::onRunButtonClicked);

    /* ─────────────────────────────────────────────────────────────
       CREATE STACKED WIDGETS (MOD MODE + LOOT MODE)
       ───────────────────────────────────────────────────────────── */

    leftStack = new QStackedWidget(this);
    leftStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rightStack = new QStackedWidget(this);
    rightStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    /* -------------------------------------------------------------
       MOD MODE - LEFT PANEL (your modsListWidget)
       ------------------------------------------------------------- */
    QWidget *modLeft = new QWidget();
    QVBoxLayout *modLeftLayout = new QVBoxLayout(modLeft);
    modLeftLayout->setContentsMargins(0,0,0,0);
    modLeftLayout->addWidget(ui->modsList);

    removeModButton = new QPushButton("Remove Mod", modLeft);
    connect(removeModButton, &QPushButton::clicked,
            this, &MainWindow::onRemoveModClicked);
    modLeftLayout->addWidget(removeModButton, 0, Qt::AlignLeft);

    /* MOD MODE - RIGHT PANEL (existing 5-tab panel) */
    QWidget *modRight = ui->tabWidget;

    QWidget *iniTab = new QWidget();
    QVBoxLayout *iniLayout = new QVBoxLayout(iniTab);
    iniEditor = new IniEditorWidget(this);
    iniLayout->addWidget(iniEditor);
    ui->tabWidget->addTab(iniTab, "INI Editor");

    leftStack->addWidget(modLeft); // index 0
    rightStack->addWidget(modRight); // index 0


    /* -------------------------------------------------------------
       LOOT MODE - LEFT PANEL (tabs: Metadata / Data / Errors)
       ------------------------------------------------------------- */

    QWidget *lootLeft = new QWidget();
    QVBoxLayout *lootLeftLayout = new QVBoxLayout(lootLeft);
    lootLeftLayout->setContentsMargins(0,0,0,0);

    sortPluginsButton = new QPushButton("Sort Plugins");
    sortPluginsButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(sortPluginsButton, &QPushButton::clicked,
            this, &MainWindow::onSortPluginsClicked);

    lootPluginList = new QListWidget();
    lootPluginList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(lootPluginList, &QListWidget::currentRowChanged,
            this, &MainWindow::displayLootMetadata);

    lootPluginName = new QLabel("Select a plugin to view metadata.");
    lootPluginType = new QLabel("Type: —");
    QLabel *mastersLabel = new QLabel("Masters");
    lootMasterList = new QListWidget();
    lootPluginDetailsView = new QTextBrowser();
    lootPluginDetailsView->setOpenExternalLinks(true);
    lootPluginDetailsView->setPlaceholderText("Plugin metadata will appear here once a masterlist is loaded.");

    QWidget *pluginDetailWidget = new QWidget();
    QVBoxLayout *pluginDetailLayout = new QVBoxLayout(pluginDetailWidget);
    pluginDetailLayout->addWidget(lootPluginName);
    pluginDetailLayout->addWidget(lootPluginType);
    pluginDetailLayout->addWidget(mastersLabel);
    pluginDetailLayout->addWidget(lootMasterList);
    pluginDetailLayout->addWidget(lootPluginDetailsView, 1);

    lootLeftLayout->addWidget(sortPluginsButton, 0, Qt::AlignLeft);
    lootLeftLayout->addWidget(lootPluginList, 3);
    lootLeftLayout->addWidget(pluginDetailWidget, 2);
    lootLeftLayout->setStretch(1, 3);
    lootLeftLayout->setStretch(2, 2);

    /* LOOT MODE - RIGHT PANEL (LOOT task tabs) */
    QWidget *lootRight = new QWidget();
    QVBoxLayout *lootRightLayout = new QVBoxLayout(lootRight);
    lootRightLayout->setContentsMargins(0,0,0,0);

    lootTaskTabs = new QTabWidget();

    QWidget *metadataTab = new QWidget();
    QVBoxLayout *metadataLayout = new QVBoxLayout(metadataTab);
    metadataLayout->setContentsMargins(0,0,0,0);
    masterlistVersionLabel = new QLabel("Masterlist version: Not downloaded");
    masterlistUpdatedLabel = new QLabel("Last updated: —");
    metadataLayout->addWidget(masterlistVersionLabel);
    metadataLayout->addWidget(masterlistUpdatedLabel);

    QHBoxLayout *masterlistButtons = new QHBoxLayout();
    downloadMasterlistButton = new QPushButton("Download Masterlist");
    updateMasterlistButton = new QPushButton("Update Masterlist");
    editUserRulesButton = new QPushButton("Edit User Rules");
    resetUserlistButton = new QPushButton("Reset Userlist");
    masterlistButtons->addWidget(downloadMasterlistButton);
    masterlistButtons->addWidget(updateMasterlistButton);
    masterlistButtons->addWidget(editUserRulesButton);
    masterlistButtons->addWidget(resetUserlistButton);
    metadataLayout->addLayout(masterlistButtons);
    metadataLayout->addStretch(1);

    connect(downloadMasterlistButton, &QPushButton::clicked,
            this, &MainWindow::onDownloadMasterlistClicked);
    connect(updateMasterlistButton, &QPushButton::clicked,
            this, &MainWindow::onUpdateMasterlistClicked);
    connect(editUserRulesButton, &QPushButton::clicked,
            this, &MainWindow::onEditUserRulesClicked);
    connect(resetUserlistButton, &QPushButton::clicked,
            this, &MainWindow::onResetUserlistClicked);

    lootTaskTabs->addTab(metadataTab, "Metadata");

    QWidget *lootDataTab = new QWidget();
    QVBoxLayout *lootDataLayout = new QVBoxLayout(lootDataTab);
    lootDataLayout->setContentsMargins(0,0,0,0);
    if (lootDataView)
        lootDataLayout->addWidget(lootDataView);
    lootTaskTabs->addTab(lootDataTab, "Data");

    QWidget *warningsTab = new QWidget();
    QVBoxLayout *warningsLayout = new QVBoxLayout(warningsTab);
    warningsLayout->setContentsMargins(0,0,0,0);
    lootWarningsTable = new QTableWidget(0, 3);
    lootWarningsTable->setHorizontalHeaderLabels({"Plugin", "Type", "Details"});
    lootWarningsTable->horizontalHeader()->setStretchLastSection(true);
    lootWarningsTable->verticalHeader()->setVisible(false);
    lootWarningsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    lootWarningsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    lootWarningsTable->setAlternatingRowColors(true);
    lootWarningsTable->setSortingEnabled(true);
    warningsLayout->addWidget(lootWarningsTable, 3);

    lootReportOutput = new QPlainTextEdit();
    lootReportOutput->setReadOnly(true);
    lootReportOutput->setPlaceholderText("LOOT output will appear here.");
    warningsLayout->addWidget(lootReportOutput, 1);

    lootTaskTabs->addTab(warningsTab, "Errors / Warnings");

    lootRightLayout->addWidget(lootTaskTabs);

    leftStack->addWidget(lootLeft); // index 1 (plugins list)
    rightStack->addWidget(lootRight); // index 1 (LOOT task tabs)

    /* ─────────────────────────────────────────────────────────────
       MODE SWITCHER TABS WITH ICONS
       ───────────────────────────────────────────────────────────── */

    modeTabs = new QTabWidget(this);
    modeTabs->setTabPosition(QTabWidget::West);
    modeTabs->setIconSize(QSize(32, 32));
    modeTabs->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    modeTabs->setMinimumWidth(72);
    modeTabs->setMaximumWidth(96);
    modeTabs->setStyleSheet(R"(
        QTabWidget::pane { border: 0; }
        QTabBar::tab {
            background: transparent;
            padding: 8px;
            margin: 4px;
        }
        QTabBar::tab:selected {
            background: rgba(255,255,255,0.10);
            border-radius: 6px;
        }
    )");

    //
    // Create the tabs with icons
    //
    modeTabs->addTab(new QWidget(), QIcon(), "");   // index 0 = MOD MODE
    modeTabs->setTabToolTip(0, "Mod Management Mode");

    modeTabs->addTab(new QWidget(), QIcon(), "");  // index 1 = LOOT MODE
    modeTabs->setTabToolTip(1, "LOOT Sorting Mode");

    // Pick icon set based on detected game
    updateModeTabIcons();

    //
    // Connect tab switch to stacked widget swap
    //
    connect(modeTabs, &QTabWidget::currentChanged, this, [this](int index) {
        leftStack->setCurrentIndex(index);
        rightStack->setCurrentIndex(index);

        if (index == 0)
            qDebug() << "[MODE] Mod Mode activated";
        else
            qDebug() << "[MODE] LOOT Mode activated";
    });

    if (auto *tabBar = modeTabs->tabBar()) {
        tabBar->setAttribute(Qt::WA_Hover, true);
        tabBar->setMouseTracking(true);
        tabBar->installEventFilter(this);
    }

    /* ─────────────────────────────────────────────────────────────
       INSERT STACKED WIDGETS INTO EXISTING UI LAYOUT
       ───────────────────────────────────────────────────────────── */

    QHBoxLayout *contentLayout = ui->contentLayout;

    if (!contentLayout) {
        qWarning() << "[ERROR] The MainWindow.ui content layout is not available!";
        return;
    }

    // Remove original widgets from layout
    contentLayout->removeWidget(ui->modsList);
    contentLayout->removeWidget(ui->tabWidget);

    // Insert new dynamic mode switching containers
    contentLayout->insertWidget(0, modeTabs);
    contentLayout->insertWidget(1, leftStack);
    contentLayout->insertWidget(2, rightStack);
    contentLayout->setSpacing(8);
    contentLayout->setStretch(0, 0);
    contentLayout->setStretch(1, 1);
    contentLayout->setStretch(2, 1);

    qDebug() << "[INIT] Recreating LOOT manager";
    recreateLootManager();
    qDebug() << "[INIT] LOOT manager step done";

    initializeModManager();
    refreshModsList();
    rescanVirtualPlugins();
    updateIniEditorSources();
    refreshMasterlistInfoLabels();
    reloadLootMetadata();

    /* ─────────────────────────────────────────────────────────────
       DEFAULT MODE: MOD MODE (index = 0)
       ───────────────────────────────────────────────────────────── */
    leftStack->setCurrentIndex(0);
    rightStack->setCurrentIndex(0);

    qDebug() << "[DEBUG] MainWindow initialized with MOD MODE active.";
}


MainWindow::~MainWindow()
{
    delete ui;
}


/* ─────────────────────────────────────────────────────────────
   STYLE SETUP
   ───────────────────────────────────────────────────────────── */
void MainWindow::setupStyle()
{
    qDebug() << "[DEBUG] Applying lavender/dark purple theme";

    QString style = R"(
        QWidget {
            background-color: #2b0033;
            color: #E0E0E0;
        }

        QPushButton {
            background-color: #C084FC;
            color: #1E1E1E;
            border-radius: 6px;
            padding: 6px;
        }

        QPushButton:hover {
            background-color: #E5B0FF;
        }

        QListWidget {
            background-color: #3B0B3B;
            color: white;
            border: 1px solid #6A0DAD;
        }

        QTabWidget::pane {
            background-color: #3B0B3B;
            border: 2px solid #6A0DAD;
        }

        QTabBar::tab {
            background-color: #2E003E;
            color: #C084FC;
            padding: 6px;
            margin: 2px;
            border-radius: 4px;
        }

        QTabBar::tab:selected {
            background-color: #C084FC;
            color: #2E003E;
        }
    )";

    setStyleSheet(style);
}

bool MainWindow::loadWorkspaceConfig()
{
    QString configFile = QDir::homePath() + "/.config/NordicReliquary/config.ini";
    QFile f(configFile);
    if (f.open(QIODevice::ReadOnly)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            const QString line = in.readLine();
            const int idx = line.indexOf('=');
            if (idx <= 0)
                continue;
            const QString key = line.left(idx).trimmed();
            const QString value = line.mid(idx + 1).trimmed();
            if (key == "workspacePath")
                workspacePath = value;
            else if (key == "gamePath")
                gameInstallPath = value;
        }
    }

    if (workspacePath.isEmpty())
        workspacePath = QDir::currentPath() + "/Workspace";
    if (gameInstallPath.isEmpty())
        gameInstallPath = installPath;

    downloadsPath = workspacePath + "/Downloads";
    modsPath = workspacePath + "/Mods";
    virtualDataPath = workspacePath + "/VirtualData";

    QDir().mkpath(workspacePath);
    QDir().mkpath(downloadsPath);
    QDir().mkpath(modsPath);
    QDir().mkpath(virtualDataPath);

    lootDataRoot = workspacePath + "/LootData";
    QDir().mkpath(lootDataRoot + "/masterlists");
    QDir().mkpath(lootDataRoot + "/userlists");

    if (!gameInstallPath.isEmpty())
        installPath = gameInstallPath;

    return true;
}

void MainWindow::saveConfigPaths() const
{
    QString configFile = QDir::homePath() + "/.config/NordicReliquary/config.ini";
    QDir().mkpath(QFileInfo(configFile).absolutePath());
    QFile f(configFile);
    if (!f.open(QIODevice::WriteOnly))
        return;

    QTextStream out(&f);
    out << "gamePath=" << gameInstallPath << "\n";
    out << "workspacePath=" << workspacePath << "\n";
}

void MainWindow::setupDataViews()
{
    if (dataModel)
        return;

    dataModel = new QFileSystemModel(this);
    dataModel->setReadOnly(true);
    dataModel->setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Files);

    modDataView = new QTreeView();
    modDataView->setModel(dataModel);
    modDataView->setUniformRowHeights(true);

    auto *modDataLayout = new QVBoxLayout();
    modDataLayout->setContentsMargins(0,0,0,0);
    modDataLayout->addWidget(modDataView);
    ui->tabData->setLayout(modDataLayout);

    lootDataView = new QTreeView();
    lootDataView->setModel(dataModel);
    lootDataView->setUniformRowHeights(true);
}


/* ─────────────────────────────────────────────────────────────
   LOAD SKYRIM DATA PATH
   ───────────────────────────────────────────────────────────── */
void MainWindow::loadDataPath() {
    if (!virtualDataPath.isEmpty()) {
        dataPath = virtualDataPath;
        return;
    }

    QSettings settings("Kartavian", "NordicMod");
    dataPath = settings.value("dataPath").toString();
}

void MainWindow::saveDataPath(const QString &path) {
    QSettings settings("Kartavian", "NordicMod");
    settings.setValue("dataPath", path);
}

void MainWindow::refreshDataRoots()
{
    if (!dataModel)
        return;

    QString effectivePath = dataPath;
    if (effectivePath.isEmpty())
        effectivePath = QDir::homePath();

    QModelIndex rootIndex = dataModel->setRootPath(effectivePath);
    if (modDataView)
        modDataView->setRootIndex(rootIndex);
    if (lootDataView)
        lootDataView->setRootIndex(rootIndex);
}

void MainWindow::recreateLootManager()
{
    lootManager.reset();

    if (dataPath.isEmpty() || installPath.isEmpty()) {
        if (sortPluginsButton)
            sortPluginsButton->setEnabled(false);
        return;
    }

    qDebug() << "[LOOT] Creating manager for data" << dataPath << "install" << installPath << "game" << activeGame;
    lootManager = std::make_unique<LootManager>(dataPath, installPath, activeGame);
    qDebug() << "[LOOT] Manager create attempt finished";
    bool ready = lootManager && lootManager->isValid();
    if (!ready)
        lootManager.reset();

    if (sortPluginsButton)
        sortPluginsButton->setEnabled(ready);
}

void MainWindow::displayLootMetadata(int index)
{
    if (!lootPluginName || !lootPluginType || !lootMasterList) {
        return;
    }

    if (index < 0 || index >= static_cast<int>(cachedPlugins.size())) {
        lootPluginName->setText("Select a plugin to view metadata.");
        lootPluginType->setText("Type: —");
        lootMasterList->clear();
        if (lootPluginDetailsView)
            lootPluginDetailsView->setHtml("<p>No plugin selected.</p>");
        return;
    }

    const PluginInfo &plugin = cachedPlugins.at(index);
    QString displayName = QString::fromStdString(!plugin.name.empty() ? plugin.name : plugin.filename);
    lootPluginName->setText(displayName);
    lootPluginType->setText(QString("Type: %1").arg(QString::fromStdString(plugin.type)));

    lootMasterList->clear();
    if (plugin.masters.empty()) {
        lootMasterList->addItem("No masters detected");
    } else {
        for (const auto &master : plugin.masters) {
            lootMasterList->addItem(QString::fromStdString(master));
        }
    }

    if (lootPluginDetailsView)
        lootPluginDetailsView->setHtml(buildPluginMetadataHtml(plugin));
}

void MainWindow::appendLootReport(const QString &line)
{
    if (!lootReportOutput)
        return;

    lootReportOutput->appendPlainText(line);
}

void MainWindow::reloadLootMetadata()
{
    if (!lootManager)
        return;

    ensureLootDataFolders();
    lootPluginDetailsCache.clear();
    lootGeneralMessages = QJsonArray();

    QString masterlistPath = masterlistFilePath();
    QString preludePath = masterlistPreludePath();
    bool masterlistLoaded = false;
    if (!masterlistPath.isEmpty() && QFileInfo::exists(masterlistPath)) {
        masterlistLoaded = lootManager->loadMasterlist(masterlistPath, preludePath);
    }

    QString userlistPath = userlistPathForActiveGame();
    if (!userlistPath.isEmpty() && QFileInfo::exists(userlistPath))
        lootManager->loadUserlist(userlistPath);

    if (!masterlistLoaded) {
        rebuildWarningsTable();
        if (lootPluginDetailsView)
            lootPluginDetailsView->setHtml("<p>Download a masterlist to view LOOT metadata.</p>");
        return;
    }

    for (const PluginInfo &plugin : cachedPlugins) {
        QString pluginName = QString::fromStdString(plugin.filename);
        QJsonObject detail = lootManager->pluginDetails(pluginName);
        if (!detail.isEmpty())
            lootPluginDetailsCache.insert(normalizedPluginKey(pluginName), detail);
    }

    lootGeneralMessages = lootManager->generalMessages();
    rebuildWarningsTable();

    if (lootPluginList && lootPluginList->currentRow() >= 0)
        displayLootMetadata(lootPluginList->currentRow());
}

void MainWindow::refreshMasterlistInfoLabels()
{
    if (!masterlistVersionLabel || !masterlistUpdatedLabel)
        return;

    QString repoDir = masterlistDirectory();
    QDir repo(repoDir);
    if (!repo.exists()) {
        masterlistVersionLabel->setText("Masterlist version: Not downloaded");
        masterlistUpdatedLabel->setText("Last updated: —");
        return;
    }

    QString version = runGitForOutput({"rev-parse", "--short", "HEAD"}, repoDir);
    if (version.isEmpty())
        version = "Unknown";
    masterlistVersionLabel->setText(QString("Masterlist version: %1").arg(version));

    QString updated = runGitForOutput({"log", "-1", "--format=%ci"}, repoDir);
    if (updated.isEmpty())
        updated = "Unknown";
    masterlistUpdatedLabel->setText(QString("Last updated: %1").arg(updated));
}

QString MainWindow::lootGameSlug() const
{
    switch (activeGame) {
    case LootGameType_SkyrimSE:
        return QStringLiteral("skyrimse");
    case LootGameType_Skyrim:
        return QStringLiteral("skyrim");
    case LootGameType_Fallout4:
        return QStringLiteral("fallout4");
    case LootGameType_Fallout3:
        return QStringLiteral("fallout3");
    case LootGameType_FalloutNV:
        return QStringLiteral("falloutnv");
    case LootGameType_Oblivion:
        return QStringLiteral("oblivion");
    case LootGameType_Morrowind:
        return QStringLiteral("morrowind");
    case LootGameType_OpenMW:
        return QStringLiteral("openmw");
    default:
        return QStringLiteral("skyrimse");
    }
}

QString MainWindow::masterlistRepoUrl() const
{
    QString slug = lootGameSlug();
    if (slug.isEmpty())
        return QString();
    return QStringLiteral("https://github.com/loot/%1").arg(slug);
}

QString MainWindow::masterlistDirectory() const
{
    if (lootDataRoot.isEmpty())
        return QString();
    QString slug = lootGameSlug();
    if (slug.isEmpty())
        return QString();
    QString dir = lootDataRoot + "/masterlists/" + slug;
    QDir().mkpath(dir);
    return dir;
}

QString MainWindow::masterlistFilePath() const
{
    QString dir = masterlistDirectory();
    if (dir.isEmpty())
        return QString();
    return dir + "/masterlist.yaml";
}

QString MainWindow::masterlistPreludePath() const
{
    QString dir = masterlistDirectory();
    if (dir.isEmpty())
        return QString();
    QString path = dir + "/prelude.yaml";
    return QFileInfo::exists(path) ? path : QString();
}

QString MainWindow::userlistPathForActiveGame() const
{
    if (lootDataRoot.isEmpty())
        return QString();
    QString slug = lootGameSlug();
    if (slug.isEmpty())
        return QString();
    QString dir = lootDataRoot + "/userlists/" + slug;
    QDir().mkpath(dir);
    return dir + "/userlist.yaml";
}

void MainWindow::ensureLootDataFolders() const
{
    if (lootDataRoot.isEmpty())
        return;
    QString slug = lootGameSlug();
    if (slug.isEmpty())
        return;
    QDir().mkpath(lootDataRoot + "/masterlists/" + slug);
    QDir().mkpath(lootDataRoot + "/userlists/" + slug);
}

bool MainWindow::runGitCommand(const QStringList &args, const QString &workingDir, const QString &description)
{
    QProcess git;
    git.setProgram(QStringLiteral("git"));
    git.setArguments(args);
    git.setWorkingDirectory(workingDir);
    git.start();
    if (!git.waitForFinished(-1)) {
        appendLootReport(QString("Git %1 did not finish: %2").arg(description, git.errorString()));
        return false;
    }

    const QString stdoutText = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
    const QString stderrText = QString::fromUtf8(git.readAllStandardError()).trimmed();
    if (!stdoutText.isEmpty())
        appendLootReport(stdoutText);
    if (!stderrText.isEmpty())
        appendLootReport(stderrText);

    bool ok = git.exitStatus() == QProcess::NormalExit && git.exitCode() == 0;
    appendLootReport(ok ? QString("%1 succeeded.").arg(description)
                        : QString("%1 failed (exit %2).").arg(description).arg(git.exitCode()));
    return ok;
}

QString MainWindow::runGitForOutput(const QStringList &args, const QString &workingDir) const
{
    QProcess git;
    git.setProgram(QStringLiteral("git"));
    git.setArguments(args);
    git.setWorkingDirectory(workingDir);
    git.start();
    if (!git.waitForFinished(5000))
        return QString();
    if (git.exitStatus() != QProcess::NormalExit || git.exitCode() != 0)
        return QString();
    return QString::fromUtf8(git.readAllStandardOutput()).trimmed();
}

QString MainWindow::normalizedPluginKey(const QString &name) const
{
    QString key = name;
    return key.toLower();
}

void MainWindow::rebuildWarningsTable()
{
    if (!lootWarningsTable)
        return;

    struct WarningEntry {
        QString plugin;
        QString type;
        QString message;
    };

    QVector<WarningEntry> entries;
    QSet<QString> knownPlugins;
    for (const PluginInfo &plugin : cachedPlugins) {
        knownPlugins.insert(normalizedPluginKey(QString::fromStdString(plugin.filename)));
    }

    auto appendEntry = [&](const QString &plugin, const QString &type, const QString &message) {
        if (message.isEmpty())
            return;
        entries.push_back({plugin, type, message});
    };

    for (const PluginInfo &plugin : cachedPlugins) {
        QString pluginName = QString::fromStdString(plugin.filename);
        QString key = normalizedPluginKey(pluginName);
        QJsonObject detail = lootPluginDetailsCache.value(key);
        if (detail.isEmpty())
            continue;

        for (const QJsonValue &value : detail.value("messages").toArray()) {
            QJsonObject obj = value.toObject();
            appendEntry(pluginName,
                        obj.value("level").toString("info"),
                        obj.value("text").toString());
        }

        if (detail.value("has_user_metadata").toBool())
            appendEntry(pluginName, "User Override", QStringLiteral("User rules are applied to this plugin."));

        for (const QJsonValue &value : detail.value("dirty").toArray()) {
            QJsonObject obj = value.toObject();
            QString summary = QStringLiteral("Utility %1 | CRC %2 | ITM %3 | UDR %4 | NAV %5")
                                  .arg(obj.value("utility").toString())
                                  .arg(obj.value("crc").toString())
                                  .arg(obj.value("itm").toInt())
                                  .arg(obj.value("deleted_references").toInt())
                                  .arg(obj.value("deleted_navmeshes").toInt());
            QString detailText = obj.value("detail").toString();
            if (!detailText.isEmpty())
                summary += QStringLiteral(" | %1").arg(detailText);
            appendEntry(pluginName, "Dirty", summary);
        }

        QJsonArray requirements = detail.value("requirements").toArray();
        for (const QJsonValue &value : requirements) {
            QJsonObject obj = value.toObject();
            QString required = obj.value("name").toString();
            if (required.isEmpty())
                continue;
            if (!knownPlugins.contains(required.toLower())) {
                appendEntry(pluginName,
                            "Missing Master",
                            QStringLiteral("Requires %1, which is not present.").arg(required));
            }
        }
    }

    for (const QJsonValue &value : lootGeneralMessages) {
        QJsonObject obj = value.toObject();
        appendEntry("General", obj.value("level").toString("info"), obj.value("text").toString());
    }

    std::sort(entries.begin(), entries.end(), [](const WarningEntry &lhs, const WarningEntry &rhs) {
        int compare = lhs.plugin.compare(rhs.plugin, Qt::CaseInsensitive);
        if (compare == 0)
            return lhs.type < rhs.type;
        return compare < 0;
    });

    lootWarningsTable->setSortingEnabled(false);
    lootWarningsTable->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        lootWarningsTable->setItem(row, 0, new QTableWidgetItem(entries[row].plugin));
        lootWarningsTable->setItem(row, 1, new QTableWidgetItem(entries[row].type));
        QTableWidgetItem *detailItem = new QTableWidgetItem(entries[row].message);
        detailItem->setToolTip(entries[row].message);
        lootWarningsTable->setItem(row, 2, detailItem);
    }
    lootWarningsTable->resizeColumnsToContents();
    lootWarningsTable->setSortingEnabled(true);
}

QString MainWindow::buildPluginMetadataHtml(const PluginInfo &plugin) const
{
    QString pluginName = QString::fromStdString(plugin.filename);
    QString key = normalizedPluginKey(pluginName);
    QJsonObject detail = lootPluginDetailsCache.value(key);
    if (detail.isEmpty())
        return QStringLiteral("<p>No LOOT metadata available for %1.</p>").arg(pluginName.toHtmlEscaped());

    auto renderList = [](const QJsonArray &array,
                         const std::function<QString(const QJsonObject &)> &builder) {
        if (array.isEmpty())
            return QString();
        QString html("<ul>");
        for (const QJsonValue &value : array) {
            QJsonObject obj = value.toObject();
            html += QStringLiteral("<li>%1</li>").arg(builder(obj));
        }
        html += QStringLiteral("</ul>");
        return html;
    };

    QString html;
    QString displayName = detail.value("name").toString(pluginName);
    html += QStringLiteral("<h3>%1</h3>").arg(displayName.toHtmlEscaped());
    html += QStringLiteral("<p><b>Masterlist entry:</b> %1</p>")
                .arg(detail.value("has_masterlist").toBool() ? QStringLiteral("Yes") : QStringLiteral("No"));
    html += QStringLiteral("<p><b>User rules:</b> %1</p>")
                .arg(detail.value("has_user_metadata").toBool() ? QStringLiteral("Yes") : QStringLiteral("No"));

    const QString group = detail.value("group").toString();
    if (!group.isEmpty())
        html += QStringLiteral("<p><b>Group:</b> %1</p>").arg(group.toHtmlEscaped());

    auto fileBuilder = [](const QJsonObject &obj) {
        QString text = obj.value("name").toString();
        QString display = obj.value("display").toString();
        if (!display.isEmpty())
            text = display;
        if (!obj.value("detail").toString().isEmpty())
            text += QStringLiteral(" (%1)").arg(obj.value("detail").toString());
        return text.toHtmlEscaped();
    };

    QJsonArray loadAfter = detail.value("load_after").toArray();
    if (!loadAfter.isEmpty())
        html += QStringLiteral("<h4>Load After</h4>%1").arg(renderList(loadAfter, fileBuilder));

    QJsonArray requirements = detail.value("requirements").toArray();
    if (!requirements.isEmpty())
        html += QStringLiteral("<h4>Requirements</h4>%1").arg(renderList(requirements, fileBuilder));

    QJsonArray incompatibilities = detail.value("incompatibilities").toArray();
    if (!incompatibilities.isEmpty())
        html += QStringLiteral("<h4>Conflicts</h4>%1").arg(renderList(incompatibilities, fileBuilder));

    QJsonArray tags = detail.value("tags").toArray();
    if (!tags.isEmpty()) {
        html += QStringLiteral("<h4>Bash Tags</h4>");
        html += renderList(tags, [](const QJsonObject &obj) {
            QString entry = obj.value("name").toString();
            QString suggestion = obj.value("suggestion").toString();
            if (!suggestion.isEmpty())
                entry += QStringLiteral(" (%1)").arg(suggestion);
            return entry.toHtmlEscaped();
        });
    }

    QJsonArray messages = detail.value("messages").toArray();
    if (!messages.isEmpty()) {
        html += QStringLiteral("<h4>Messages</h4>");
        html += renderList(messages, [](const QJsonObject &obj) {
            QString entry = obj.value("level").toString();
            entry = entry.toUpper();
            entry += QStringLiteral(": %1").arg(obj.value("text").toString());
            return entry.toHtmlEscaped();
        });
    }

    QJsonArray dirty = detail.value("dirty").toArray();
    if (!dirty.isEmpty()) {
        html += QStringLiteral("<h4>Dirty Info</h4>");
        html += renderList(dirty, [](const QJsonObject &obj) {
            QString summary = QStringLiteral("%1 (CRC %2, ITM %3, UDR %4, NAV %5)")
                                   .arg(obj.value("utility").toString())
                                   .arg(obj.value("crc").toString())
                                   .arg(obj.value("itm").toInt())
                                   .arg(obj.value("deleted_references").toInt())
                                   .arg(obj.value("deleted_navmeshes").toInt());
            QString detail = obj.value("detail").toString();
            if (!detail.isEmpty())
                summary += QStringLiteral(" - %1").arg(detail);
            return summary.toHtmlEscaped();
        });
    }

    QJsonArray clean = detail.value("clean").toArray();
    if (!clean.isEmpty()) {
        html += QStringLiteral("<h4>Clean Info</h4>");
        html += renderList(clean, [](const QJsonObject &obj) {
            QString summary = QStringLiteral("%1 (CRC %2)")
                                   .arg(obj.value("utility").toString())
                                   .arg(obj.value("crc").toString());
            QString detail = obj.value("detail").toString();
            if (!detail.isEmpty())
                summary += QStringLiteral(" - %1").arg(detail);
            return summary.toHtmlEscaped();
        });
    }

    return html;
}

void MainWindow::onDownloadMasterlistClicked()
{
    ensureLootDataFolders();
    QString repoUrl = masterlistRepoUrl();
    if (repoUrl.isEmpty()) {
        appendLootReport("No masterlist repository is configured for this game.");
        return;
    }

    QString targetDir = masterlistDirectory();
    if (targetDir.isEmpty())
        return;

    QDir dir(targetDir);
    if (dir.exists()) {
        if (!dir.isEmpty()) {
            appendLootReport("Masterlist already downloaded. Use Update Masterlist instead.");
            return;
        }
        dir.removeRecursively();
    }

    QFileInfo info(targetDir);
    QDir parent = info.absoluteDir();
    QDir().mkpath(parent.absolutePath());
    QString localName = info.fileName();
    QStringList args = {"clone", repoUrl, localName};
    if (runGitCommand(args, parent.absolutePath(), "Download masterlist")) {
        refreshMasterlistInfoLabels();
        reloadLootMetadata();
    }
}

void MainWindow::onUpdateMasterlistClicked()
{
    ensureLootDataFolders();
    QString repoDir = masterlistDirectory();
    QDir dir(repoDir);
    if (!dir.exists() || dir.isEmpty()) {
        appendLootReport("Masterlist not found. Downloading a fresh copy...");
        onDownloadMasterlistClicked();
        return;
    }

    if (runGitCommand({"pull", "--ff-only"}, repoDir, "Update masterlist")) {
        refreshMasterlistInfoLabels();
        reloadLootMetadata();
    }
}

void MainWindow::onEditUserRulesClicked()
{
    QString path = userlistPathForActiveGame();
    if (path.isEmpty())
        return;

    QFileInfo info(path);
    QDir().mkpath(info.absolutePath());
    if (!info.exists()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write("plugins: []\n");
            file.close();
        }
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        appendLootReport("Unable to open userlist in an editor. Please open it manually at " + path);
}

void MainWindow::onResetUserlistClicked()
{
    QString path = userlistPathForActiveGame();
    if (path.isEmpty())
        return;

    if (QFile::exists(path) && !QFile::remove(path)) {
        appendLootReport("Failed to reset userlist. Check file permissions.");
        return;
    }

    if (lootManager)
        lootManager->clearUserMetadata();

    appendLootReport("Userlist reset.");
    reloadLootMetadata();
}

/* ─────────────────────────────────────────────────────────────
   POPULATE PLUGIN LIST
───────────────────────────────────────────────────────────── */
void MainWindow::populatePluginList(const std::vector<PluginInfo>& plugins)
{
    qDebug() << "[UI] populatePluginList start. size=" << plugins.size();
    if (!ui->pluginListWidget) {
        qDebug() << "[ERROR] pluginListWidget not found in UI.";
        return;
    }

    cachedPlugins = plugins;

    ui->pluginListWidget->clear();
    bool lootListAvailable = lootPluginList != nullptr;
    if (lootListAvailable)
        lootPluginList->clear();

    auto addListItem = [](QListWidget *list, const PluginInfo &plugin) {
        QString base = QString::fromStdString(plugin.filename);
        QListWidgetItem *item = new QListWidgetItem(base);

        if (plugin.type == "ESM")
            item->setForeground(Qt::lightGray);
        else if (plugin.type == "ESP")
            item->setForeground(Qt::green);
        else if (plugin.type == "ESL")
            item->setForeground(Qt::cyan);

        list->addItem(item);
    };

    for (const auto &plugin : cachedPlugins) {
        qDebug() << "[UI] adding plugin" << QString::fromStdString(plugin.filename);
        QString label = QString::fromStdString(plugin.filename + " [" + plugin.type + "]");
        QListWidgetItem *item = new QListWidgetItem(label);

        if (plugin.type == "ESM")
            item->setForeground(Qt::lightGray);
        else if (plugin.type == "ESP")
            item->setForeground(Qt::green);
        else if (plugin.type == "ESL")
            item->setForeground(Qt::cyan);

        ui->pluginListWidget->addItem(item);

        if (lootListAvailable)
            addListItem(lootPluginList, plugin);
    }

    if (lootListAvailable) {
        if (!cachedPlugins.empty()) {
            lootPluginList->setCurrentRow(0);
            displayLootMetadata(0);
        } else {
            lootPluginList->clearSelection();
            displayLootMetadata(-1);
        }
    }

    reloadLootMetadata();
    qDebug() << "[DEBUG] Populated plugin list with" << plugins.size() << "items.";
}

void MainWindow::initializeModManager()
{
    if (workspacePath.isEmpty() || virtualDataPath.isEmpty())
        return;

    modManager = std::make_unique<ModManager>(workspacePath, installPath, virtualDataPath, this);
    modManager->setDownloadsRoot(downloadsPath);

    QString error;
    if (!modManager->initialize(&error) && !error.isEmpty()) {
        qWarning() << "[ModManager] Initialization issue:" << error;
    }

    connect(modManager.get(), &ModManager::modsChanged,
            this, &MainWindow::refreshModsList);
}

void MainWindow::refreshModsList()
{
    if (!ui->modsList || !modManager)
        return;

    QSignalBlocker blocker(ui->modsList);
    ui->modsList->clear();

    for (const ModRecord &record : modManager->mods()) {
        QListWidgetItem *item = new QListWidgetItem(record.name, ui->modsList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(record.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, record.id);
        if (record.type == ModType::ToolMod)
            item->setText(record.name + " (Tool)");
    }

    QObject::connect(ui->modsList, &QListWidget::itemChanged,
                     this, &MainWindow::onModItemChanged,
                     Qt::UniqueConnection);

    updateToolLaunchers();
}

void MainWindow::updateToolLaunchers()
{
    if (!runToolCombo)
        return;

    toolEntries.clear();

    QString baseExe = defaultGameExecutable();
    QString basePath = (gameInstallPath.isEmpty() ? installPath : gameInstallPath) + "/" + baseExe;

    ToolEntry base;
    base.id = QStringLiteral("builtin:game");
    base.label = baseExe;
    base.exePath = "steam";
    base.args = {"-applaunch", QString::number(steamAppId())};
    base.type = ToolEntryType::Game;
    toolEntries.push_back(base);

    if (modManager) {
        for (const ModRecord &record : modManager->mods()) {
            if (record.type != ModType::ToolMod)
                continue;
            if (record.launcherPath.isEmpty())
                continue;
            ToolEntry entry;
            entry.id = record.id;
            entry.label = record.name;
            entry.exePath = record.launcherPath;
            entry.args = record.launcherArgs.split(' ', Qt::SkipEmptyParts);
            entry.type = ToolEntryType::Tool;
            toolEntries.push_back(entry);
        }
    }

    QSignalBlocker blocker(runToolCombo);
    runToolCombo->clear();

    QSettings settings("Kartavian", "NordicMod");
    QString preferred = settings.value("lastToolId").toString();
    int preferredIndex = -1;

    for (size_t i = 0; i < toolEntries.size(); ++i) {
        runToolCombo->addItem(toolEntries[i].label, toolEntries[i].id);
        if (toolEntries[i].id == preferred)
            preferredIndex = static_cast<int>(i);
    }

    if (preferredIndex < 0) {
        for (size_t i = 0; i < toolEntries.size(); ++i) {
            if (toolEntries[i].label.contains("skse", Qt::CaseInsensitive)) {
                preferredIndex = static_cast<int>(i);
                break;
            }
        }
    }

    if (preferredIndex < 0 && !toolEntries.empty())
        preferredIndex = 0;

    if (preferredIndex >= 0) {
        runToolCombo->setCurrentIndex(preferredIndex);
        selectedToolId = toolEntries[preferredIndex].id;
        if (preferred.isEmpty())
            settings.setValue("lastToolId", selectedToolId);
    } else {
        selectedToolId.clear();
    }
}

void MainWindow::updateIniEditorSources()
{
    if (!iniEditor)
        return;

    QList<QPair<QString, QString>> roots;
    if (!gameInstallPath.isEmpty())
        roots.append({gameInstallPath, "Skyrim"});

    if (!virtualDataPath.isEmpty())
        roots.append({virtualDataPath, "Virtual Data"});

    if (!modsPath.isEmpty())
        roots.append({modsPath, "Mods"});

    iniEditor->setIniRoots(roots);
}

void MainWindow::rescanVirtualPlugins()
{
    if (dataPath.isEmpty())
        return;

    qDebug() << "[INIT] Scanning plugins in" << dataPath;
    PluginManager pluginManager;
    pluginManager.scan(dataPath.toStdString());
    populatePluginList(pluginManager.getPlugins());
    qDebug() << "[INIT] Plugin scan complete";
}

void MainWindow::onInstallArchivesRequested(const QStringList &archives)
{
    if (!modManager)
        return;

    for (const QString &name : archives) {
        QString archivePath = downloadsPath + "/" + name;
        ModRecord record;
        QString error;
        if (!modManager->installArchive(archivePath, &record, &error)) {
            qWarning() << "[ModManager] Install failed for" << name << ":" << error;
            continue;
        }
    }

    refreshModsList();
    rescanVirtualPlugins();
}

void MainWindow::onModItemChanged(QListWidgetItem *item)
{
    if (!modManager || !item)
        return;

    QString modId = item->data(Qt::UserRole).toString();
    bool enabled = item->checkState() == Qt::Checked;
    QString error;
    if (!modManager->setModEnabled(modId, enabled, &error)) {
        qWarning() << "[ModManager] Failed to toggle mod:" << error;
        return;
    }

    rescanVirtualPlugins();
}

void MainWindow::onRemoveModClicked()
{
    if (!modManager || !ui->modsList)
        return;

    QListWidgetItem *current = ui->modsList->currentItem();
    if (!current)
        return;

    QString modId = current->data(Qt::UserRole).toString();
    if (modId.isEmpty())
        return;

    QString error;
    if (!modManager->removeMod(modId, &error)) {
        qWarning() << "[ModManager] Unable to remove mod:" << error;
        return;
    }

    refreshModsList();
    rescanVirtualPlugins();
}

void MainWindow::onRunToolChanged(int index)
{
    if (!runToolCombo || index < 0 || index >= static_cast<int>(toolEntries.size()))
        return;

    selectedToolId = toolEntries[index].id;
    QSettings settings("Kartavian", "NordicMod");
    settings.setValue("lastToolId", selectedToolId);
}

void MainWindow::onRunButtonClicked()
{
    if (toolEntries.empty()) {
        appendLootReport("No launch targets available.");
        return;
    }

    int comboIndex = runToolCombo ? runToolCombo->currentIndex() : 0;
    if (comboIndex < 0 || comboIndex >= static_cast<int>(toolEntries.size()))
        comboIndex = 0;

    ToolEntry entry = toolEntries[comboIndex];
    QString execPath = entry.exePath;
    QFileInfo execInfo(execPath);
    bool isTool = entry.type == ToolEntryType::Tool;
    qDebug() << "[Run] Selected entry:" << entry.label
             << "type" << (isTool ? "tool" : "game")
             << "exe" << execPath
             << "exists" << execInfo.exists();

    QString protonBinary = locateProtonBinary();
    QString steamRoot = locateSteamRoot();
    QString compatPath = compatibilityDataPath();
    qDebug() << "[Run] Proton binary:" << protonBinary << "Steam root:" << steamRoot << "Compat path:" << compatPath;

    QStringList args = entry.args;
    bool started = false;

    if (isTool && execInfo.exists() &&
        !protonBinary.isEmpty() && !steamRoot.isEmpty() &&
        launchToolWithOverlay(entry, protonBinary, steamRoot, compatPath)) {
        appendLootReport(QString("Launching %1 via Proton with overlay...").arg(entry.label));
        return;
    } else if (isTool && !execInfo.exists()) {
        appendLootReport(QString("Launcher not found at %1, using Steam fallback.").arg(execInfo.absoluteFilePath()));
    }

    QString program = entry.exePath;
    QStringList steamArgs = args;
    if (isTool) {
        program = "steam";
        steamArgs = {"-applaunch", QString::number(steamAppId())};
    }

    if (QProcess::startDetached(program, steamArgs)) {
        appendLootReport(QString("Launching %1 via Steam (fallback)...").arg(entry.label));
    } else {
        appendLootReport("Failed to launch via Steam. Please ensure Steam is installed and on PATH.");
    }
}


/* ─────────────────────────────────────────────────────────────
   CHANGE SKYRIM FOLDER BUTTON HANDLER
   ───────────────────────────────────────────────────────────── */
void MainWindow::onChangeFolderClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Skyrim Installation Folder");
    if (dir.isEmpty())
        return;

    QDir chosen(dir);
    if (chosen.dirName().compare("Data", Qt::CaseInsensitive) == 0)
        chosen.cdUp();

    gameInstallPath = chosen.absolutePath();
    installPath = gameInstallPath;
    saveConfigPaths();

    initializeModManager();
    refreshModsList();

    activeGame = determineGameType(gameInstallPath);
    updateModeTabIcons();
    recreateLootManager();
    rescanVirtualPlugins();
    updateIniEditorSources();
    refreshMasterlistInfoLabels();
    reloadLootMetadata();
}

void MainWindow::onSortPluginsClicked()
{
    if (!lootManager) {
        appendLootReport("LOOT manager unavailable. Choose a valid data folder first.");
        return;
    }

    appendLootReport(QString("Starting LOOT sort for %1").arg(installPath));
    bool ok = lootManager->sortPlugins();
    if (ok) {
        appendLootReport("LOOT sort completed. Refreshing plugin lists...");
        PluginManager pluginManager;
        pluginManager.scan(dataPath.toStdString());
        populatePluginList(pluginManager.getPlugins());
        appendLootReport("Plugin lists updated.");
    } else {
        appendLootReport("LOOT sort failed. Check logs above for details.");
    }
}

LootGameType MainWindow::determineGameType(const QString &dataDir)
{
    if (dataDir.isEmpty()) {
        installPath.clear();
        return LootGameType_SkyrimSE;
    }

    QDir installDir(dataDir);

    if (!installDir.exists()) {
        installPath.clear();
        return LootGameType_SkyrimSE;
    }

    if (installDir.dirName().compare("Data", Qt::CaseInsensitive) == 0)
        installDir.cdUp();

    installPath = installDir.absolutePath();
    return detectLootType(installPath);
}

void MainWindow::updateModeTabIcons()
{
    if (!modeTabs || modeTabs->count() < 2)
        return;

    QString modPath = "graphics/icons/cheese.png";
    QString lootPath = "graphics/icons/scales.png";

    switch (activeGame) {
    case LootGameType_Skyrim:
    case LootGameType_SkyrimSE:
        modPath = "graphics/icons/modCheese.png";
        lootPath = "graphics/icons/lootScales.png";
        break;
    case LootGameType_Fallout3:
    case LootGameType_FalloutNV:
    case LootGameType_Fallout4:
        modPath = "graphics/icons/modRadiation.png";
        lootPath = "graphics/icons/lootShield.png";
        break;
    case LootGameType_Oblivion:
        modPath = "graphics/icons/modOblivionGate.png";
        lootPath = "graphics/icons/lootScales.png";
        break;
    case LootGameType_Morrowind:
    case LootGameType_OpenMW:
        modPath = "graphics/icons/modMorrowind.png";
        lootPath = "graphics/icons/lootGalaxy.png";
        break;
    default:
        break;
    }

    auto applyIcon = [this](int index, const QString &path) {
        if (index < 0 || index >= static_cast<int>(modeIconStates.size()) || !modeTabs)
            return;

        auto &state = modeIconStates[index];
        if (state.bounceAnimation) {
            state.bounceAnimation->stop();
            state.bounceAnimation.reset();
        }
        if (state.spinTimer) {
            state.spinTimer->stop();
            state.spinTimer.reset();
        }
        state.animRunning = false;

        QPixmap pix = loadIconPixmap(path);
        state.basePixmap = pix;
        state.animationType = animationTypeForPath(path);

        modeTabs->setTabIcon(index, QIcon(pix));
    };

    applyIcon(0, modPath);
    applyIcon(1, lootPath);
    applyModeTabGlow();
}

QString MainWindow::currentGlowColor() const
{
    switch (activeGame) {
    case LootGameType_Skyrim:
    case LootGameType_SkyrimSE:
        return "#6EC1FF"; // icy blue
    case LootGameType_Fallout3:
    case LootGameType_FalloutNV:
    case LootGameType_Fallout4:
        return "#7CFF45"; // nuclear green
    case LootGameType_Oblivion:
        return "#FF5E5E"; // daedric red
    case LootGameType_Morrowind:
    case LootGameType_OpenMW:
        return "#FFD700"; // golden
    default:
        return "#E5E4E2"; // platinum fallback
    }
}

void MainWindow::applyModeTabGlow()
{
    if (!modeTabs)
        return;

    QString glow = currentGlowColor();
    QString style = QString(R"(
        QTabWidget::pane { border: 0; }
        QTabBar::tab {
            background: transparent;
            padding: 8px;
            margin: 4px;
            border-radius: 6px;
        }
        QTabBar::tab:selected {
            background: rgba(255,255,255,0.10);
            border-radius: 8px;
            border: 2px solid %1;
        }
    )").arg(glow);

    modeTabs->setStyleSheet(style);

    if (auto *tabBar = modeTabs->tabBar()) {
        auto *effect = new QGraphicsDropShadowEffect(tabBar);
        effect->setOffset(0);
        effect->setBlurRadius(18);
        effect->setColor(QColor(glow));
        tabBar->setGraphicsEffect(effect);
    }
}

QString MainWindow::defaultGameExecutable() const
{
    switch (activeGame) {
    case LootGameType_Skyrim:
        return QStringLiteral("Skyrim.exe");
    case LootGameType_SkyrimSE:
        return QStringLiteral("SkyrimSE.exe");
    default:
        return QStringLiteral("Skyrim.exe");
    }
}

int MainWindow::steamAppId() const
{
    switch (activeGame) {
    case LootGameType_SkyrimSE:
        return 489830;
    default:
        return 72850;
    }
}

QString MainWindow::locateSteamRoot() const
{
    QStringList candidates = {
        QDir::homePath() + "/.steam/steam",
        QDir::homePath() + "/.steam/root",
        QDir::homePath() + "/.local/share/Steam",
        QDir::homePath() + "/.var/app/com.valvesoftware.Steam/.steam/steam"
    };

    for (const QString &path : candidates) {
        QDir dir(path);
        if (dir.exists())
            return dir.absolutePath();
    }

    return QString();
}

QString MainWindow::compatibilityDataPath() const
{
    QString root = locateSteamRoot();
    if (root.isEmpty())
        return QString();

    QString compat = root + "/steamapps/compatdata/" + QString::number(steamAppId());
    QDir().mkpath(compat);
    return compat;
}

bool MainWindow::launchToolWithOverlay(const ToolEntry &entry,
                                       const QString &protonBinary,
                                       const QString &steamRoot,
                                       const QString &compatPath)
{
    if (workspacePath.isEmpty() || gameInstallPath.isEmpty() || virtualDataPath.isEmpty())
        return false;

    QString unionfsPath = QStandardPaths::findExecutable("unionfs");
    QString fusermountPath = QStandardPaths::findExecutable("fusermount");
    if (unionfsPath.isEmpty() || fusermountPath.isEmpty())
        return false;

    QString helperScript = workspacePath + "/overlay_launcher.sh";
    QFile helper(helperScript);
    if (helper.open(QIODevice::WriteOnly)) {
        helper.write("#!/bin/bash\n"
                     "set -euo pipefail\n"
                     "UNIONFS=\"$1\"\n"
                     "FUSERMOUNT=\"$2\"\n"
                     "UPPER=\"$3\"\n"
                     "LOWER=\"$4\"\n"
                     "PROTON=\"$5\"\n"
                     "STEAMROOT=\"$6\"\n"
                     "COMPAT=\"$7\"\n"
                     "EXE=\"$8\"\n"
                     "TOOLROOT=\"$9\"\n"
                     "TARGET=\"${10}\"\n"
                     "shift 10\n"
                     "mkdir -p \"$TARGET\"\n"
                     "trap 'STATUS=$?; \"$FUSERMOUNT\" -u \"$TARGET\" || true; rmdir \"$TARGET\" || true; exit $STATUS' EXIT\n"
                     "BRANCHES=\"$UPPER\"=RW\n"
                     "if [ -d \"$TOOLROOT\" ]; then\n"
                     "  BRANCHES=\"$BRANCHES:$TOOLROOT\"=RO\n"
                     "fi\n"
                     "BRANCHES=\"$BRANCHES:$LOWER\"=RO\n"
                     "\"$UNIONFS\" -o cow \"$BRANCHES\" \"$TARGET\"\n"
                     "cd \"$TARGET\"\n"
                     "LAUNCHER=$(basename \"$EXE\")\n"
                     "STEAM_COMPAT_DATA_PATH=\"$COMPAT\" STEAM_COMPAT_CLIENT_INSTALL_PATH=\"$STEAMROOT\" \"$PROTON\" run \"./$LAUNCHER\"\n");
        helper.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
                             QFileDevice::ReadGroup | QFileDevice::ExeGroup |
                             QFileDevice::ReadOther | QFileDevice::ExeOther);
    }

    QString overlayMountPath = workspacePath + QString("/RuntimeOverlay_%1").arg(QDateTime::currentMSecsSinceEpoch());
    QString toolRoot = QFileInfo(entry.exePath).absolutePath();

    QString cmd = QString("\"%1\" \"%2\" \"%3\" \"%4\" \"%5\" \"%6\" \"%7\" \"%8\" \"%9\" \"%10\" \"%11\"")
            .arg(helperScript)
            .arg(unionfsPath)
            .arg(fusermountPath)
            .arg(virtualDataPath)
            .arg(gameInstallPath)
            .arg(protonBinary)
            .arg(steamRoot)
            .arg(compatPath)
            .arg(entry.exePath)
            .arg(toolRoot)
            .arg(overlayMountPath);
    bool started = QProcess::startDetached("bash", {"-lc", cmd});
    return started;
}

QString MainWindow::locateProtonBinary() const
{
    QSettings settings("Kartavian", "NordicMod");
    QString stored = settings.value("protonPath").toString();
    if (!stored.isEmpty() && QFileInfo::exists(stored))
        return stored;

    QString root = locateSteamRoot();
    if (root.isEmpty())
        return QString();

    QString common = root + "/steamapps/common";
    QStringList candidates = {
        "Proton 7.0-6",
        "Proton 7.0",
        "Proton - Experimental",
        "Proton 8.0"
    };

    for (const QString &name : candidates) {
        QString path = common + "/" + name + "/proton";
        if (QFileInfo::exists(path)) {
            settings.setValue("protonPath", path);
            return path;
        }
    }

    QDir dir(common);
    for (const QString &name : dir.entryList(QStringList() << "Proton*", QDir::Dirs)) {
        QString path = common + "/" + name + "/proton";
        if (QFileInfo::exists(path)) {
            settings.setValue("protonPath", path);
            return path;
        }
    }

    return QString();
}

MainWindow::IconAnimationType MainWindow::animationTypeForPath(const QString &path) const
{
    static const QSet<QString> spinNames = {
        QStringLiteral("modBlackHole"),
        QStringLiteral("modGalaxy"),
        QStringLiteral("modRadiation"),
        QStringLiteral("lootGalaxy")
    };

    QFileInfo info(path);
    QString base = info.completeBaseName();
    if (spinNames.contains(base))
        return IconAnimationType::Spin;

    return IconAnimationType::Bounce;
}

QPixmap MainWindow::loadIconPixmap(const QString &path) const
{
    QPixmap pix = loadRotatedPixmap(path);
    if (pix.isNull()) {
        qWarning() << "[ICONS] Failed to load icon:" << path;
    }
    return pix;
}

void MainWindow::triggerModeTabAnimation(int index)
{
    if (!modeTabs || index < 0 || index >= static_cast<int>(modeIconStates.size()))
        return;

    auto &state = modeIconStates[index];
    if (state.basePixmap.isNull())
        return;

    if (state.animRunning)
        return;

    state.animRunning = true;

    if (state.animationType == IconAnimationType::Spin)
        startSpinAnimation(index);
    else
        startBounceAnimation(index);
}

void MainWindow::startBounceAnimation(int index)
{
    if (!modeTabs || index < 0 || index >= static_cast<int>(modeIconStates.size()))
        return;

    auto &state = modeIconStates[index];
    if (state.basePixmap.isNull()) {
        state.animRunning = false;
        return;
    }

    auto animation = std::make_unique<QVariantAnimation>();
    animation->setDuration(250);
    animation->setStartValue(1.0);
    animation->setEndValue(1.12);
    animation->setEasingCurve(QEasingCurve::OutBounce);

    QVariantAnimation *rawAnim = animation.get();
    QObject::connect(rawAnim, &QVariantAnimation::valueChanged, this, [this, index](const QVariant &value) {
        double scale = value.toDouble();
        auto &state = modeIconStates[index];
        if (state.basePixmap.isNull())
            return;
        QSize baseSize = state.basePixmap.size();
        QSize scaledSize(static_cast<int>(baseSize.width() * scale),
                         static_cast<int>(baseSize.height() * scale));
        QPixmap scaled = state.basePixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        modeTabs->tabBar()->setTabIcon(index, QIcon(scaled));
    });

    QObject::connect(rawAnim, &QVariantAnimation::finished, this, [this, index]() {
        if (!modeTabs)
            return;
        modeTabs->tabBar()->setTabIcon(index, QIcon(modeIconStates[index].basePixmap));
        modeIconStates[index].animRunning = false;
        modeIconStates[index].bounceAnimation.reset();
    });

    state.bounceAnimation = std::move(animation);
    rawAnim->start();
}

void MainWindow::startSpinAnimation(int index)
{
    if (!modeTabs || index < 0 || index >= static_cast<int>(modeIconStates.size()))
        return;

    auto &state = modeIconStates[index];
    if (state.basePixmap.isNull()) {
        state.animRunning = false;
        return;
    }

    auto timer = std::make_unique<QTimer>();
    timer->setInterval(16);
    const int durationMs = 400;
    const int steps = durationMs / timer->interval();
    auto progress = std::make_shared<int>(0);

    QTimer *rawTimer = timer.get();
    QObject::connect(rawTimer, &QTimer::timeout, this, [this, index, progress, steps]() {
        if (!modeTabs)
            return;

        auto &state = modeIconStates[index];
        if (state.basePixmap.isNull() || !state.spinTimer) {
            state.animRunning = false;
            return;
        }

        double theta = (static_cast<double>(*progress) / steps) * 360.0;
        QTransform transform;
        transform.rotate(theta);
        QPixmap rotated = state.basePixmap.transformed(transform, Qt::SmoothTransformation);
        modeTabs->tabBar()->setTabIcon(index, QIcon(rotated));

        (*progress)++;
        if (*progress > steps) {
            state.spinTimer->stop();
            state.spinTimer.reset();
            modeTabs->tabBar()->setTabIcon(index, QIcon(state.basePixmap));
            state.animRunning = false;
        }
    });

    state.spinTimer = std::move(timer);
    rawTimer->start();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (modeTabs && obj == modeTabs->tabBar()) {
        if (event->type() == QEvent::HoverMove || event->type() == QEvent::HoverEnter) {
            auto *hoverEvent = static_cast<QHoverEvent*>(event);
            QPoint pos = hoverEvent->position().toPoint();
            int idx = modeTabs->tabBar()->tabAt(pos);
            if (idx >= 0)
                triggerModeTabAnimation(idx);
        }
    }

    return QMainWindow::eventFilter(obj, event);
}
