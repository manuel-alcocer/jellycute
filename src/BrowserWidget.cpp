#include "BrowserWidget.h"
#include "DetailPage.h"
#include "MediaRow.h"
#include "PlayEmblemDelegate.h"

#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

namespace {
// QAbstractScrollArea::setViewportMargins is protected, so expose it.
class CenterableList : public QListWidget {
public:
    using QListWidget::QListWidget;
    using QListWidget::setViewportMargins;
};

constexpr int SidebarWidth = 240;
constexpr int GridIconW = 180;
constexpr int GridIconH = 270;
constexpr int GridCellW = 210;
constexpr int GridCellH = 360;
constexpr int PageSize = 100;
constexpr int MaxPaginationButtons = 9;
constexpr int ClientFilterBatch = 5000;     // upper bound for "?" client-side filter

QIcon makePlaceholder(const QSize& s) {
    QPixmap pm(s);
    pm.fill(QColor(40, 40, 40));
    return QIcon(pm);
}

QIcon iconFromPixmap(const QPixmap& pm, const QSize& target) {
    QPixmap scaled = pm.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap canvas(target);
    canvas.fill(Qt::transparent);
    QPainter p(&canvas);
    const int x = (target.width() - scaled.width()) / 2;
    const int y = (target.height() - scaled.height()) / 2;
    p.drawPixmap(x, y, scaled);
    p.end();
    return QIcon(canvas);
}

bool clickHitsPlayEmblem(const QPoint& posInViewport, const QRect& cellRect,
                         const QSize& iconSize) {
    // Icon is centered horizontally at the top of the cell.
    const int ix = cellRect.x() + (cellRect.width() - iconSize.width()) / 2;
    const int iy = cellRect.y() + PlayEmblemDelegate::IconTopGap;
    const QRect emblem(
        ix + iconSize.width() - PlayEmblemDelegate::Margin
            - PlayEmblemDelegate::Size,
        iy + iconSize.height() - PlayEmblemDelegate::Margin
            - PlayEmblemDelegate::Size,
        PlayEmblemDelegate::Size, PlayEmblemDelegate::Size);
    return emblem.contains(posInViewport);
}
}

BrowserWidget::BrowserWidget(JellyfinClient* client, QWidget* parent)
    : QWidget(parent), m_client(client),
      m_imgNam(new QNetworkAccessManager(this)) {

    auto* diskCache = new QNetworkDiskCache(this);
    const QString cacheDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/images");
    QDir().mkpath(cacheDir);
    diskCache->setCacheDirectory(cacheDir);
    diskCache->setMaximumCacheSize(qint64(256) * 1024 * 1024);
    m_imgNam->setCache(diskCache);
    QPixmapCache::setCacheLimit(64 * 1024);

    m_placeholder = makePlaceholder(QSize(GridIconW, GridIconH));

    buildSidebar();
    buildHomePage();
    buildGridPage();

    m_detail = new DetailPage(client, this);

    m_pages = new QStackedWidget(this);
    m_pages->setObjectName(QStringLiteral("contentPanel"));
    m_pages->addWidget(m_homePage);
    m_pages->addWidget(m_gridPage);
    m_pages->addWidget(m_detail);

    // Detail page emits Play (with explicit startTicks) and breadcrumb clicks.
    // Forward play upward; handle breadcrumb internally.
    connect(m_detail, &DetailPage::playRequested,
            this, &BrowserWidget::playRequested);
    connect(m_detail, &DetailPage::breadcrumbClicked, this, [this](int idx) {
        const QStringList ancestors = currentBreadcrumb();
        if (ancestors.size() == 1 && ancestors.first() == tr("Inicio"))
            popToBreadcrumb(-1);
        else
            popToBreadcrumb(idx);
    });
    connect(m_detail, &DetailPage::itemSelected, this,
            [this](const JellyfinItem& it) { showDetail(it); });

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);   // breathing room around panels
    root->setSpacing(6);                    // gap between sidebar and content
    root->addWidget(m_sidebar);
    root->addWidget(m_pages, 1);

    connect(m_client, &JellyfinClient::viewsLoaded, this, &BrowserWidget::onViewsLoaded);
    connect(m_client, &JellyfinClient::itemsLoaded, this, &BrowserWidget::onItemsLoaded);
    connect(m_client, &JellyfinClient::resumeLoaded, this, &BrowserWidget::onResumeLoaded);
    connect(m_client, &JellyfinClient::latestLoaded, this, &BrowserWidget::onLatestLoaded);
}

