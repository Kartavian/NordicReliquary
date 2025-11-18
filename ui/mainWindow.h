#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtWidgets/QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QTabWidget>
#include <QPushButton>
#include <QTreeView>
#include <QFileSystemModel>
#include <QPlainTextEdit>
#include <QTextBrowser>
#include <QLabel>
#include <QTableWidget>
#include <QVariantAnimation>
#include <QTimer>
#include <QPixmap>
#include "iniEditorWidget.h"
#include <QComboBox>
#include <QListWidgetItem>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <array>
#include <memory>
#include <vector>
#include "pluginManager.h"
#include "../loot-shim/include/loot_shim.h"
#include "modManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class LootManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT  // Required for Qt’s meta-object compiler (signals/slots)

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onChangeFolderClicked();  // Handles "Change Folder" button click
    void onSortPluginsClicked();   // Trigger LOOT sort routine (placeholder)
    void onInstallArchivesRequested(const QStringList &archives);
    void onModItemChanged(QListWidgetItem *item);
    void onRemoveModClicked();
    void onRunToolChanged(int index);
    void onRunButtonClicked();
    void onDownloadMasterlistClicked();
    void onUpdateMasterlistClicked();
    void onEditUserRulesClicked();
    void onResetUserlistClicked();

private:
    enum class IconAnimationType { Bounce, Spin };

    struct TabIconState {
        QPixmap basePixmap;
        IconAnimationType animationType = IconAnimationType::Bounce;
        std::unique_ptr<QVariantAnimation> bounceAnimation;
        std::unique_ptr<QTimer> spinTimer;
        bool animRunning = false;
    };

    enum class ToolEntryType { Game, Tool };

    struct ToolEntry {
        QString id;
        QString label;
        QString exePath;
        QStringList args;
        ToolEntryType type = ToolEntryType::Game;
    };

    Ui::MainWindow *ui;

    // ----- NEW WIDGETS FOR MODE SWITCHING -----
    QTabWidget* modeTabs;   // New: vertical mode selector with icons
    QStackedWidget* leftStack;
    QStackedWidget* rightStack;

    // LOOT mode UI pieces
    QListWidget* lootPluginList;
    QTabWidget* lootTaskTabs = nullptr;
    QPushButton* sortPluginsButton = nullptr;
    QPushButton* removeModButton = nullptr;
    QLabel* lootPluginName = nullptr;
    QLabel* lootPluginType = nullptr;
    QListWidget* lootMasterList = nullptr;
    QPlainTextEdit* lootReportOutput = nullptr;
    QTextBrowser* lootPluginDetailsView = nullptr;
    QLabel* masterlistVersionLabel = nullptr;
    QLabel* masterlistUpdatedLabel = nullptr;
    QPushButton* downloadMasterlistButton = nullptr;
    QPushButton* updateMasterlistButton = nullptr;
    QPushButton* editUserRulesButton = nullptr;
    QPushButton* resetUserlistButton = nullptr;
    QTableWidget* lootWarningsTable = nullptr;
    QComboBox* runToolCombo = nullptr;
    IniEditorWidget* iniEditor = nullptr;

    QString dataPath;
    QString installPath;
    LootGameType activeGame;
    QFileSystemModel* dataModel = nullptr;
    QTreeView* modDataView = nullptr;
    QTreeView* lootDataView = nullptr;
    std::vector<PluginInfo> cachedPlugins;
    std::unique_ptr<LootManager> lootManager;
    std::array<TabIconState, 2> modeIconStates;
    std::unique_ptr<ModManager> modManager;
    std::vector<ToolEntry> toolEntries;
    QString selectedToolId;
    QHash<QString, QJsonObject> lootPluginDetailsCache;
    QJsonArray lootGeneralMessages;

    QString workspacePath;
    QString downloadsPath;
    QString modsPath;
    QString virtualDataPath;
    QString gameInstallPath;
    QString lootDataRoot;

    void loadDataPath();
    void saveDataPath(const QString &path);
    void saveConfigPaths() const;
    bool loadWorkspaceConfig();
    void initializeModManager();
    void refreshModsList();
    void rescanVirtualPlugins();
    void updateToolLaunchers();
    void updateIniEditorSources();
    QString defaultGameExecutable() const;
    int steamAppId() const;
    QString locateSteamRoot() const;
    QString locateProtonBinary() const;
    QString compatibilityDataPath() const;
    bool launchToolWithOverlay(const ToolEntry &entry,
                               const QString &protonBinary,
                               const QString &steamRoot,
                               const QString &compatPath);
    void setupStyle();
    void populatePluginList(const std::vector<PluginInfo> &plugins);
    void setupDataViews();
    void refreshDataRoots();
    LootGameType determineGameType(const QString &dataDir);
    void updateModeTabIcons();
    void recreateLootManager();
    void displayLootMetadata(int index);
    void appendLootReport(const QString &line);
    void reloadLootMetadata();
    void refreshMasterlistInfoLabels();
    QString masterlistRepoUrl() const;
    QString masterlistDirectory() const;
    QString masterlistFilePath() const;
    QString masterlistPreludePath() const;
    QString userlistPathForActiveGame() const;
    QString lootGameSlug() const;
    void ensureLootDataFolders() const;
    bool runGitCommand(const QStringList &args, const QString &workingDir, const QString &description);
    QString runGitForOutput(const QStringList &args, const QString &workingDir) const;
    QString normalizedPluginKey(const QString &name) const;
    void rebuildWarningsTable();
    QString buildPluginMetadataHtml(const PluginInfo &plugin) const;
    IconAnimationType animationTypeForPath(const QString &path) const;
    QString currentGlowColor() const;
    void applyModeTabGlow();
    void triggerModeTabAnimation(int index);
    void startBounceAnimation(int index);
    void startSpinAnimation(int index);
    QPixmap loadIconPixmap(const QString &path) const;
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // MAINWINDOW_H
