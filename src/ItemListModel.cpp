#include "ItemListModel.h"

namespace {
constexpr qint64 TICKS_PER_SECOND = 10'000'000LL;
}

ItemListModel::ItemListModel(QObject* parent) : QAbstractListModel(parent) {}

void ItemListModel::setClient(JellyfinClient* client) {
    if (m_client == client) return;
    m_client = client;
    // Image-url roles depend on the client; refresh views if we already have items.
    if (!m_items.isEmpty()) {
        emit dataChanged(index(0), index(m_items.size() - 1),
                         {PosterUrlRole, BackdropUrlRole});
    }
}

int ItemListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_items.size();
}

QHash<int, QByteArray> ItemListModel::roleNames() const {
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {TypeRole, "type"},
        {CollectionTypeRole, "collectionType"},
        {SeriesNameRole, "seriesName"},
        {ProductionYearRole, "productionYear"},
        {IndexNumberRole, "indexNumber"},
        {ParentIndexNumberRole, "parentIndexNumber"},
        {RunTimeSecondsRole, "runTimeSeconds"},
        {ResumeSecondsRole, "resumeSeconds"},
        {IsFolderRole, "isFolder"},
        {PlayableRole, "playable"},
        {IsFavoriteRole, "isFavorite"},
        {IsPlayedRole, "isPlayed"},
        {OverviewRole, "overview"},
        {OfficialRatingRole, "officialRating"},
        {CommunityRatingRole, "communityRating"},
        {PrimaryImageTagRole, "primaryImageTag"},
        {BackdropImageTagRole, "backdropImageTag"},
        {SeriesIdRole, "seriesId"},
        {SeriesPrimaryImageTagRole, "seriesPrimaryImageTag"},
        {PosterUrlRole, "posterUrl"},
        {BackdropUrlRole, "backdropUrl"},
    };
}

QVariant ItemListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};
    const JellyfinItem& it = m_items.at(index.row());
    switch (role) {
        case IdRole: return it.id;
        case NameRole: return it.name;
        case TypeRole: return it.type;
        case CollectionTypeRole: return it.collectionType;
        case SeriesNameRole: return it.seriesName;
        case ProductionYearRole: return it.productionYear;
        case IndexNumberRole: return it.indexNumber;
        case ParentIndexNumberRole: return it.parentIndexNumber;
        case RunTimeSecondsRole:
            return static_cast<double>(it.runTimeTicks) / TICKS_PER_SECOND;
        case ResumeSecondsRole:
            return static_cast<double>(it.resumePositionTicks) / TICKS_PER_SECOND;
        case IsFolderRole: return it.isFolder;
        case PlayableRole: return it.playable();
        case IsFavoriteRole: return it.isFavorite;
        case IsPlayedRole: return it.isPlayed;
        case OverviewRole: return it.overview;
        case OfficialRatingRole: return it.officialRating;
        case CommunityRatingRole: return it.communityRating;
        case PrimaryImageTagRole: return it.primaryImageTag;
        case BackdropImageTagRole: return it.backdropImageTag;
        case SeriesIdRole: return it.seriesId;
        case SeriesPrimaryImageTagRole: return it.seriesPrimaryImageTag;
        case PosterUrlRole:
            if (!m_client) return QUrl();
            if (!it.primaryImageTag.isEmpty())
                return m_client->imageUrl(it.id, it.primaryImageTag,
                                          QStringLiteral("Primary"), 360);
            // Episodes typically lack their own primary image; fall back to
            // the parent series poster so the row isn't empty.
            if (!it.seriesId.isEmpty() && !it.seriesPrimaryImageTag.isEmpty())
                return m_client->imageUrl(it.seriesId, it.seriesPrimaryImageTag,
                                          QStringLiteral("Primary"), 360);
            return QUrl();
        case BackdropUrlRole:
            if (!m_client || it.backdropImageTag.isEmpty()) return QUrl();
            return m_client->imageUrl(it.id, it.backdropImageTag,
                                      QStringLiteral("Backdrop"), 720);
        default: return {};
    }
}

void ItemListModel::setItems(QList<JellyfinItem> items) {
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    emit countChanged();
}

void ItemListModel::appendItems(const QList<JellyfinItem>& items) {
    if (items.isEmpty()) return;
    const int first = m_items.size();
    beginInsertRows(QModelIndex(), first, first + items.size() - 1);
    m_items.append(items);
    endInsertRows();
    emit countChanged();
}

void ItemListModel::clear() {
    if (m_items.isEmpty()) return;
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
}

QVariantMap ItemListModel::get(int index) const {
    QVariantMap m;
    if (index < 0 || index >= m_items.size()) return m;
    const auto roles = roleNames();
    const QModelIndex idx = this->index(index);
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        m.insert(QString::fromUtf8(it.value()), data(idx, it.key()));
    return m;
}
