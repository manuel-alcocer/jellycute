#include "BrowseModel.h"

BrowseModel::BrowseModel(QObject* parent) : ItemListModel(parent) {}

BrowseModel::BrowseModel(JellyfinClient* client, QObject* parent)
    : ItemListModel(parent) {
    wireClient(client);
}

void BrowseModel::wireClient(JellyfinClient* client) {
    if (m_client == client) return;
    setClient(client);
    if (!client) return;

    connect(client, &JellyfinClient::resumeLoaded, this, &BrowseModel::onResume);
    connect(client, &JellyfinClient::viewsLoaded, this, &BrowseModel::onViews);
    connect(client, &JellyfinClient::latestLoaded, this, &BrowseModel::onLatest);
    connect(client, &JellyfinClient::itemsLoaded, this, &BrowseModel::onItems);
    connect(client, &JellyfinClient::networkError, this,
            [this](const QString& message) {
                if (!m_loading) return;
                setLoading(false);
                emit loadFailed(message);
            });
}

void BrowseModel::setLoading(bool v) {
    if (m_loading == v) return;
    m_loading = v;
    emit loadingChanged();
}

void BrowseModel::setSource(Source s, const QString& key) {
    m_source = s;
    m_sourceKey = key;
    setLoading(true);
}

void BrowseModel::loadResume() {
    if (!m_client) return;
    setSource(Resume);
    m_client->fetchResume();
}

void BrowseModel::loadUserViews() {
    if (!m_client) return;
    setSource(Views);
    m_client->fetchUserViews();
}

void BrowseModel::loadLatest(const QString& parentId,
                             const QString& includeItemTypes,
                             int limit) {
    if (!m_client) return;
    setSource(Latest, parentId);
    m_client->fetchLatest(parentId, includeItemTypes, limit);
}

void BrowseModel::loadChildren(const QString& parentId,
                               const QString& includeItemTypes,
                               bool recursive,
                               const QString& sortBy,
                               const QString& nameStartsWith,
                               int startIndex,
                               int limit) {
    if (!m_client) return;
    setSource(Children, parentId);
    m_client->fetchItems(parentId, includeItemTypes, recursive, sortBy,
                         nameStartsWith, startIndex, limit);
}

void BrowseModel::loadEpisodes(const QString& seriesId) {
    if (!m_client) return;
    setSource(Episodes, seriesId);
    m_client->fetchEpisodes(seriesId);
}

void BrowseModel::onResume(const QList<JellyfinItem>& items) {
    if (m_source != Resume) return;
    setItems(items);
    m_totalCount = items.size();
    emit totalCountChanged();
    setLoading(false);
}

void BrowseModel::onViews(const QList<JellyfinItem>& items) {
    if (m_source != Views) return;
    setItems(items);
    m_totalCount = items.size();
    emit totalCountChanged();
    setLoading(false);
}

void BrowseModel::onLatest(const QString& parentId,
                           const QList<JellyfinItem>& items) {
    if (m_source != Latest || parentId != m_sourceKey) return;
    setItems(items);
    m_totalCount = items.size();
    emit totalCountChanged();
    setLoading(false);
}

void BrowseModel::onItems(const QString& parentId,
                          const QList<JellyfinItem>& items,
                          int totalCount) {
    // Episodes are also delivered via itemsLoaded(seriesId, ...).
    if ((m_source != Children && m_source != Episodes) || parentId != m_sourceKey)
        return;
    setItems(items);
    m_totalCount = totalCount;
    emit totalCountChanged();
    setLoading(false);
}