void BrowserWidget::buildSidebar() {
    m_sidebar = new QListWidget(this);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(SidebarWidth);
    m_sidebar->setFrameShape(QFrame::NoFrame);
    m_sidebar->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sidebar->setSpacing(2);
    m_sidebar->setUniformItemSizes(true);
    m_sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Built-in icon set as a stand-in until we ship our own.
    auto add = [this](const QString& text, QStyle::StandardPixmap sp) {
        auto* w = new QListWidgetItem(text);
        w->setIcon(style()->standardIcon(sp));
        w->setSizeHint(QSize(0, 32));
        m_sidebar->addItem(w);
        return w;
    };

    m_homeItem = add(tr("Inicio"), QStyle::SP_DirHomeIcon);

    // Section header for libraries — non-selectable, Dolphin-style.
    auto* libHeader = new QListWidgetItem(tr("Bibliotecas"));
    libHeader->setFlags(Qt::NoItemFlags);
    libHeader->setForeground(QColor("#7f8c8d"));
    QFont hf = libHeader->font();
    hf.setPointSize(qMax(1, hf.pointSize() - 1));
    libHeader->setFont(hf);
    libHeader->setSizeHint(QSize(0, 28));
    m_sidebar->addItem(libHeader);

    m_sidebar->setCurrentItem(m_homeItem);

    connect(m_sidebar, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem* current, QListWidgetItem*) {
        onSidebarChanged(current);
    });
}

void BrowserWidget::buildHomePage() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* container = new QWidget;
    m_homeLayout = new QVBoxLayout(container);
    m_homeLayout->setContentsMargins(12, 12, 12, 24);
    m_homeLayout->setSpacing(12);

    m_resumeRow = new MediaRow("__resume__", tr("Continuar viendo"),
                               MediaRow::PosterShape::Backdrop, container);
    connect(m_resumeRow, &MediaRow::itemActivated,
            this, &BrowserWidget::onRowItemActivated);
    connect(m_resumeRow, &MediaRow::playClicked, this,
            [this](const JellyfinItem& it) {
                emit playRequested(it, it.resumePositionTicks);
            });
    connect(m_resumeRow, &MediaRow::seeAllClicked,
            this, &BrowserWidget::onRowSeeAll);
    m_homeLayout->addWidget(m_resumeRow);

    m_homeLayout->addStretch();

    scroll->setWidget(container);
    m_homePage = scroll;
}

void BrowserWidget::buildGridPage() {
    m_gridPage = new QWidget(this);
    auto* layout = new QVBoxLayout(m_gridPage);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    // Header: breadcrumb (left, expands) + A-Z slide-over trigger (right).
    auto* header = new QHBoxLayout;
    header->setContentsMargins(4, 0, 4, 0);
    m_breadcrumb = new QWidget(m_gridPage);
    m_breadcrumbLayout = new QHBoxLayout(m_breadcrumb);
    m_breadcrumbLayout->setContentsMargins(0, 0, 0, 0);
    m_breadcrumbLayout->setSpacing(0);
    m_breadcrumbLayout->addStretch();

    m_letterBtn = new QToolButton(m_gridPage);
    m_letterBtn->setText(tr("A-Z"));
    m_letterBtn->setToolTip(tr("Filtrar por letra inicial"));

    header->addWidget(m_breadcrumb, 1);
    header->addWidget(m_letterBtn);

    // Card around the grid (matches the Inicio sections look).
    m_gridCard = new QFrame(m_gridPage);
    m_gridCard->setObjectName(QStringLiteral("contentCard"));
    auto* cardLayout = new QVBoxLayout(m_gridCard);
    cardLayout->setContentsMargins(12, 12, 12, 12);
    cardLayout->setSpacing(0);

    m_grid = new CenterableList(m_gridCard);
    m_grid->setItemDelegate(new PlayEmblemDelegate(/*twoLineCaption=*/true, m_grid));
    m_grid->setMouseTracking(true);
    m_grid->viewport()->setAttribute(Qt::WA_Hover, true);
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setIconSize(QSize(GridIconW, GridIconH));
    m_grid->setGridSize(QSize(GridCellW, GridCellH));
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setSpacing(8);
    m_grid->setWordWrap(true);
    m_grid->setSelectionMode(QAbstractItemView::SingleSelection);
    m_grid->setFrameShape(QFrame::NoFrame);
    // Opaque viewport (matches contentCard) avoids stale-paint hairlines in
    // the empty area below the last row when relying on the parent's
    // transparent pass-through.
    m_grid->viewport()->setObjectName(QStringLiteral("gridViewport"));
    m_grid->viewport()->installEventFilter(this);
    cardLayout->addWidget(m_grid);

    m_paginationBar = new QWidget(m_gridPage);
    m_paginationLayout = new QHBoxLayout(m_paginationBar);
    m_paginationLayout->setContentsMargins(0, 0, 0, 0);
    m_paginationLayout->setSpacing(4);

    layout->addLayout(header);
    layout->addWidget(m_gridCard, 1);
    layout->addWidget(m_paginationBar);
    m_paginationBar->hide();

    // Right-side slide-over panel for the alphabet filter.
    m_letterPanel = new QFrame(m_gridPage);
    m_letterPanel->setObjectName(QStringLiteral("letterPanel"));
    m_letterPanel->setVisible(false);
    auto* panelLayout = new QVBoxLayout(m_letterPanel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    panelLayout->setSpacing(10);

    auto* panelHeader = new QHBoxLayout;
    auto* panelTitle = new QLabel(tr("Filtrar por inicial"), m_letterPanel);
    QFont pf = panelTitle->font();
    pf.setBold(true);
    pf.setPointSize(pf.pointSize() + 1);
    panelTitle->setFont(pf);
    panelHeader->addWidget(panelTitle, 1);
    panelLayout->addLayout(panelHeader);

    auto* g = new QGridLayout;
    g->setSpacing(6);
    auto addBtn = [&](const QString& text, QChar value, int row, int col, int colSpan = 1) {
        auto* b = new QToolButton(m_letterPanel);
        b->setText(text);
        b->setMinimumSize(40, 40);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QToolButton::clicked, this, [this, value]() {
            slideOutLetterPanel();
            onLetterPicked(value);
        });
        g->addWidget(b, row, col, 1, colSpan);
    };
    const QString letters = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    const QString digits = QStringLiteral("0123456789");
    const int cols = 6;
    for (int i = 0; i < letters.size(); ++i)
        addBtn(QString(letters[i]), letters[i], i / cols, i % cols);
    int row = (letters.size() + cols - 1) / cols;
    for (int i = 0; i < digits.size(); ++i)
        addBtn(QString(digits[i]), digits[i], row + i / cols, i % cols);
    row += (digits.size() + cols - 1) / cols;
    addBtn(tr("?"), QChar('?'), row, 0, cols / 2);
    addBtn(tr("Todo"), QChar('\0'), row, cols / 2, cols - cols / 2);
    panelLayout->addLayout(g);
    panelLayout->addStretch();

    connect(m_grid, &QListWidget::itemClicked, this, &BrowserWidget::onItemActivated);
    connect(m_letterBtn, &QToolButton::clicked, this, [this]() {
        if (m_letterPanel && m_letterPanel->isVisible()) slideOutLetterPanel();
        else slideInLetterPanel();
    });

    m_gridPage->installEventFilter(this);
}

