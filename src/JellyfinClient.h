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

struct JellyfinMediaStream {
    QString type;          // "Video", "Audio", "Subtitle"
    QString codec;
    QString language;      // 3-letter ISO (e.g. "spa", "eng"); may be empty
    QString displayTitle;  // server-formatted, e.g. "Spanish - DTS-HD MA - 5.1"
    QString title;
    int width = 0;
    int height = 0;
    int channels = 0;
    bool isDefault = false;
    bool isForced = false;
};

struct JellyfinMediaSource {
    QString container;     // e.g. "mkv", "mp4"
    qint64 runTimeTicks = 0;
    qint64 bitrate = 0;    // bits per second
    qint64 size = 0;       // bytes
    QList<JellyfinMediaStream> streams;
};

// Outcome of negotiating playback with the server: which URL to feed to mpv,
// the play-mode the server agreed to, and the session identifier we have to
// echo back on every subsequent /Sessions/Playing* report and on the
// transcoding teardown.
struct JellyfinPlayback {
    QUrl url;
    QString playSessionId;
    QString mediaSourceId;
    enum Method { DirectPlay, DirectStream, Transcode };
    Method method = DirectPlay;
    QString container;
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
    QList<JellyfinMediaSource> mediaSources;   // populated by fetchItemDetails
};

class JellyfinClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl server READ server WRITE setServer NOTIFY serverChanged)
    Q_PROPERTY(QString userId READ userId NOTIFY credentialsChanged)
    Q_PROPERTY(QString accessToken READ accessToken NOTIFY credentialsChanged)
    // True once a user-id + access token are present (whether from a fresh
    // authentication or from credentials restored at startup). QML uses this
    // to decide whether to push LoginPage onto the page stack.
    Q_PROPERTY(bool authenticated READ isAuthenticated NOTIFY credentialsChanged)
public:
    explicit JellyfinClient(QObject* parent = nullptr);

    Q_INVOKABLE void setServer(const QUrl& url);
    QUrl server() const { return m_server; }

    Q_INVOKABLE void setCredentials(const QString& userId, const QString& accessToken);
    QString userId() const { return m_userId; }
    QString accessToken() const { return m_token; }
    bool isAuthenticated() const {
        return !m_userId.isEmpty() && !m_token.isEmpty();
    }

    QString deviceId() const { return m_deviceId; }
    QString clientName() const { return m_clientName; }
    QString clientVersion() const { return m_clientVersion; }

    // Auth
    Q_INVOKABLE void authenticate(const QString& username, const QString& password);

    // Browsing
    Q_INVOKABLE void fetchUserViews();
    Q_INVOKABLE void fetchItems(const QString& parentId,
                                const QString& includeItemTypes = QString(),
                                bool recursive = false,
                                const QString& sortBy = QStringLiteral("SortName"),
                                const QString& nameStartsWith = QString(),
                                int startIndex = 0,
                                int limit = 0);
    Q_INVOKABLE void fetchResume();
    Q_INVOKABLE void fetchEpisodes(const QString& seriesId);
    Q_INVOKABLE void fetchLatest(const QString& parentId,
                                 const QString& includeItemTypes = QString(),
                                 int limit = 16);
    Q_INVOKABLE void fetchItemDetails(const QString& itemId);

    // Related-content lookups for the detail page.
    Q_INVOKABLE void fetchSagaSiblings(const QString& itemId);
    Q_INVOKABLE void fetchGenrePeers(const QString& itemId,
                                     const QStringList& genres,
                                     int limit = 10);
    Q_INVOKABLE void fetchSimilar(const QString& itemId, int limit = 10);

    // User-data mutations.
    Q_INVOKABLE void setFavorite(const QString& itemId, bool favorite);
    Q_INVOKABLE void setPlayed(const QString& itemId, bool played);

    // Image URL helper
    Q_INVOKABLE QUrl imageUrl(const QString& itemId, const QString& tag,
                              const QString& type = QStringLiteral("Primary"),
                              int maxHeight = 360) const;

    // Direct-play URL. Used as a fallback when PlaybackInfo isn't reachable;
    // production code should prefer resolvePlayback().
    QUrl streamUrl(const QString& itemId) const;

    // Negotiate playback with the server: posts a DeviceProfile to
    // /Items/{id}/PlaybackInfo and asynchronously emits playbackResolved()
    // (or playbackResolveFailed() on error) with the picked URL + session
    // info. startTicks lets the server pre-position transcoding.
    void resolvePlayback(const QString& itemId, qint64 startTicks = 0);

    // Tears down a server-side transcoding session. Safe to call when the
    // last play was a direct-play; the server just answers 204.
    void stopActiveEncoding(const QString& playSessionId);

    // Playback reporting. The play info from resolvePlayback() must be passed
    // through so the server attributes watch time + transcoding to the right
    // session.
    void reportPlaybackStart(const QString& itemId,
                             const JellyfinPlayback& info,
                             qint64 positionTicks);
    void reportPlaybackProgress(const QString& itemId,
                                const JellyfinPlayback& info,
                                qint64 positionTicks, bool paused);
    void reportPlaybackStopped(const QString& itemId,
                               const JellyfinPlayback& info,
                               qint64 positionTicks);

signals:
    void serverChanged();
    void credentialsChanged();
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
    void sagaSiblingsLoaded(const QString& itemId, const QList<JellyfinItem>& items);
    void genrePeersLoaded(const QString& itemId, const QList<JellyfinItem>& items);
    void similarLoaded(const QString& itemId, const QList<JellyfinItem>& items);
    void playbackResolved(const QString& itemId, const JellyfinPlayback& info);
    void playbackResolveFailed(const QString& itemId, const QString& message);
    void networkError(const QString& message);

private:
    QNetworkRequest makeRequest(const QString& path,
                                const QList<QPair<QString,QString>>& query = {}) const;
    QString authHeader(bool withToken) const;
    QList<JellyfinItem> parseItems(const QJsonArray& arr) const;
    void handleItemsReply(QNetworkReply* reply, const QString& parentId);
    QUrl directPlayUrl(const QString& itemId, const QString& mediaSourceId,
                       const QString& playSessionId) const;
    QUrl absoluteFromRelative(const QString& s) const;

    QNetworkAccessManager m_nam;
    QUrl m_server;
    QString m_userId;
    QString m_token;
    QString m_deviceId;
    QString m_clientName = QStringLiteral("jellycute");
    QString m_clientVersion = QStringLiteral("0.1");
};
