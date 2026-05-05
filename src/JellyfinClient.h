#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

struct JellyfinPerson {
    QString id;
    QString name;
    QString role;
    QString type;       // "Actor", "Director", "Writer", "Producer", ...
    QString primaryImageTag;
};

struct JellyfinItem {
    QString id;
    QString name;
    QString type;          // "Movie", "Episode", "Series", "Season", "CollectionFolder", ...
    QString collectionType; // For CollectionFolder: "movies", "tvshows", "music", ...
    QString seriesName;
    int productionYear = 0;
    qint64 runTimeTicks = 0;
    qint64 resumePositionTicks = 0;
    bool isFolder = false;
    QString primaryImageTag;
    QString seriesId;
    QString seriesPrimaryImageTag;
    int indexNumber = 0;
    int parentIndexNumber = 0;

    // Filled by fetchItemDetails.
    QString overview;
    QString officialRating;
    double communityRating = 0.0;
    bool isFavorite = false;
    bool isPlayed = false;
    bool playable() const {
        return type == QStringLiteral("Movie") || type == QStringLiteral("Episode")
               || type == QStringLiteral("Video");
    }
    QStringList genres;
    QStringList studios;
    QStringList tags;
    QList<JellyfinPerson> people;
    QString backdropImageTag;
};

class JellyfinClient : public QObject {
    Q_OBJECT
public:
    explicit JellyfinClient(QObject* parent = nullptr);

    void setServer(const QUrl& url);
    QUrl server() const { return m_server; }

    void setCredentials(const QString& userId, const QString& accessToken);
    QString userId() const { return m_userId; }
    QString accessToken() const { return m_token; }

    QString deviceId() const { return m_deviceId; }
    QString clientName() const { return m_clientName; }
    QString clientVersion() const { return m_clientVersion; }

    // Auth
    void authenticate(const QString& username, const QString& password);

    // Browsing
    void fetchUserViews();
    void fetchItems(const QString& parentId,
                    const QString& includeItemTypes = QString(),
                    bool recursive = false,
                    const QString& sortBy = QStringLiteral("SortName"),
                    const QString& nameStartsWith = QString(),
                    int startIndex = 0,
                    int limit = 0);
    void fetchResume();
    void fetchEpisodes(const QString& seriesId);
    void fetchLatest(const QString& parentId, const QString& includeItemTypes = QString(),
                     int limit = 16);
    void fetchItemDetails(const QString& itemId);

    // User-data mutations.
    void setFavorite(const QString& itemId, bool favorite);
    void setPlayed(const QString& itemId, bool played);

    // Image URL helper
    QUrl imageUrl(const QString& itemId, const QString& tag,
                  const QString& type = QStringLiteral("Primary"),
                  int maxHeight = 360) const;

    // Stream URL for libmpv (direct, server transcodes if needed)
    QUrl streamUrl(const QString& itemId) const;

    // Playback reporting
    void reportPlaybackStart(const QString& itemId, qint64 positionTicks);
    void reportPlaybackProgress(const QString& itemId, qint64 positionTicks, bool paused);
    void reportPlaybackStopped(const QString& itemId, qint64 positionTicks);

signals:
    void authenticated();
    void authenticationFailed(const QString& message);
    void viewsLoaded(const QList<JellyfinItem>& items);
    void itemsLoaded(const QString& parentId,
                     const QList<JellyfinItem>& items,
                     int totalCount);
    void resumeLoaded(const QList<JellyfinItem>& items);
    void latestLoaded(const QString& parentId, const QList<JellyfinItem>& items);
    void itemDetailsLoaded(const JellyfinItem& item);
    void favoriteToggled(const QString& itemId, bool isFavorite);
    void playedToggled(const QString& itemId, bool isPlayed);
    void networkError(const QString& message);

private:
    QNetworkRequest makeRequest(const QString& path,
                                const QList<QPair<QString,QString>>& query = {}) const;
    QString authHeader(bool withToken) const;
    QList<JellyfinItem> parseItems(const QJsonArray& arr) const;
    void handleItemsReply(QNetworkReply* reply, const QString& parentId);

    QNetworkAccessManager m_nam;
    QUrl m_server;
    QString m_userId;
    QString m_token;
    QString m_deviceId;
    QString m_clientName = QStringLiteral("jellycute");
    QString m_clientVersion = QStringLiteral("0.1");
};