void BrowserWidget::start() {
    m_stack.clear();
    m_libraries.clear();
    m_latestRows.clear();
    m_history.clear();
    m_currentDetailItem = {};
    m_resumeRow->clear();
    m_client->fetchUserViews();
    m_client->fetchResume();
    m_pages->setCurrentWidget(m_homePage);
}

BrowserWidget::NavEntry BrowserWidget::snapshotCurrent() const {
    NavEntry e;
    if (m_pages->currentWidget() == m_detail) {
        e.kind = NavEntry::Detail;
        e.detailItem = m_currentDetailItem;
    } else if (m_pages->currentWidget() == m_gridPage) {
        e.kind = NavEntry::Grid;
        e.gridStack = m_stack;
    } else {
        e.kind = NavEntry::Home;
    }
    return e;
}

void BrowserWidget::pushHistory() {
    m_history.push(snapshotCurrent());
}

void BrowserWidget::restoreEntry(const NavEntry& e) {
    switch (e.kind) {
    case NavEntry::Home:
        m_stack.clear();
        m_currentDetailItem = {};
        m_pages->setCurrentWidget(m_homePage);
        break;
    case NavEntry::Grid:
        m_stack = e.gridStack;
        m_currentDetailItem = {};
        m_pages->setCurrentWidget(m_gridPage);
        updateHeader();
        m_grid->clear();
        ++m_frameGen;
        if (!m_stack.isEmpty() && !m_stack.top().items.isEmpty()) {
            populateGrid(m_stack.top().items);
            if (m_stack.top().paginated) rebuildPaginationBar();
        } else {
            requestForFrame();
        }
        break;
    case NavEntry::Detail:
        // Re-show without pushing onto history.
        m_currentDetailItem = e.detailItem;
        m_detail->showItem(e.detailItem, currentBreadcrumb());
        m_pages->setCurrentWidget(m_detail);
        break;
    }
}

bool BrowserWidget::isSidebarVisible() const {
    return m_sidebar && m_sidebar->isVisible();
}

void BrowserWidget::setSidebarVisible(bool visible) {
    if (m_sidebar) m_sidebar->setVisible(visible);
}

void BrowserWidget::goBack() {
    if (m_history.isEmpty()) return;
    const NavEntry e = m_history.pop();
    restoreEntry(e);
}

void BrowserWidget::refreshCurrent() {
    if (m_pages->currentWidget() == m_homePage) {
        loadHomeContent();
    } else {
        ++m_frameGen;
        m_grid->clear();
        requestForFrame();
    }
}

void BrowserWidget::loadHomeContent() {
    m_resumeRow->clear();
    for (auto* row : m_latestRows) row->clear();
    m_client->fetchResume();
    for (const auto& lib : m_libraries) m_client->fetchLatest(lib.id);
}

void BrowserWidget::onSidebarChanged(QListWidgetItem* current) {
    if (!current) return;
    if (current == m_homeItem) {
        // Going home is a forward navigation when you weren't already there.
        if (m_pages->currentWidget() != m_homePage) pushHistory();
        showHome();
        return;
    }
    // Library item?
    const QString libId = current->data(Qt::UserRole).toString();
    if (libId.isEmpty()) return;
    for (const auto& lib : m_libraries) {
        if (lib.id == libId) {
            pushHistory();
            showGridForLibrary(lib);
            return;
        }
    }
}

void BrowserWidget::showHome() {
    m_pages->setCurrentWidget(m_homePage);
}

