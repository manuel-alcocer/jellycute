#pragma once

#include "ItemListModel.h"

#include <QString>

// Facade model that connects to a JellyfinClient and exposes high-level
// loadX() entry points to QML. Each load picks one of the client's async
// signals and routes the matching list back into the model. Only the most
// recently requested source is honoured: switching sources clears the
// previous selection's pending result so stale items don't bleed in.
class BrowseModel : public ItemListModel {
    Q_OBJECT
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
public:
    explicit BrowseModel(QObject* parent = nullptr);
    BrowseModel(JellyfinClient* client, QObject* parent = nullptr);

    void wireClient(JellyfinClient* client);

    int totalCount() const { return m_totalCount; }
    bool loading() const { return m_loading; }

    Q_INVOKABLE void loadResume();
    Q_INVOKABLE void loadUserViews();
    Q_INVOKABLE void loadLatest(const QString& parentId,
                                const QString& includeItemTypes = QString(),
                                int limit = 16);
    Q_INVOKABLE void loadChildren(const QString& parentId,
                                  const QString& includeItemTypes = QString(),
                                  bool recursive = false,
                                  const QString& sortBy = QStringLiteral("SortName"),
                                  const QString& nameStartsWith = QString(),
                                  int startIndex = 0,
                                  int limit = 0);
    Q_INVOKABLE void loadEpisodes(const QString& seriesId);

signals:
    void totalCountChanged();
    void loadingChanged();
    void loadFailed(const QString& message);

private:
    enum Source { None, Resume, Views, Latest, Children, Episodes };

    void onResume(const QList<JellyfinItem>& items);
    void onViews(const QList<JellyfinItem>& items);
    void onLatest(const QString& parentId, const QList<JellyfinItem>& items);
    void onItems(const QString& parentId,
                 const QList<JellyfinItem>& items, int totalCount);

    void setLoading(bool v);
    void setSource(Source s, const QString& key = QString());

    Source m_source = None;
    QString m_sourceKey;     // parentId / seriesId — used to filter signals.
    int m_totalCount = 0;
    bool m_loading = false;
};
