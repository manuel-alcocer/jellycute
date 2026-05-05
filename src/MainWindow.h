#pragma once

#include <QMainWindow>
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
    void changeUser();
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

private:
    void buildToolBar();
    void buildPlayerControlBar();
    void applyOscMode(bool useNative);
    void rebuildTrackMenus();
    void buildTrackPanels();
    static QString hms(qint64 seconds);

    JellyfinClient* m_client;
    QStackedWidget* m_stack;
    BrowserWidget* m_browser;
    MpvWidget* m_player;
    QWidget* m_playerPage = nullptr;
    QWidget* m_playerBar = nullptr;
    QWidget* m_controlBar = nullptr;
    QToolButton* m_playPauseBtn = nullptr;
    QSlider* m_positionSlider = nullptr;
    TimeDisplay* m_posLabel = nullptr;
    TimeDisplay* m_durLabel = nullptr;
    QToolButton* m_audioBtn = nullptr;
    QToolButton* m_subBtn = nullptr;
    QToolButton* m_aspectBtn = nullptr;
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

    JellyfinItem m_currentItem;
    bool m_playing = false;
    qint64 m_lastReportedPositionSeconds = -1;
};