void BrowserWidget::showDetail(const JellyfinItem& item) {
    pushHistory();
    m_currentDetailItem = item;
    m_detail->showItem(item, currentBreadcrumb());
    m_pages->setCurrentWidget(m_detail);
}

void BrowserWidget::showGridForLibrary(const JellyfinItem& library) {
    m_stack.clear();
    Frame f;
    f.title = library.name;
    f.parentId = library.id;
    f.sortBy = "SortName";
    f.paginated = true;
    m_stack.push(std::move(f));
    m_pages->setCurrentWidget(m_gridPage);
    updateHeader();
    m_grid->clear();
    ++m_frameGen;
    requestForFrame();
}

void BrowserWidget::updateHeader() {
    if (m_stack.isEmpty()) return;
    const Frame& f = m_stack.top();
    rebuildBreadcrumb();

    const bool showFilter = f.paginated;
    m_letterBtn->setVisible(showFilter);
    if (showFilter) {
        const QChar c = f.letter;
        if (c == QChar('\0'))      m_letterBtn->setText(tr("A-Z"));
        else if (c == QChar('?'))  m_letterBtn->setText(tr("?"));
        else                        m_letterBtn->setText(QString(c));
    }
    m_paginationBar->setVisible(showFilter);
}

void BrowserWidget::rebuildBreadcrumb() {
    QLayoutItem* li;
    while ((li = m_breadcrumbLayout->takeAt(0)) != nullptr) {
        if (auto* w = li->widget()) w->deleteLater();
        delete li;
    }
    for (int i = 0; i < m_stack.size(); ++i) {
        const bool isLast = (i == m_stack.size() - 1);
        const QString title = m_stack[i].title;
        if (isLast) {
            auto* lbl = new QLabel(title, m_breadcrumb);
            QFont bf = lbl->font();
            bf.setBold(true);
            bf.setPointSize(bf.pointSize() + 4);
            lbl->setFont(bf);
            m_breadcrumbLayout->addWidget(lbl);
        } else {
            auto* btn = new QToolButton(m_breadcrumb);
            btn->setText(title);
            btn->setAutoRaise(true);
            btn->setCursor(Qt::PointingHandCursor);
            QFont bf = btn->font();
            bf.setPointSize(bf.pointSize() + 4);
            btn->setFont(bf);
            const int target = i;
            connect(btn, &QToolButton::clicked, this, [this, target]() {
                popToBreadcrumb(target);
            });
            m_breadcrumbLayout->addWidget(btn);
            auto* sep = new QLabel(QStringLiteral("›"), m_breadcrumb);
            sep->setStyleSheet("color:#7f8c8d; padding: 0 6px;");
            QFont sf = sep->font();
            sf.setPointSize(sf.pointSize() + 4);
            sep->setFont(sf);
            m_breadcrumbLayout->addWidget(sep);
        }
    }
    m_breadcrumbLayout->addStretch();
}

QStringList BrowserWidget::currentBreadcrumb() const {
    if (m_stack.isEmpty()) return QStringList{tr("Inicio")};
    QStringList out;
    for (const auto& f : m_stack) out << f.title;
    return out;
}

void BrowserWidget::popToBreadcrumb(int idx) {
    if (m_stack.isEmpty() || idx < 0) {
        showHome();
        return;
    }
    while (m_stack.size() > idx + 1) m_stack.pop();
    m_pages->setCurrentWidget(m_gridPage);
    updateHeader();
    m_grid->clear();
    ++m_frameGen;
    if (!m_stack.top().items.isEmpty()) {
        populateGrid(m_stack.top().items);
        if (m_stack.top().paginated) rebuildPaginationBar();
    } else {
        requestForFrame();
    }
}

void BrowserWidget::requestForFrame() {
    if (m_stack.isEmpty()) return;
    Frame& f = m_stack.top();
    if (!f.paginated) {
        m_client->fetchItems(f.parentId, f.includeTypes, f.recursive, f.sortBy);
        return;
    }
    if (f.letter == QChar('?')) {
        // "?" means non-alphanumeric: server can't filter that, so we fetch
        // a large batch (sorted) and filter client-side.
        m_client->fetchItems(f.parentId, f.includeTypes, f.recursive, f.sortBy,
                             /*nameStartsWith=*/QString(),
                             /*startIndex=*/0,
                             /*limit=*/ClientFilterBatch);
        return;
    }
    QString starts;
    if (f.letter != QChar('\0')) starts = QString(f.letter);
    m_client->fetchItems(f.parentId, f.includeTypes, f.recursive, f.sortBy,
                         starts, f.page * PageSize, PageSize);
}

void BrowserWidget::slideInLetterPanel() {
    if (!m_letterPanel) return;
    const int panelW = qMin(320, m_gridPage->width() - 40);
    const int parentH = m_gridPage->height();
    const int parentW = m_gridPage->width();
    const bool wasVisible = m_letterPanel->isVisible();
    m_letterPanel->setGeometry(parentW, 0, panelW, parentH);
    m_letterPanel->show();
    m_letterPanel->raise();
    auto* anim = new QPropertyAnimation(m_letterPanel, "geometry", this);
    anim->setDuration(220);
    anim->setStartValue(QRect(parentW, 0, panelW, parentH));
    anim->setEndValue(QRect(parentW - panelW, 0, panelW, parentH));
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
    // Watch app-wide mouse presses while open so a click outside closes it.
    if (!wasVisible) qApp->installEventFilter(this);
}

