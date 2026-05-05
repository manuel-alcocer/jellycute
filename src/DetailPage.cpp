#include "DetailPage.h"

#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int PosterW = 260;
constexpr int PosterH = 390;
}

DetailPage::DetailPage(JellyfinClient* client, QWidget* parent)
    : QWidget(parent), m_client(client),
      m_imgNam(new QNetworkAccessManager(this)) {

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 12, 16, 12);
    root->setSpacing(10);

    // Breadcrumb header (clickable ancestors + current item).
    m_breadcrumb = new QWidget(this);
    m_breadcrumbLayout = new QHBoxLayout(m_breadcrumb);
    m_breadcrumbLayout->setContentsMargins(4, 0, 4, 0);
    m_breadcrumbLayout->setSpacing(0);
    m_breadcrumbLayout->addStretch();
    root->addWidget(m_breadcrumb);

    // Card around the body so the detail looks like a Inicio-style section.
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("detailCard"));
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 12, 12, 12);
    cardLayout->setSpacing(0);
    root->addWidget(card, 1);

    auto* scroll = new QScrollArea(card);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* container = new QWidget(scroll);
    auto* body = new QHBoxLayout(container);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(20);

    // Poster
    m_posterLabel = new QLabel(container);
    m_posterLabel->setFixedSize(PosterW, PosterH);
    m_posterLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_posterLabel->setStyleSheet(
        "background:#222; border:1px solid #d0d2d4; border-radius:6px;");
    body->addWidget(m_posterLabel, 0, Qt::AlignTop);

    // Right column
    auto* col = new QVBoxLayout;
    col->setSpacing(10);
    body->addLayout(col, 1);

    m_titleLabel = new QLabel(container);
    QFont tf = m_titleLabel->font();
    tf.setPointSize(tf.pointSize() + 12);
    tf.setBold(true);
    m_titleLabel->setFont(tf);
    m_titleLabel->setWordWrap(true);
    col->addWidget(m_titleLabel);

    m_metaLabel = new QLabel(container);
    m_metaLabel->setStyleSheet("color:#5d6164;");
    col->addWidget(m_metaLabel);

    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    m_playBtn = new QPushButton(tr("▶ Reproducir desde el inicio"), container);
    m_resumeBtn = new QPushButton(container);
    m_favoriteBtn = new QPushButton(container);
    m_watchedBtn = new QPushButton(container);
    m_unwatchedBtn = new QPushButton(container);
    btnRow->addWidget(m_playBtn);
    btnRow->addWidget(m_resumeBtn);
    btnRow->addWidget(m_favoriteBtn);
    btnRow->addWidget(m_watchedBtn);
    btnRow->addWidget(m_unwatchedBtn);
    btnRow->addStretch();
    col->addLayout(btnRow);

    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        emit playRequested(m_item, 0);
    });
    connect(m_resumeBtn, &QPushButton::clicked, this, [this]() {
        emit playRequested(m_item, m_item.resumePositionTicks);
    });
    connect(m_favoriteBtn, &QPushButton::clicked, this, [this]() {
        m_client->setFavorite(m_item.id, !m_item.isFavorite);
    });
    connect(m_watchedBtn, &QPushButton::clicked, this, [this]() {
        m_client->setPlayed(m_item.id, true);
    });
    connect(m_unwatchedBtn, &QPushButton::clicked, this, [this]() {
        m_client->setPlayed(m_item.id, false);
    });

    m_overviewLabel = new QLabel(container);
    m_overviewLabel->setWordWrap(true);
    m_overviewLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    col->addWidget(m_overviewLabel);

    auto addInfoRow = [&](const QString& label) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* k = new QLabel(label, container);
        k->setStyleSheet("color:#5d6164; min-width:80px;");
        k->setMinimumWidth(80);
        auto* v = new QLabel(container);
        v->setWordWrap(true);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(k, 0, Qt::AlignTop);
        row->addWidget(v, 1, Qt::AlignTop);
        col->addLayout(row);
        return v;
    };

    m_genresLabel    = addInfoRow(tr("Géneros"));
    m_directorsLabel = addInfoRow(tr("Director"));
    m_writersLabel   = addInfoRow(tr("Escritor"));
    m_castLabel      = addInfoRow(tr("Reparto"));
    m_studiosLabel   = addInfoRow(tr("Estudios"));
    m_tagsLabel      = addInfoRow(tr("Etiquetas"));

    col->addStretch();

    scroll->setWidget(container);
    cardLayout->addWidget(scroll);

    connect(m_client, &JellyfinClient::itemDetailsLoaded,
            this, &DetailPage::onItemDetailsLoaded);
    connect(m_client, &JellyfinClient::favoriteToggled,
            this, &DetailPage::onFavoriteToggled);
    connect(m_client, &JellyfinClient::playedToggled,
            this, &DetailPage::onPlayedToggled);
}

