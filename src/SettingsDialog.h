#pragma once

#include <QDialog>

class BrowserWidget;
class MpvWidget;
class QListWidget;
class QStackedWidget;
class QButtonGroup;

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

    BrowserWidget* m_browser;
    MpvWidget* m_player;
    QListWidget* m_categories = nullptr;
    QStackedWidget* m_pages = nullptr;

    // Per-page widgets read on save.
    QListWidget* m_libsList = nullptr;
    QButtonGroup* m_hwGroup = nullptr;
};
