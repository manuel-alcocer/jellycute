#pragma once

#include <QDialog>
#include <QHash>

class BrowserWidget;
class MpvWidget;
class JellyfinClient;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QButtonGroup;
class ServerProbe;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(BrowserWidget* browser, MpvWidget* player,
                   QWidget* parent = nullptr);

private slots:
    void onSave();

private:
    QWidget* buildLibrariesPage();
    QWidget* buildPlaybackPage();
    QWidget* buildAppearancePage();
    QWidget* buildConnectionsPage();

    void refreshServers();
    void refreshAccounts();
    void onAddServer();
    void onEditServer();
    void onRemoveServer();
    void onAddAccount();
    void onEditAccount();
    void onRemoveAccount();
    void probeServer(const QString& serverId);

    BrowserWidget* m_browser;
    MpvWidget* m_player;
    QListWidget* m_categories = nullptr;
    QStackedWidget* m_pages = nullptr;

    // Per-page widgets read on save.
    QListWidget* m_libsList = nullptr;
    class QComboBox* m_hwCombo = nullptr;
    QButtonGroup* m_themeGroup = nullptr;

    QListWidget* m_serversList = nullptr;
    QListWidget* m_accountsList = nullptr;
    ServerProbe* m_probe = nullptr;
    QHash<QString, int> m_serverStatus;   // serverId → 0 unknown / 1 online / 2 offline
};
