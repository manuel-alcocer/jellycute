#include "MediaRow.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

namespace {
constexpr int VerticalIconW = 160;
constexpr int VerticalIconH = 240;
constexpr int BackdropIconW = 280;
constexpr int BackdropIconH = 158;       // ~16:9
constexpr int LabelLines = 2;
constexpr int Spacing = 12;
constexpr int PlayEmblemSize = 36;
constexpr int PlayEmblemMargin = 8;

bool clickHitsPlayEmblem(const QPoint& posInViewport, const QRect& cellRect,
                         const QSize& iconSize) {
    const int ix = cellRect.x() + (cellRect.width() - iconSize.width()) / 2;
    const int iy = cellRect.y() + 4;
    const QRect emblem(ix + iconSize.width() - PlayEmblemMargin - PlayEmblemSize,
                       iy + iconSize.height() - PlayEmblemMargin - PlayEmblemSize,
                       PlayEmblemSize, PlayEmblemSize);
    return emblem.contains(posInViewport);
}
}

MediaRow::MediaRow(const QString& sectionId, const QString& title,
                   PosterShape shape, QWidget* parent)
    : QWidget(parent), m_sectionId(sectionId), m_shape(shape) {
    setObjectName(QStringLiteral("mediaRow"));
    setAttribute(Qt::WA_StyledBackground);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(8);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    m_title = new QLabel(title, this);
    QFont f = m_title->font();
    f.setPointSize(f.pointSize() + 4);
    f.setBold(true);
    m_title->setFont(f);

    m_seeAll = new QPushButton(tr("Ver Todo"), this);
    m_seeAll->setFlat(true);
    m_seeAll->setCursor(Qt::PointingHandCursor);
    m_seeAll->setStyleSheet("color: #f59e0b; border: none; padding: 2px 6px;");
    m_seeAll->setVisible(false);

    header->addWidget(m_title);
    header->addStretch();
    header->addWidget(m_seeAll);

    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setFlow(QListView::LeftToRight);
    m_list->setWrapping(false);
    m_list->setMovement(QListView::Static);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setIconSize(posterSize());
    m_list->setSpacing(Spacing);
    m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setWordWrap(true);
    // IconMode + LeftToRight reports a sizeHint covering all items, which
    // forces parents wider than the viewport when the row has many posters.
    // Ignore the horizontal hint so the list takes whatever width the parent
    // grants and lets its internal h-scrollbar handle the overflow.
    QSizePolicy listPolicy = m_list->sizePolicy();
    listPolicy.setHorizontalPolicy(QSizePolicy::Ignored);
    m_list->setSizePolicy(listPolicy);
    m_list->setMinimumWidth(0);

    const QFontMetrics fm(font());
    const int textBlock = fm.lineSpacing() * LabelLines + 6;
    const int rowH = posterSize().height() + textBlock + Spacing * 2;
    m_list->setFixedHeight(rowH);
    m_list->setGridSize(QSize(posterSize().width() + Spacing * 2, rowH - Spacing));

    root->addLayout(header);
    root->addWidget(m_list);

    connect(m_list, &QListWidget::itemClicked, this, &MediaRow::onActivated);
    connect(m_seeAll, &QPushButton::clicked, this, [this]() {
        emit seeAllClicked(m_sectionId);
    });
    m_list->viewport()->installEventFilter(this);
}

bool MediaRow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_list->viewport()
        && event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (QListWidgetItem* w = m_list->itemAt(me->pos())) {
                const QRect cell = m_list->visualItemRect(w);
                if (clickHitsPlayEmblem(me->pos(), cell, m_list->iconSize())) {
                    const QString id = w->data(Qt::UserRole).toString();
                    for (const auto& it : m_items) {
                        if (it.id == id && it.playable()) {
                            emit playClicked(it);
                            return true;
                        }
                    }
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

QSize MediaRow::posterSize() const {
    return m_shape == PosterShape::Vertical
               ? QSize(VerticalIconW, VerticalIconH)
               : QSize(BackdropIconW, BackdropIconH);
}

void MediaRow::setSeeAllVisible(bool v) { m_seeAll->setVisible(v); }

void MediaRow::clear() {
    m_list->clear();
    m_byId.clear();
    m_items.clear();
}

bool MediaRow::isEmpty() const { return m_items.isEmpty(); }

void MediaRow::setItems(const QList<JellyfinItem>& items) {
    clear();
    m_items = items;

    QPixmap placeholderPm(posterSize());
    placeholderPm.fill(QColor(40, 40, 40));
    const QIcon placeholder(placeholderPm);

    for (const auto& it : items) {
        QString label = it.name;
        if (it.type == "Episode" && it.parentIndexNumber > 0 && it.indexNumber > 0) {
            label = QString("S%1E%2 — %3")
                        .arg(it.parentIndexNumber, 2, 10, QChar('0'))
                        .arg(it.indexNumber, 2, 10, QChar('0'))
                        .arg(it.name);
        }
        if (it.productionYear > 0 && it.type == "Movie") {
            label = QString("%1\n%2").arg(it.name).arg(it.productionYear);
        }

        auto* w = new QListWidgetItem(placeholder, label);
        w->setData(Qt::UserRole, it.id);
        w->setTextAlignment(Qt::AlignHCenter);
        m_list->addItem(w);
        m_byId.insert(it.id, w);
    }
}

void MediaRow::setItemIcon(const QString& itemId, const QIcon& icon) {
    auto it = m_byId.constFind(itemId);
    if (it == m_byId.constEnd()) return;
    it.value()->setIcon(icon);
}

void MediaRow::onActivated(QListWidgetItem* w) {
    const QString id = w->data(Qt::UserRole).toString();
    for (const auto& it : m_items) {
        if (it.id == id) {
            emit itemActivated(it);
            m_list->clearSelection();
            return;
        }
    }
}
