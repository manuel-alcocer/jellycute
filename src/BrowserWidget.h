#pragma once

#include <QWidget>
#include <QStack>
#include <QHash>
#include <QIcon>
#include "JellyfinClient.h"

class QFrame;
class QListWidget;
class QListWidgetItem;
class QLabel;
class DetailPage;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QHBoxLayout;
class QVBoxLayout;
class QNetworkAccessManager;
class MediaRow;

class BrowserWidget : public QWidget {
    Q_OBJECT
public:
    explicit BrowserWidget(JellyfinClient* client, QWidget* parent = nullptr);

    void start();
    void refreshCurrent();
    QStringList currentBreadcrumb() const;     // for the detail page header
    void popToBreadcrumb(int idx);             // 0 = root grid; -1 = home

    bool isSidebarVisible() const;
    void setSidebarVisible(bool visible);
    void goBack();                             // pop one breadcrumb level

    QList<JellyfinItem> allLibraries() const { return m_libraries; }
    bool isLibraryHidden(const QString& id) const;
    void setLibraryHidden(const QString& id, bool hidden);

signals:
    // startTicks: 0 → from beginning; >0 → resume.
    void playRequested(const JellyfinItem& item, qint64 startTicks);
    void librariesChanged(const QList<JellyfinItem>& libraries);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onSidebarChanged(QListWidgetItem* current);
    void onItemActivated(QListWidgetItem* w);
    void onBackClicked();

    void onViewsLoaded(const QList<JellyfinItem>& items);
    void onItemsLoaded(const QString& parentId,
                       const QList<JellyfinItem>& items, int totalCount);
    void onResumeLoaded(const QList<JellyfinItem>& items);
    void onLatestLoaded(const QString& parentId, const QList<JellyfinItem>& items);

    void onRowItemActivated(const JellyfinItem& it);
    void onRowSeeAll(const QString& sectionId);

private:
    enum class Page { Home, Grid };

    struct Frame {
        QString title;
        QString parentId;
        QString includeTypes;
        bool recursive = false;
        QString sortBy = QStringLiteral("SortName");
        QList<JellyfinItem> items;
        // Library-only state: pagination + alphabet filter.
        bool paginated = false;
        QChar letter = QChar('\0');     // '\0' = all, '?' = non-alphanumeric, else single char
        int page = 0;
        int totalCount = 0;
        // Cache for client-side "?" filtering.
        QList<JellyfinItem> allFiltered;
    };

    struct NavEntry {
        enum Kind { Home, Grid, Detail };
        Kind kind = Home;
        QStack<Frame> gridStack;            // populated when kind == Grid
        JellyfinItem detailItem;            // populated when kind == Detail
    };

    void buildSidebar();
    void buildHomePage();
    void buildGridPage();

    void showHome();
    void showGridForLibrary(const JellyfinItem& library);
    void showDetail(const JellyfinItem& item);

    void loadHomeContent();
    void requestForFrame();
    void populateGrid(const QList<JellyfinItem>& items);
    void rebuildPaginationBar();
    void slideInLetterPanel();
    void slideOutLetterPanel();
    void onLetterPicked(QChar letter);
    void onPagePicked(int page);
    void applyClientSideFilter(const QList<JellyfinItem>& src);
    void rebuildBreadcrumb();
    void rebuildLibrarySidebar();
    void rebuildHomeLatestRows();

    // History (LIFO) of past views. pushHistory() snapshots the current
    // state right before a forward navigation; goBack pops one and restores.
    NavEntry snapshotCurrent() const;
    void pushHistory();
    void restoreEntry(const NavEntry& e);
    void fetchPoster(QListWidgetItem* w, const JellyfinItem& it,
                     const QSize& iconSize, bool useBackdrop = false);
    void updateHeader();
    void recenterGrid();

    JellyfinClient* m_client;

    // Sidebar
    QListWidget* m_sidebar;
    QListWidgetItem* m_homeItem = nullptr;

    // Pages
    QStackedWidget* m_pages;

    // Home
    QWidget* m_homePage;
    QVBoxLayout* m_homeLayout;
    MediaRow* m_resumeRow;
    QHash<QString, MediaRow*> m_latestRows;     // libraryId → row

    // Grid
    QWidget* m_gridPage;
    QListWidget* m_grid;
    QToolButton* m_letterBtn;
    QWidget* m_paginationBar;
    QHBoxLayout* m_paginationLayout;
    QWidget* m_breadcrumb;
    QHBoxLayout* m_breadcrumbLayout;
    QFrame* m_gridCard;
    QFrame* m_letterPanel;
    DetailPage* m_detail = nullptr;

    // Grid navigation
    QStack<Frame> m_stack;
    QStack<NavEntry> m_history;       // LIFO of past views (excluding current)
    JellyfinItem m_currentDetailItem; // valid while detail page is shown
    quint64 m_frameGen = 0;
    QIcon m_placeholder;
    int m_currentSideMargin = 0;

    QList<JellyfinItem> m_libraries;             // populated from views
    QNetworkAccessManager* m_imgNam;
};