void DetailPage::showItem(const JellyfinItem& item, const QStringList& ancestors) {
    m_item = item;
    m_ancestors = ancestors;
    rebuildBreadcrumb();
    rebuildInfo();
    fetchPoster();
    // Always refresh the full record from the server: the grid only carries
    // a subset (name/poster/userdata) and never has Overview / People.
    m_client->fetchItemDetails(item.id);
}

void DetailPage::rebuildBreadcrumb() {
    QLayoutItem* li;
    while ((li = m_breadcrumbLayout->takeAt(0)) != nullptr) {
        if (auto* w = li->widget()) w->deleteLater();
        delete li;
    }
    auto addSep = [&]() {
        auto* sep = new QLabel(QStringLiteral("›"), m_breadcrumb);
        sep->setStyleSheet("color:#7f8c8d; padding: 0 6px;");
        QFont sf = sep->font(); sf.setPointSize(sf.pointSize() + 4); sep->setFont(sf);
        m_breadcrumbLayout->addWidget(sep);
    };
    for (int i = 0; i < m_ancestors.size(); ++i) {
        auto* btn = new QToolButton(m_breadcrumb);
        btn->setText(m_ancestors[i]);
        btn->setAutoRaise(true);
        btn->setCursor(Qt::PointingHandCursor);
        QFont bf = btn->font(); bf.setPointSize(bf.pointSize() + 4); btn->setFont(bf);
        const int target = i;
        connect(btn, &QToolButton::clicked, this, [this, target]() {
            emit breadcrumbClicked(target);
        });
        m_breadcrumbLayout->addWidget(btn);
        addSep();
    }
    auto* current = new QLabel(m_item.name, m_breadcrumb);
    QFont cf = current->font();
    cf.setBold(true);
    cf.setPointSize(cf.pointSize() + 4);
    current->setFont(cf);
    m_breadcrumbLayout->addWidget(current);
    m_breadcrumbLayout->addStretch();
}

QString DetailPage::formatRuntime(qint64 ticks) {
    if (ticks <= 0) return {};
    const qint64 totalSeconds = ticks / 10'000'000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 mins = (totalSeconds % 3600) / 60;
    if (hours > 0) return QString("%1h %2m").arg(hours).arg(mins, 2, 10, QChar('0'));
    return QString("%1 min").arg(mins);
}

void DetailPage::rebuildInfo() {
    QString title = m_item.name;
    if (m_item.type == "Episode" && !m_item.seriesName.isEmpty()) {
        title = QString("%1 — S%2E%3 %4")
                    .arg(m_item.seriesName)
                    .arg(m_item.parentIndexNumber, 2, 10, QChar('0'))
                    .arg(m_item.indexNumber, 2, 10, QChar('0'))
                    .arg(m_item.name);
    }
    m_titleLabel->setText(title);

    QStringList meta;
    if (m_item.productionYear > 0)         meta << QString::number(m_item.productionYear);
    const QString rt = formatRuntime(m_item.runTimeTicks);
    if (!rt.isEmpty())                     meta << rt;
    if (!m_item.officialRating.isEmpty())  meta << m_item.officialRating;
    if (m_item.communityRating > 0.0)
        meta << QString("★ %1").arg(QString::number(m_item.communityRating, 'f', 1));
    m_metaLabel->setText(meta.join("  •  "));

    m_overviewLabel->setText(m_item.overview);
    m_overviewLabel->setVisible(!m_item.overview.isEmpty());

    m_genresLabel->setText(m_item.genres.join(", "));
    m_genresLabel->parentWidget()->parentWidget()->setVisible(true);

    QStringList directors, writers, cast, castWithRoles;
    for (const auto& p : m_item.people) {
        if (p.type == "Director") directors << p.name;
        else if (p.type == "Writer") writers << p.name;
        else if (p.type == "Actor")  cast << (p.role.isEmpty()
                                                  ? p.name
                                                  : QString("%1 (%2)").arg(p.name, p.role));
    }
    m_directorsLabel->setText(directors.join(", "));
    m_writersLabel->setText(writers.join(", "));
    m_castLabel->setText(cast.join(", "));
    m_studiosLabel->setText(m_item.studios.join(", "));
    m_tagsLabel->setText(m_item.tags.join(", "));

    auto hideIfEmpty = [](QLabel* lbl) {
        // Hide both label cells (k + v) by toggling visibility on the row.
        if (auto* row = lbl->parentWidget()) {
            // The HBox layout containing this label was added directly to the
            // info column; siblings stay visible. Hiding the label keeps the
            // row collapsed visually because both cells are in the same row.
        }
        lbl->setVisible(!lbl->text().isEmpty());
    };
    hideIfEmpty(m_genresLabel);
    hideIfEmpty(m_directorsLabel);
    hideIfEmpty(m_writersLabel);
    hideIfEmpty(m_castLabel);
    hideIfEmpty(m_studiosLabel);
    hideIfEmpty(m_tagsLabel);

    updateActionButtons();
}