void BrowserWidget::slideOutLetterPanel() {
    if (!m_letterPanel || !m_letterPanel->isVisible()) return;
    qApp->removeEventFilter(this);
    const QRect start = m_letterPanel->geometry();
    auto* anim = new QPropertyAnimation(m_letterPanel, "geometry", this);
    anim->setDuration(180);
    anim->setStartValue(start);
    anim->setEndValue(QRect(m_gridPage->width(), 0, start.width(), start.height()));
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, m_letterPanel, &QWidget::hide);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void BrowserWidget::onLetterPicked(QChar letter) {
    if (m_stack.isEmpty() || !m_stack.top().paginated) return;
    Frame& f = m_stack.top();
    if (f.letter == letter) return;
    f.letter = letter;
    f.page = 0;
    f.totalCount = 0;
    f.allFiltered.clear();
    updateHeader();
    m_grid->clear();
    ++m_frameGen;
    requestForFrame();
}

void BrowserWidget::onPagePicked(int page) {
    if (m_stack.isEmpty() || !m_stack.top().paginated) return;
    Frame& f = m_stack.top();
    if (page == f.page) return;
    f.page = page;
    m_grid->clear();
    ++m_frameGen;
    if (f.letter == QChar('?') && !f.allFiltered.isEmpty()) {
        // Already have the full filtered list; just slice locally.
        const int start = f.page * PageSize;
        const int end = qMin<int>(start + PageSize, f.allFiltered.size());
        QList<JellyfinItem> slice;
        slice.reserve(end - start);
        for (int i = start; i < end; ++i) slice.push_back(f.allFiltered[i]);
        populateGrid(slice);
        rebuildPaginationBar();
        return;
    }
    requestForFrame();
}

void BrowserWidget::applyClientSideFilter(const QList<JellyfinItem>& src) {
    if (m_stack.isEmpty()) return;
    Frame& f = m_stack.top();
    f.allFiltered.clear();
    for (const auto& it : src) {
        if (it.name.isEmpty()) continue;
        const QChar c = it.name.at(0);
        if (!c.isLetter() && !c.isDigit()) f.allFiltered.push_back(it);
    }
    f.totalCount = f.allFiltered.size();
    const int start = f.page * PageSize;
    const int end = qMin<int>(start + PageSize, f.allFiltered.size());
    QList<JellyfinItem> slice;
    slice.reserve(end - start);
    for (int i = start; i < end; ++i) slice.push_back(f.allFiltered[i]);
    populateGrid(slice);
    rebuildPaginationBar();
}

void BrowserWidget::rebuildPaginationBar() {
    // Clear existing widgets (keep no fixed children — recreate every refresh).
    QLayoutItem* item;
    while ((item = m_paginationLayout->takeAt(0)) != nullptr) {
        if (auto* w = item->widget()) w->deleteLater();
        delete item;
    }
    if (m_stack.isEmpty()) return;
    const Frame& f = m_stack.top();
    if (!f.paginated) { m_paginationBar->hide(); return; }
    const int totalPages = qMax(1, (f.totalCount + PageSize - 1) / PageSize);
    if (totalPages <= 1 && f.totalCount <= PageSize) {
        if (f.totalCount > 0) {
            auto* info = new QLabel(tr("%1 elementos").arg(f.totalCount), m_paginationBar);
            info->setAlignment(Qt::AlignCenter);
            m_paginationLayout->addStretch();
            m_paginationLayout->addWidget(info);
            m_paginationLayout->addStretch();
            m_paginationBar->show();
        } else {
            m_paginationBar->hide();
        }
        return;
    }
    m_paginationBar->show();

    auto addNav = [&](const QString& text, int target, bool enabled) {
        auto* b = new QToolButton(m_paginationBar);
        b->setText(text);
        b->setEnabled(enabled);
        connect(b, &QToolButton::clicked, this, [this, target]() { onPagePicked(target); });
        m_paginationLayout->addWidget(b);
    };
    auto addPage = [&](int p) {
        auto* b = new QToolButton(m_paginationBar);
        b->setText(QString::number(p + 1));
        b->setCheckable(true);
        b->setChecked(p == f.page);
        connect(b, &QToolButton::clicked, this, [this, p]() { onPagePicked(p); });
        m_paginationLayout->addWidget(b);
    };

    m_paginationLayout->addStretch();
    addNav(tr("« Inicio"), 0, f.page > 0);
    addNav(tr("‹ Ant."), f.page - 1, f.page > 0);

    // Compact: show a window of pages around the current one.
    const int half = MaxPaginationButtons / 2;
    int firstPage = qMax(0, f.page - half);
    int lastPage  = qMin(totalPages - 1, firstPage + MaxPaginationButtons - 1);
    firstPage = qMax(0, lastPage - MaxPaginationButtons + 1);
    if (firstPage > 0) {
        addPage(0);
        if (firstPage > 1) {
            auto* lbl = new QLabel("…", m_paginationBar);
            lbl->setAlignment(Qt::AlignCenter);
            m_paginationLayout->addWidget(lbl);
        }
    }
    for (int p = firstPage; p <= lastPage; ++p) addPage(p);
    if (lastPage < totalPages - 1) {
        if (lastPage < totalPages - 2) {
            auto* lbl = new QLabel("…", m_paginationBar);
            lbl->setAlignment(Qt::AlignCenter);
            m_paginationLayout->addWidget(lbl);
        }
        addPage(totalPages - 1);
    }

    addNav(tr("Sig. ›"), f.page + 1, f.page < totalPages - 1);
    addNav(tr("Final »"), totalPages - 1, f.page < totalPages - 1);

    m_paginationLayout->addSpacing(16);
    auto* info = new QLabel(tr("%1 elementos").arg(f.totalCount), m_paginationBar);
    info->setAlignment(Qt::AlignCenter);
    m_paginationLayout->addWidget(info);
    m_paginationLayout->addStretch();
}

