#include "DetailPage.h"
#include "MediaRow.h"

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
#include <QPixmapCache>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
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
    auto* containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(20);

    // Top section: poster (left) + info column (right).
    auto* topSection = new QWidget(container);
    auto* body = new QHBoxLayout(topSection);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(20);
    containerLayout->addWidget(topSection);

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
        // Wrap each k/v pair in a QWidget so hideIfEmpty can collapse the
        // whole row (both the key and the value) when there's no data.
        auto* row = new QWidget(container);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(8);
        auto* k = new QLabel(label, row);
        k->setStyleSheet("color:#5d6164; min-width:80px;");
        k->setMinimumWidth(80);
        auto* v = new QLabel(row);
        v->setWordWrap(true);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        rowLayout->addWidget(k, 0, Qt::AlignTop);
        rowLayout->addWidget(v, 1, Qt::AlignTop);
        col->addWidget(row);
        return v;
    };

    m_genresLabel    = addInfoRow(tr("Géneros"));
    m_directorsLabel = addInfoRow(tr("Director"));
    m_writersLabel   = addInfoRow(tr("Escritor"));
    m_castLabel      = addInfoRow(tr("Reparto"));
    m_studiosLabel   = addInfoRow(tr("Estudios"));
    m_tagsLabel      = addInfoRow(tr("Etiquetas"));

    m_containerLabel       = addInfoRow(tr("Archivo"));
    m_videoStreamLabel     = addInfoRow(tr("Vídeo"));
    m_audioStreamsLabel    = addInfoRow(tr("Audio"));
    m_subtitleStreamsLabel = addInfoRow(tr("Subtítulos"));

    col->addStretch();

    // Related-content rows (under the top section, capped at 10 combined).
    auto buildRow = [&](const QString& sectionId, const QString& title) {
        auto* row = new MediaRow(sectionId, title,
                                 MediaRow::PosterShape::Vertical, container);
        row->setSeeAllVisible(false);
        row->setVisible(false);
        connect(row, &MediaRow::itemActivated, this,
                [this](const JellyfinItem& it) { emit itemSelected(it); });
        connect(row, &MediaRow::playClicked, this,
                [this](const JellyfinItem& it) {
                    emit playRequested(it, it.resumePositionTicks);
                });
        containerLayout->addWidget(row);
        return row;
    };
    m_sagaRow    = buildRow(QStringLiteral("relatedSaga"),    tr("De la saga"));
    m_genreRow   = buildRow(QStringLiteral("relatedGenre"),   tr("Mismo género"));
    m_similarRow = buildRow(QStringLiteral("relatedSimilar"), tr("Similares"));

    containerLayout->addStretch();

    scroll->setWidget(container);
    cardLayout->addWidget(scroll);

    connect(m_client, &JellyfinClient::itemDetailsLoaded,
            this, &DetailPage::onItemDetailsLoaded);
    connect(m_client, &JellyfinClient::favoriteToggled,
            this, &DetailPage::onFavoriteToggled);
    connect(m_client, &JellyfinClient::playedToggled,
            this, &DetailPage::onPlayedToggled);
    connect(m_client, &JellyfinClient::sagaSiblingsLoaded,
            this, &DetailPage::onSagaSiblingsLoaded);
    connect(m_client, &JellyfinClient::genrePeersLoaded,
            this, &DetailPage::onGenrePeersLoaded);
    connect(m_client, &JellyfinClient::similarLoaded,
            this, &DetailPage::onSimilarLoaded);
}

void DetailPage::showItem(const JellyfinItem& item, const QStringList& ancestors) {
    m_item = item;
    m_ancestors = ancestors;
    clearRelatedRows();
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

    rebuildMediaInfo();

    auto hideIfEmpty = [](QLabel* lbl) {
        // Both key and value are siblings inside the row's wrapper QWidget;
        // toggling the wrapper hides the row entirely.
        if (auto* row = lbl->parentWidget()) row->setVisible(!lbl->text().isEmpty());
    };
    hideIfEmpty(m_genresLabel);
    hideIfEmpty(m_directorsLabel);
    hideIfEmpty(m_writersLabel);
    hideIfEmpty(m_castLabel);
    hideIfEmpty(m_studiosLabel);
    hideIfEmpty(m_tagsLabel);
    hideIfEmpty(m_containerLabel);
    hideIfEmpty(m_videoStreamLabel);
    hideIfEmpty(m_audioStreamsLabel);
    hideIfEmpty(m_subtitleStreamsLabel);

    updateActionButtons();
}

