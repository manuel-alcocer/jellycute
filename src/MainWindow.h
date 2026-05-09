#pragma once

#include <QList>
#include <QMainWindow>
#include <functional>
#include "JellyfinClient.h"

class QStackedWidget;
class QTimer;
class BrowserWidget;
class MpvWidget;
class QActionGroup;
class QSlider;
class QLabel;
class QToolButton;
class QFrame;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QVBoxLayout;
class TimeDisplay;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(JellyfinClient* client, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onPlayWithStart(const JellyfinItem& item, qint64 startTicks);
    void onPlaybackResolved(const QString& itemId, const JellyfinPlayback& info);
    void onPlaybackResolveFailed(const QString& itemId, const QString& message);
    void rebuildAccountsMenu();
    void switchToAccount(const QString& accountId);
    void openSettings();
    void onMpvIdleChanged(bool idle);
    void onMpvEndReached();
    void onMpvPaused(bool paused);
    void onMpvPosition(qint64 seconds);
    void reportProgressTick();
    void switchToBrowser();
    void toggleFullscreen();
    void exitFullscreen();

public:
    void slideUpPanel(QFrame* panel);
    void slideDownPanel(QFrame* panel);
    void refreshThemedIcons();

private:
    void buildToolBar();
    void buildPlayerControlBar();
    void applyOscMode(bool useNative);
    void rebuildTrackMenus();
    void buildTrackPanels();
    void setAutoHideEnabled(bool on);
    void showControlBar();
    void hideControlBar();
    void positionControlBarOverlay();
    bool isInControlBarTree(QObject* w) const;
    void applyControlBarIcons();
    static QString hms(qint64 seconds);

    JellyfinClient* m_client;
    class QToolButton* m_userBtn = nullptr;
    class QMenu* m_accountsMenu = nullptr;
    QStackedWidget* m_stack;
    BrowserWidget* m_browser;
    MpvWidget* m_player;
    QWidget* m_playerPage = nullptr;
    QWidget* m_controlBar = nullptr;
    QToolButton* m_playPauseBtn = nullptr;
    QSlider* m_positionSlider = nullptr;
    TimeDisplay* m_posLabel = nullptr;
    TimeDisplay* m_durLabel = nullptr;
    QToolButton* m_audioBtn = nullptr;
    QToolButton* m_subBtn = nullptr;
    QToolButton* m_aspectBtn = nullptr;
    QToolButton* m_autoHideBtn = nullptr;
    QToolButton* m_fullscreenBtn = nullptr;
    QToolButton* m_seekBackBtn = nullptr;
    QToolButton* m_seekFwdBtn = nullptr;
    QToolButton* m_volumeIconBtn = nullptr;
    QSlider* m_volumeSlider = nullptr;
    class QFrame* m_audioPanel = nullptr;
    class QFrame* m_subPanel = nullptr;
    class QVBoxLayout* m_audioPanelList = nullptr;
    class QVBoxLayout* m_subPanelList = nullptr;
    QAction* m_oscToggleAct = nullptr;
    bool m_userSeeking = false;
    QTimer* m_progressTimer;
    QAction* m_fullscreenAct = nullptr;
    class QToolBar* m_toolBar = nullptr;

    bool m_autoHideEnabled = false;
    QTimer* m_autoHideTimer = nullptr;
    QGraphicsOpacityEffect* m_controlBarOpacity = nullptr;
    QPropertyAnimation* m_controlBarFade = nullptr;

    // Re-applies icons after a theme switch. Each entry is a closure that
    // captures a (toolbar QAction* | toolbar button) and resets its icon
    // using the theme's current foreground colour.
    QList<std::function<void()>> m_iconRefreshers;

    JellyfinItem m_currentItem;
    JellyfinPlayback m_currentPlayback;
    qint64 m_pendingStartTicks = 0;
    bool m_playing = false;
    qint64 m_lastReportedPositionSeconds = -1;
};