// ---- Jellyfin signal handlers ----

void BrowserWidget::onViewsLoaded(const QList<JellyfinItem>& items) {
    // First version: keep only Movies / TV-Shows libraries.
    m_libraries.clear();
    for (const auto& lib : items) {
        if (lib.collectionType == QStringLiteral("movies")
            || lib.collectionType == QStringLiteral("tvshows")) {
            m_libraries.push_back(lib);
        }
    }
    rebuildLibrarySidebar();
    rebuildHomeLatestRows();
    emit librariesChanged(m_libraries);
}

bool BrowserWidget::isLibraryHidden(const QString& id) const {
    return QSettings().value(QStringLiteral("hiddenLibraries"))
        .toStringList().contains(id);
}

void BrowserWidget::setLibraryHidden(const QString& id, bool hidden) {
    QSettings s;
    QStringList list = s.value(QStringLiteral("hiddenLibraries")).toStringList();
    if (hidden) {
        if (!list.contains(id)) list << id;
    } else {
        list.removeAll(id);
    }
    s.setValue(QStringLiteral("hiddenLibraries"), list);
    rebuildLibrarySidebar();
    rebuildHomeLatestRows();
    emit librariesChanged(m_libraries);
}

void BrowserWidget::rebuildLibrarySidebar() {
    // Wipe existing library entries beyond the section header. We keep
    // index 0 (Inicio) and index 1 (Bibliotecas header).
    while (m_sidebar->count() > 2) {
        delete m_sidebar->takeItem(m_sidebar->count() - 1);
    }
    QStyle* st = style();
    for (const auto& lib : m_libraries) {
        if (isLibraryHidden(lib.id)) continue;
        auto* w = new QListWidgetItem(lib.name);
        w->setIcon(st->standardIcon(QStyle::SP_DirIcon));
        w->setData(Qt::UserRole, lib.id);
        w->setSizeHint(QSize(0, 32));
        m_sidebar->addItem(w);
    }
}

void BrowserWidget::rebuildHomeLatestRows() {
    for (auto* row : m_latestRows) {
        m_homeLayout->removeWidget(row);
        row->deleteLater();
    }
    m_latestRows.clear();

    for (const auto& lib : m_libraries) {
        if (lib.type != "CollectionFolder") continue;
        if (isLibraryHidden(lib.id)) continue;
        auto* row = new MediaRow(
            QStringLiteral("latest:") + lib.id,
            tr("Último añadido — %1").arg(lib.name),
            MediaRow::PosterShape::Vertical,
            m_homeLayout->parentWidget());
        row->setSeeAllVisible(true);
        connect(row, &MediaRow::itemActivated,
                this, &BrowserWidget::onRowItemActivated);
        connect(row, &MediaRow::playClicked, this,
                [this](const JellyfinItem& it) {
                    emit playRequested(it, it.resumePositionTicks);
                });
        connect(row, &MediaRow::seeAllClicked,
                this, &BrowserWidget::onRowSeeAll);
        const int insertAt = m_homeLayout->count() - 1;
        m_homeLayout->insertWidget(insertAt, row);
        m_latestRows.insert(lib.id, row);
        m_client->fetchLatest(lib.id);
    }
}

void BrowserWidget::onResumeLoaded(const QList<JellyfinItem>& items) {
    m_resumeRow->setVisible(!items.isEmpty());
    if (items.isEmpty()) return;
    m_resumeRow->setItems(items);
    for (const auto& it : items) {
        fetchPoster(nullptr, it, m_resumeRow->posterSize(), /*useBackdrop=*/true);
    }
}

void BrowserWidget::onLatestLoaded(const QString& parentId, const QList<JellyfinItem>& items) {
    auto it = m_latestRows.find(parentId);
    if (it == m_latestRows.end()) return;
    MediaRow* row = it.value();
    row->setVisible(!items.isEmpty());
    if (items.isEmpty()) return;
    row->setItems(items);
    for (const auto& jit : items) {
        fetchPoster(nullptr, jit, row->posterSize(), /*useBackdrop=*/false);
    }
}