namespace {
QString formatBitrate(qint64 bps) {
    if (bps <= 0) return {};
    if (bps >= 1'000'000)
        return QString::number(bps / 1'000'000.0, 'f', 1) + QStringLiteral(" Mbps");
    return QString::number(bps / 1000) + QStringLiteral(" kbps");
}

QString formatFileSize(qint64 bytes) {
    if (bytes <= 0) return {};
    constexpr qint64 GB = 1'000'000'000;
    constexpr qint64 MB = 1'000'000;
    if (bytes >= GB) return QString::number(bytes / double(GB), 'f', 2) + QStringLiteral(" GB");
    return QString::number(bytes / double(MB), 'f', 0) + QStringLiteral(" MB");
}

QString channelLayout(int channels) {
    switch (channels) {
        case 1: return QStringLiteral("Mono");
        case 2: return QStringLiteral("Stereo");
        case 6: return QStringLiteral("5.1");
        case 8: return QStringLiteral("7.1");
        default: return channels > 0 ? QString::number(channels) + QStringLiteral("ch") : QString();
    }
}

QString streamLine(const JellyfinMediaStream& s, bool includeChannels) {
    QStringList parts;
    if (!s.language.isEmpty()) parts << s.language.toUpper();
    if (!s.codec.isEmpty()) parts << s.codec.toUpper();
    if (includeChannels) {
        const QString ch = channelLayout(s.channels);
        if (!ch.isEmpty()) parts << ch;
    }
    QStringList flags;
    if (s.isDefault) flags << QObject::tr("predeterminado");
    if (s.isForced)  flags << QObject::tr("forzado");
    QString line = QStringLiteral("• ") + parts.join(QStringLiteral(" — "));
    if (!flags.isEmpty()) line += QStringLiteral(" [") + flags.join(", ") + QStringLiteral("]");
    return line;
}
}

void DetailPage::rebuildMediaInfo() {
    if (m_item.mediaSources.isEmpty()) {
        m_containerLabel->clear();
        m_videoStreamLabel->clear();
        m_audioStreamsLabel->clear();
        m_subtitleStreamsLabel->clear();
        return;
    }
    const JellyfinMediaSource& src = m_item.mediaSources.first();

    QStringList containerBits;
    if (!src.container.isEmpty()) containerBits << src.container.toUpper();
    const QString br = formatBitrate(src.bitrate);
    if (!br.isEmpty()) containerBits << br;
    const QString sz = formatFileSize(src.size);
    if (!sz.isEmpty()) containerBits << sz;
    m_containerLabel->setText(containerBits.join(QStringLiteral("  •  ")));

    QStringList audioLines, subLines;
    QString videoLine;
    for (const auto& s : src.streams) {
        if (s.type == "Video" && videoLine.isEmpty()) {
            QStringList parts;
            if (!s.codec.isEmpty()) parts << s.codec.toUpper();
            if (s.width > 0 && s.height > 0)
                parts << QString::number(s.width) + QStringLiteral("×") + QString::number(s.height);
            videoLine = parts.join(QStringLiteral(" — "));
        } else if (s.type == "Audio") {
            audioLines << streamLine(s, /*includeChannels=*/true);
        } else if (s.type == "Subtitle") {
            subLines << streamLine(s, /*includeChannels=*/false);
        }
    }
    m_videoStreamLabel->setText(videoLine);
    m_audioStreamsLabel->setText(audioLines.join(QStringLiteral("\n")));
    m_subtitleStreamsLabel->setText(subLines.join(QStringLiteral("\n")));
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
    // Related-content lookups: only meaningful for movies. Episodes/series
    // would surface unrelated results.
    if (m_item.type == QStringLiteral("Movie")) {
        m_client->fetchSagaSiblings(m_item.id);
        m_client->fetchGenrePeers(m_item.id, m_item.genres, 10);
        m_client->fetchSimilar(m_item.id, 10);
    }
}

