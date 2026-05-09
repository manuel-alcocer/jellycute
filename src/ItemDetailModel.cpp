#include "ItemDetailModel.h"

namespace {
constexpr qint64 TICKS_PER_SECOND = 10'000'000LL;
}

ItemDetailModel::ItemDetailModel(QObject* parent) : QObject(parent) {}

void ItemDetailModel::setClient(JellyfinClient* c) {
    if (m_client == c) return;
    m_client = c;
    emit clientChanged();
    if (!c) return;
    connect(c, &JellyfinClient::itemDetailsLoaded, this,
            &ItemDetailModel::onItemDetailsLoaded);
    connect(c, &JellyfinClient::favoriteToggled, this,
            &ItemDetailModel::onFavoriteToggled);
    connect(c, &JellyfinClient::playedToggled, this,
            &ItemDetailModel::onPlayedToggled);
    connect(c, &JellyfinClient::networkError, this,
            [this](const QString& message) {
                if (!m_loading) return;
                setLoading(false);
                emit loadFailed(message);
            });
}

qint64 ItemDetailModel::runTimeSeconds() const {
    return m_item.runTimeTicks / TICKS_PER_SECOND;
}

qint64 ItemDetailModel::resumeSeconds() const {
    return m_item.resumePositionTicks / TICKS_PER_SECOND;
}

QUrl ItemDetailModel::posterUrl() const {
    if (!m_client) return {};
    if (!m_item.primaryImageTag.isEmpty())
        return m_client->imageUrl(m_item.id, m_item.primaryImageTag,
                                  QStringLiteral("Primary"), 540);
    if (!m_item.seriesId.isEmpty() && !m_item.seriesPrimaryImageTag.isEmpty())
        return m_client->imageUrl(m_item.seriesId, m_item.seriesPrimaryImageTag,
                                  QStringLiteral("Primary"), 540);
    return {};
}

QUrl ItemDetailModel::backdropUrl() const {
    if (!m_client || m_item.backdropImageTag.isEmpty()) return {};
    return m_client->imageUrl(m_item.id, m_item.backdropImageTag,
                              QStringLiteral("Backdrop"), 1080);
}

void ItemDetailModel::prefill(const QVariantMap& m) {
    if (m.contains(QStringLiteral("id")))
        m_item.id = m.value(QStringLiteral("id")).toString();
    if (m.contains(QStringLiteral("name")))
        m_item.name = m.value(QStringLiteral("name")).toString();
    if (m.contains(QStringLiteral("type")))
        m_item.type = m.value(QStringLiteral("type")).toString();
    if (m.contains(QStringLiteral("collectionType")))
        m_item.collectionType = m.value(QStringLiteral("collectionType")).toString();
    if (m.contains(QStringLiteral("seriesName")))
        m_item.seriesName = m.value(QStringLiteral("seriesName")).toString();
    if (m.contains(QStringLiteral("seriesId")))
        m_item.seriesId = m.value(QStringLiteral("seriesId")).toString();
    if (m.contains(QStringLiteral("productionYear")))
        m_item.productionYear = m.value(QStringLiteral("productionYear")).toInt();
    if (m.contains(QStringLiteral("indexNumber")))
        m_item.indexNumber = m.value(QStringLiteral("indexNumber")).toInt();
    if (m.contains(QStringLiteral("parentIndexNumber")))
        m_item.parentIndexNumber = m.value(QStringLiteral("parentIndexNumber")).toInt();
    if (m.contains(QStringLiteral("runTimeSeconds")))
        m_item.runTimeTicks = qint64(m.value(QStringLiteral("runTimeSeconds")).toDouble())
                              * TICKS_PER_SECOND;
    if (m.contains(QStringLiteral("resumeSeconds")))
        m_item.resumePositionTicks =
            qint64(m.value(QStringLiteral("resumeSeconds")).toDouble())
            * TICKS_PER_SECOND;
    if (m.contains(QStringLiteral("isFolder")))
        m_item.isFolder = m.value(QStringLiteral("isFolder")).toBool();
    if (m.contains(QStringLiteral("isFavorite")))
        m_item.isFavorite = m.value(QStringLiteral("isFavorite")).toBool();
    if (m.contains(QStringLiteral("isPlayed")))
        m_item.isPlayed = m.value(QStringLiteral("isPlayed")).toBool();
    if (m.contains(QStringLiteral("primaryImageTag")))
        m_item.primaryImageTag = m.value(QStringLiteral("primaryImageTag")).toString();
    if (m.contains(QStringLiteral("backdropImageTag")))
        m_item.backdropImageTag = m.value(QStringLiteral("backdropImageTag")).toString();
    if (m.contains(QStringLiteral("seriesPrimaryImageTag")))
        m_item.seriesPrimaryImageTag =
            m.value(QStringLiteral("seriesPrimaryImageTag")).toString();
    emit itemChanged();
}

void ItemDetailModel::load(const QString& itemId) {
    if (!m_client || itemId.isEmpty()) return;
    if (m_item.id != itemId) {
        // Different item — clear stale fields that won't be in the prefill.
        m_item = JellyfinItem{};
        m_item.id = itemId;
        emit itemChanged();
    }
    setLoading(true);
    m_client->fetchItemDetails(itemId);
}

void ItemDetailModel::toggleFavorite() {
    if (!m_client || m_item.id.isEmpty()) return;
    m_client->setFavorite(m_item.id, !m_item.isFavorite);
}

void ItemDetailModel::togglePlayed() {
    if (!m_client || m_item.id.isEmpty()) return;
    m_client->setPlayed(m_item.id, !m_item.isPlayed);
}

void ItemDetailModel::onItemDetailsLoaded(const JellyfinItem& item) {
    if (item.id != m_item.id) return;
    m_item = item;
    setLoading(false);
    emit itemChanged();
}

void ItemDetailModel::onFavoriteToggled(const QString& itemId, bool isFavorite) {
    if (itemId != m_item.id || m_item.isFavorite == isFavorite) return;
    m_item.isFavorite = isFavorite;
    emit itemChanged();
}

void ItemDetailModel::onPlayedToggled(const QString& itemId, bool isPlayed) {
    if (itemId != m_item.id || m_item.isPlayed == isPlayed) return;
    m_item.isPlayed = isPlayed;
    emit itemChanged();
}

void ItemDetailModel::setLoading(bool v) {
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
}