void BrowserWidget::onItemsLoaded(const QString& parentId,
                                  const QList<JellyfinItem>& items, int totalCount) {
    if (m_pages->currentWidget() != m_gridPage) return;
    if (m_stack.isEmpty() || m_stack.top().parentId != parentId) return;
    Frame& f = m_stack.top();
    f.items = items;
    f.totalCount = totalCount;

    if (f.paginated && f.letter == QChar('?')) {
        applyClientSideFilter(items);
        return;
    }
    populateGrid(items);
    if (f.paginated) rebuildPaginationBar();
}

// ---- Grid population & navigation ----

void BrowserWidget::populateGrid(const QList<JellyfinItem>& items) {
    m_grid->clear();
    ++m_frameGen;
    for (const auto& it : items) {
        QString title = it.name;
        if (it.type == "Episode" && it.parentIndexNumber > 0 && it.indexNumber > 0) {
            title = QString("S%1E%2 — %3")
                        .arg(it.parentIndexNumber, 2, 10, QChar('0'))
                        .arg(it.indexNumber, 2, 10, QChar('0'))
                        .arg(it.name);
        }
        const QString year = it.productionYear > 0
                                 ? QString::number(it.productionYear)
                                 : QString();
        auto* w = new QListWidgetItem(m_placeholder, title);
        w->setData(Qt::UserRole + 1, it.id);
        w->setData(Qt::UserRole + 2, it.type);
        w->setData(Qt::UserRole + 3, it.isFolder);
        w->setData(Qt::UserRole + 4, it.seriesId);
        w->setData(Qt::UserRole + 5, (qlonglong) it.resumePositionTicks);
        w->setData(Qt::UserRole + 6, (qlonglong) it.runTimeTicks);
        w->setData(PlayEmblemDelegate::PlayableRole, it.playable());
        w->setData(PlayEmblemDelegate::YearRole, year);
        w->setTextAlignment(Qt::AlignHCenter);
        m_grid->addItem(w);
        fetchPoster(w, it, QSize(GridIconW, GridIconH), /*useBackdrop=*/false);
    }
}

void BrowserWidget::onItemActivated(QListWidgetItem* w) {
    const QString id = w->data(Qt::UserRole + 1).toString();
    const QString type = w->data(Qt::UserRole + 2).toString();
    const bool isFolder = w->data(Qt::UserRole + 3).toBool();
    const QString seriesId = w->data(Qt::UserRole + 4).toString();
    const qint64 resumeTicks = (qint64) w->data(Qt::UserRole + 5).toLongLong();
    const qint64 runTicks = (qint64) w->data(Qt::UserRole + 6).toLongLong();

    if (type == "Series") {
        pushHistory();
        Frame f; f.title = w->text(); f.parentId = id;
        m_stack.push(std::move(f));
        updateHeader();
        m_grid->clear();
        ++m_frameGen;
        m_client->fetchEpisodes(id);
        return;
    }
    if (isFolder || type == "CollectionFolder" || type == "Folder"
        || type == "Season" || type == "BoxSet") {
        pushHistory();
        Frame f; f.title = w->text(); f.parentId = id;
        m_stack.push(std::move(f));
        updateHeader();
        m_grid->clear();
        ++m_frameGen;
        requestForFrame();
        return;
    }
    // Playable leaf → request the detail view, not playback.
    JellyfinItem it;
    it.id = id; it.type = type; it.name = w->text();
    it.seriesId = seriesId;
    it.resumePositionTicks = resumeTicks;
    it.runTimeTicks = runTicks;
    showDetail(it);
    // Drop the grid's selection so coming back from detail isn't ringed in blue.
    m_grid->clearSelection();
}

void BrowserWidget::onBackClicked() {
    // Kept for compatibility with the rest of the code; navigation is now
    // driven by the breadcrumb. Equivalent to "go up one level".
    if (m_stack.size() <= 1) return;
    popToBreadcrumb(m_stack.size() - 2);
}

void BrowserWidget::onRowItemActivated(const JellyfinItem& it) {
    if (it.type == "Series") {
        pushHistory();
        Frame f; f.title = it.name; f.parentId = it.id;
        m_stack.clear();
        m_stack.push(std::move(f));
        m_pages->setCurrentWidget(m_gridPage);
        updateHeader();
        m_grid->clear();
        ++m_frameGen;
        m_client->fetchEpisodes(it.id);
        return;
    }
    showDetail(it);
}

void BrowserWidget::onRowSeeAll(const QString& sectionId) {
    if (sectionId.startsWith("latest:")) {
        const QString libId = sectionId.mid(7);
        for (int i = 0; i < m_sidebar->count(); ++i) {
            QListWidgetItem* w = m_sidebar->item(i);
            if (w->data(Qt::UserRole).toString() == libId) {
                m_sidebar->setCurrentItem(w);
                return;
            }
        }
    }
}

// ---- Poster fetching (shared between grid and rows) ----