void DetailPage::updateActionButtons() {
    const bool isPlayable = m_item.playable();
    m_playBtn->setVisible(isPlayable);
    m_resumeBtn->setVisible(isPlayable && m_item.resumePositionTicks > 0);
    if (m_resumeBtn->isVisible()) {
        const qint64 secs = m_item.resumePositionTicks / 10'000'000;
        m_resumeBtn->setText(QString("⟳ %1 %2:%3:%4")
                                 .arg(tr("Reanudar"))
                                 .arg(secs / 3600, 2, 10, QChar('0'))
                                 .arg((secs % 3600) / 60, 2, 10, QChar('0'))
                                 .arg(secs % 60, 2, 10, QChar('0')));
    }
    m_favoriteBtn->setText(m_item.isFavorite
                               ? tr("♥ En favoritos")
                               : tr("♡ Añadir a favoritos"));
    m_watchedBtn->setText(tr("✓ Marcar como visto"));
    m_unwatchedBtn->setText(tr("○ Marcar como no visto"));
    m_watchedBtn->setVisible(isPlayable && !m_item.isPlayed);
    m_unwatchedBtn->setVisible(isPlayable && m_item.isPlayed);
}

void DetailPage::fetchPoster() {
    QString tag = m_item.primaryImageTag;
    QString id = m_item.id;
    if (tag.isEmpty() && !m_item.seriesPrimaryImageTag.isEmpty()) {
        tag = m_item.seriesPrimaryImageTag;
        id = m_item.seriesId;
    }
    if (tag.isEmpty()) {
        m_posterLabel->setPixmap(QPixmap());
        return;
    }
    QUrl u = m_client->imageUrl(id, tag, "Primary", PosterH * 2);
    QNetworkRequest req(u);
    req.setRawHeader("X-Emby-Authorization",
                     QString("MediaBrowser Token=\"%1\"").arg(m_client->accessToken()).toUtf8());
    QNetworkReply* reply = m_imgNam->get(req);
    const QString currentId = m_item.id;
    connect(reply, &QNetworkReply::finished, this, [this, reply, currentId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        if (m_item.id != currentId) return;       // user navigated away
        QPixmap pm;
        if (!pm.loadFromData(reply->readAll())) return;
        m_posterLabel->setPixmap(pm.scaled(PosterW, PosterH,
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
    });
}

void DetailPage::onItemDetailsLoaded(const JellyfinItem& item) {
    if (item.id != m_item.id) return;
    m_item = item;
    rebuildInfo();
    // The grid hands us a partial item without image tags, so the first
    // fetchPoster() call inside showItem() found nothing. Now that we have
    // the full record, retry.
    fetchPoster();
}

void DetailPage::onFavoriteToggled(const QString& itemId, bool fav) {
    if (itemId != m_item.id) return;
    m_item.isFavorite = fav;
    updateActionButtons();
}

void DetailPage::onPlayedToggled(const QString& itemId, bool played) {
    if (itemId != m_item.id) return;
    m_item.isPlayed = played;
    if (played) m_item.resumePositionTicks = 0;
    updateActionButtons();
}