void DetailPage::onSagaSiblingsLoaded(const QString& itemId,
                                      const QList<JellyfinItem>& items) {
    if (itemId != m_item.id) return;
    m_sagaItems = items;
    rebuildRelatedRows();
}

void DetailPage::onGenrePeersLoaded(const QString& itemId,
                                    const QList<JellyfinItem>& items) {
    if (itemId != m_item.id) return;
    m_genreItems = items;
    rebuildRelatedRows();
}

void DetailPage::onSimilarLoaded(const QString& itemId,
                                 const QList<JellyfinItem>& items) {
    if (itemId != m_item.id) return;
    m_similarItems = items;
    rebuildRelatedRows();
}

void DetailPage::clearRelatedRows() {
    m_sagaItems.clear();
    m_genreItems.clear();
    m_similarItems.clear();
    if (m_sagaRow)    { m_sagaRow->clear();    m_sagaRow->setVisible(false);    }
    if (m_genreRow)   { m_genreRow->clear();   m_genreRow->setVisible(false);   }
    if (m_similarRow) { m_similarRow->clear(); m_similarRow->setVisible(false); }
}

void DetailPage::rebuildRelatedRows() {
    // Priority order with a hard cap of 10 across the three sections.
    constexpr int kCap = 10;
    QSet<QString> seen{m_item.id};
    auto take = [&](const QList<JellyfinItem>& src, int budget) {
        QList<JellyfinItem> out;
        for (const auto& it : src) {
            if (budget <= 0) break;
            if (it.id.isEmpty() || seen.contains(it.id)) continue;
            seen.insert(it.id);
            out << it;
            --budget;
        }
        return out;
    };

    int remaining = kCap;
    const auto saga    = take(m_sagaItems,    remaining); remaining -= saga.size();
    const auto genre   = take(m_genreItems,   remaining); remaining -= genre.size();
    const auto similar = take(m_similarItems, remaining); remaining -= similar.size();

    auto fill = [this](MediaRow* row, const QList<JellyfinItem>& items) {
        row->clear();
        if (items.isEmpty()) { row->setVisible(false); return; }
        row->setItems(items);
        row->setVisible(true);
        for (const auto& it : items) fetchRelatedPoster(row, it);
    };
    fill(m_sagaRow, saga);
    fill(m_genreRow, genre);
    fill(m_similarRow, similar);
}

void DetailPage::fetchRelatedPoster(MediaRow* row, const JellyfinItem& it) {
    QString tag = it.primaryImageTag;
    QString imageId = it.id;
    if (tag.isEmpty() && !it.seriesPrimaryImageTag.isEmpty()) {
        tag = it.seriesPrimaryImageTag;
        imageId = it.seriesId;
    }
    if (tag.isEmpty()) return;

    const QSize iconSize = row->posterSize();
    QUrl u = m_client->imageUrl(imageId, tag, QStringLiteral("Primary"),
                                iconSize.height() * 2);
    const QString cacheKey = u.toString() + QStringLiteral("|")
                             + QString::number(iconSize.width()) + QStringLiteral("x")
                             + QString::number(iconSize.height());

    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) {
        row->setItemIcon(it.id, QIcon(cached));
        return;
    }

    QNetworkRequest req(u);
    req.setRawHeader("X-Emby-Authorization",
                     QString("MediaBrowser Token=\"%1\"")
                         .arg(m_client->accessToken()).toUtf8());
    QNetworkReply* reply = m_imgNam->get(req);
    const QString itemId = it.id;
    const QString currentDetailId = m_item.id;
    QPointer<MediaRow> rowGuard(row);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, rowGuard, itemId, currentDetailId, iconSize, cacheKey]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        if (m_item.id != currentDetailId || !rowGuard) return;
        QPixmap pm;
        if (!pm.loadFromData(reply->readAll())) return;
        const QPixmap scaled = pm.scaled(iconSize, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
        QPixmapCache::insert(cacheKey, scaled);
        rowGuard->setItemIcon(itemId, QIcon(scaled));
    });
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