void BrowserWidget::fetchPoster(QListWidgetItem* w, const JellyfinItem& it,
                                const QSize& iconSize, bool useBackdrop) {
    QString imageId = it.id;
    QString tag = it.primaryImageTag;
    QString type = "Primary";
    if (useBackdrop) {
        type = "Backdrop";
        // We don't track backdrop tags; rely on the URL alone (server can
        // resolve), and use the series' backdrop for nicer art on episodes.
        tag = QString();
        if (it.type == "Episode" && !it.seriesId.isEmpty()) imageId = it.seriesId;
    } else if (tag.isEmpty() && !it.seriesId.isEmpty() && !it.seriesPrimaryImageTag.isEmpty()) {
        imageId = it.seriesId;
        tag = it.seriesPrimaryImageTag;
    }

    QUrl u = m_client->imageUrl(imageId, tag, type, iconSize.height() * 2);
    const QString cacheKey = u.toString() + "|" + QString::number(iconSize.width()) + "x"
                             + QString::number(iconSize.height());

    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) {
        const QIcon icon(cached);
        if (w) w->setIcon(icon);
        else {
            // Find the row that owns this item and update it.
            if (useBackdrop) m_resumeRow->setItemIcon(it.id, icon);
            for (auto* row : m_latestRows) row->setItemIcon(it.id, icon);
        }
        return;
    }

    QNetworkRequest req(u);
    req.setRawHeader("X-Emby-Authorization",
                     QString("MediaBrowser Token=\"%1\"").arg(m_client->accessToken()).toUtf8());
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                     QNetworkRequest::PreferCache);
    req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);

    QNetworkReply* reply = m_imgNam->get(req);
    const quint64 gen = m_frameGen;
    const QString itemId = it.id;
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, w, gen, cacheKey, iconSize, itemId, useBackdrop]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        QPixmap pm;
        if (!pm.loadFromData(reply->readAll())) return;
        const QIcon icon = iconFromPixmap(pm, iconSize);
        QPixmapCache::insert(cacheKey, pm.scaled(iconSize, Qt::KeepAspectRatio,
                                                 Qt::SmoothTransformation));
        if (w) {
            if (gen != m_frameGen) return;
            w->setIcon(icon);
        } else {
            if (useBackdrop) m_resumeRow->setItemIcon(itemId, icon);
            for (auto* row : m_latestRows) row->setItemIcon(itemId, icon);
        }
    });
}

// ---- Grid centering ----

bool BrowserWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_gridPage && event->type() == QEvent::Resize
        && m_letterPanel && m_letterPanel->isVisible()) {
        const int panelW = m_letterPanel->width();
        m_letterPanel->setGeometry(m_gridPage->width() - panelW, 0,
                                    panelW, m_gridPage->height());
    }
    // While the letter panel is open, an app-wide filter is installed so any
    // mouse press outside the panel (and outside the A-Z toggle, which manages
    // its own open/close) dismisses it.
    if (m_letterPanel && m_letterPanel->isVisible()
        && event->type() == QEvent::MouseButtonPress) {
        if (auto* w = qobject_cast<QWidget*>(watched)) {
            const bool insidePanel = (w == m_letterPanel) || m_letterPanel->isAncestorOf(w);
            const bool onToggle = (w == m_letterBtn) || m_letterBtn->isAncestorOf(w);
            if (!insidePanel && !onToggle) slideOutLetterPanel();
        }
    }
    if (m_grid && watched == m_grid->viewport()) {
        if (event->type() == QEvent::Resize) {
            recenterGrid();
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                if (QListWidgetItem* w = m_grid->itemAt(me->pos())) {
                    const QRect cell = m_grid->visualItemRect(w);
                    const QString type = w->data(Qt::UserRole + 2).toString();
                    const bool isFolder = w->data(Qt::UserRole + 3).toBool();
                    const bool playable = !isFolder
                        && (type == "Movie" || type == "Episode" || type == "Video");
                    if (playable && clickHitsPlayEmblem(me->pos(), cell, m_grid->iconSize())) {
                        JellyfinItem it;
                        it.id = w->data(Qt::UserRole + 1).toString();
                        it.type = type;
                        it.name = w->text();
                        it.seriesId = w->data(Qt::UserRole + 4).toString();
                        it.resumePositionTicks =
                            (qint64) w->data(Qt::UserRole + 5).toLongLong();
                        it.runTimeTicks =
                            (qint64) w->data(Qt::UserRole + 6).toLongLong();
                        emit playRequested(it, it.resumePositionTicks);
                        return true;
                    }
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void BrowserWidget::recenterGrid() {
    const int spacing = m_grid->spacing();
    const int cellW = m_grid->gridSize().width() + spacing * 2;
    const int totalW = m_grid->width()
                     - m_grid->frameWidth() * 2
                     - (m_grid->verticalScrollBar()->isVisible()
                            ? m_grid->verticalScrollBar()->width() : 0);
    if (cellW <= 0 || totalW <= 0) return;
    const int columns = qMax(1, totalW / cellW);
    const int leftover = qMax(0, totalW - columns * cellW);
    const int sideMargin = leftover / 2;
    if (sideMargin == m_currentSideMargin) return;
    m_currentSideMargin = sideMargin;
    static_cast<CenterableList*>(m_grid)->setViewportMargins(sideMargin, 0, sideMargin, 0);
}
