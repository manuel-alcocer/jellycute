#include "JellyfinClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSysInfo>
#include <QUrlQuery>
#include <QUuid>
#include <QSettings>

#include <memory>

JellyfinClient::JellyfinClient(QObject* parent) : QObject(parent) {
    QSettings s;
    m_deviceId = s.value(QStringLiteral("deviceId")).toString();
    if (m_deviceId.isEmpty()) {
        m_deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.setValue(QStringLiteral("deviceId"), m_deviceId);
    }
}

void JellyfinClient::setServer(const QUrl& url) { m_server = url; }

void JellyfinClient::setCredentials(const QString& userId, const QString& accessToken) {
    m_userId = userId;
    m_token = accessToken;
}

QString JellyfinClient::authHeader(bool withToken) const {
    const QString deviceName = QSysInfo::machineHostName();
    QString h = QString("MediaBrowser Client=\"%1\", Device=\"%2\", DeviceId=\"%3\", Version=\"%4\"")
                    .arg(m_clientName, deviceName, m_deviceId, m_clientVersion);
    if (withToken && !m_token.isEmpty())
        h += QString(", Token=\"%1\"").arg(m_token);
    return h;
}

QNetworkRequest JellyfinClient::makeRequest(const QString& path,
                                            const QList<QPair<QString,QString>>& query) const {
    QUrl url = m_server;
    QString basePath = url.path();
    if (basePath.endsWith('/')) basePath.chop(1);
    url.setPath(basePath + path);
    if (!query.isEmpty()) {
        QUrlQuery q;
        for (const auto& kv : query) q.addQueryItem(kv.first, kv.second);
        url.setQuery(q);
    }
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-Emby-Authorization", authHeader(true).toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

void JellyfinClient::authenticate(const QString& username, const QString& password) {
    QNetworkRequest req(QUrl(m_server.toString() + "/Users/AuthenticateByName"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("X-Emby-Authorization", authHeader(false).toUtf8());

    QJsonObject body;
    body["Username"] = username;
    body["Pw"] = password;
    QNetworkReply* reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit authenticationFailed(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto obj = doc.object();
        m_token = obj.value("AccessToken").toString();
        m_userId = obj.value("User").toObject().value("Id").toString();
        if (m_token.isEmpty() || m_userId.isEmpty()) {
            emit authenticationFailed(tr("Respuesta de autenticación inválida"));
            return;
        }
        emit authenticated();
    });
}

static JellyfinItem parseSingleItem(const QJsonObject& o) {
    JellyfinItem it;
    it.id = o.value("Id").toString();
    it.name = o.value("Name").toString();
    it.type = o.value("Type").toString();
    it.collectionType = o.value("CollectionType").toString();
    it.seriesName = o.value("SeriesName").toString();
    it.seriesId = o.value("SeriesId").toString();
    it.productionYear = o.value("ProductionYear").toInt();
    it.runTimeTicks = (qint64) o.value("RunTimeTicks").toDouble();
    it.isFolder = o.value("IsFolder").toBool();
    it.indexNumber = o.value("IndexNumber").toInt();
    it.parentIndexNumber = o.value("ParentIndexNumber").toInt();
    const auto userData = o.value("UserData").toObject();
    it.resumePositionTicks = (qint64) userData.value("PlaybackPositionTicks").toDouble();
    it.isFavorite = userData.value("IsFavorite").toBool();
    it.isPlayed = userData.value("Played").toBool();
    const auto tags = o.value("ImageTags").toObject();
    it.primaryImageTag = tags.value("Primary").toString();
    it.seriesPrimaryImageTag = o.value("SeriesPrimaryImageTag").toString();

    // Detail-only fields (populated when Fields= asks for them).
    it.overview = o.value("Overview").toString();
    it.officialRating = o.value("OfficialRating").toString();
    it.communityRating = o.value("CommunityRating").toDouble();
    for (const auto& g : o.value("Genres").toArray()) it.genres << g.toString();
    for (const auto& s : o.value("Studios").toArray())
        it.studios << s.toObject().value("Name").toString();
    for (const auto& t : o.value("Tags").toArray()) it.tags << t.toString();
    for (const auto& p : o.value("People").toArray()) {
        const auto po = p.toObject();
        JellyfinPerson person;
        person.id = po.value("Id").toString();
        person.name = po.value("Name").toString();
        person.role = po.value("Role").toString();
        person.type = po.value("Type").toString();
        person.primaryImageTag = po.value("PrimaryImageTag").toString();
        it.people << person;
    }
    const auto backdrops = o.value("BackdropImageTags").toArray();
    if (!backdrops.isEmpty()) it.backdropImageTag = backdrops.first().toString();

    for (const auto& msv : o.value("MediaSources").toArray()) {
        const auto mso = msv.toObject();
        JellyfinMediaSource src;
        src.container = mso.value("Container").toString();
        src.runTimeTicks = (qint64) mso.value("RunTimeTicks").toDouble();
        src.bitrate = (qint64) mso.value("Bitrate").toDouble();
        src.size = (qint64) mso.value("Size").toDouble();
        for (const auto& sv : mso.value("MediaStreams").toArray()) {
            const auto so = sv.toObject();
            JellyfinMediaStream st;
            st.type = so.value("Type").toString();
            st.codec = so.value("Codec").toString();
            st.language = so.value("Language").toString();
            st.displayTitle = so.value("DisplayTitle").toString();
            st.title = so.value("Title").toString();
            st.width = so.value("Width").toInt();
            st.height = so.value("Height").toInt();
            st.channels = so.value("Channels").toInt();
            st.isDefault = so.value("IsDefault").toBool();
            st.isForced = so.value("IsForced").toBool();
            src.streams << st;
        }
        it.mediaSources << src;
    }
    return it;
}

QList<JellyfinItem> JellyfinClient::parseItems(const QJsonArray& arr) const {
    QList<JellyfinItem> out;
    out.reserve(arr.size());
    for (const auto& v : arr) out.push_back(parseSingleItem(v.toObject()));
    return out;
}

void JellyfinClient::handleItemsReply(QNetworkReply* reply, const QString& parentId) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit networkError(reply->errorString());
        return;
    }
    const auto doc = QJsonDocument::fromJson(reply->readAll());
    const auto obj = doc.object();
    const auto items = parseItems(obj.value("Items").toArray());
    const int total = obj.value("TotalRecordCount").toInt(items.size());
    emit itemsLoaded(parentId, items, total);
}

void JellyfinClient::fetchUserViews() {
    QNetworkReply* reply = m_nam.get(makeRequest("/Users/" + m_userId + "/Views"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit viewsLoaded(parseItems(doc.object().value("Items").toArray()));
    });
}

void JellyfinClient::fetchItems(const QString& parentId, const QString& includeItemTypes,
                                bool recursive, const QString& sortBy,
                                const QString& nameStartsWith,
                                int startIndex, int limit) {
    QList<QPair<QString,QString>> q = {
        {"ParentId", parentId},
        {"Fields", "PrimaryImageAspectRatio,UserData,SeriesPrimaryImageTag"},
        {"SortBy", sortBy},
        {"SortOrder", "Ascending"},
        {"ImageTypeLimit", "1"},
    };
    if (!includeItemTypes.isEmpty())
        q.append(QPair<QString,QString>{"IncludeItemTypes", includeItemTypes});
    if (recursive)
        q.append(QPair<QString,QString>{"Recursive", "true"});
    if (!nameStartsWith.isEmpty())
        q.append(QPair<QString,QString>{"NameStartsWith", nameStartsWith});
    if (startIndex > 0)
        q.append(QPair<QString,QString>{"StartIndex", QString::number(startIndex)});
    if (limit > 0)
        q.append(QPair<QString,QString>{"Limit", QString::number(limit)});
    QNetworkReply* reply = m_nam.get(makeRequest("/Users/" + m_userId + "/Items", q));
    connect(reply, &QNetworkReply::finished, this, [this, reply, parentId]() {
        handleItemsReply(reply, parentId);
    });
}

void JellyfinClient::fetchResume() {
    QList<QPair<QString,QString>> q = {
        {"Limit", "24"},
        {"Fields", "PrimaryImageAspectRatio,UserData,SeriesPrimaryImageTag"},
        {"MediaTypes", "Video"},
        {"ImageTypeLimit", "1"},
    };
    QNetworkReply* reply = m_nam.get(makeRequest("/Users/" + m_userId + "/Items/Resume", q));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit resumeLoaded(parseItems(doc.object().value("Items").toArray()));
    });
}

void JellyfinClient::fetchLatest(const QString& parentId,
                                 const QString& includeItemTypes, int limit) {
    QList<QPair<QString,QString>> q = {
        {"ParentId", parentId},
        {"Limit", QString::number(limit)},
        {"Fields", "PrimaryImageAspectRatio,UserData,SeriesPrimaryImageTag"},
        {"ImageTypeLimit", "1"},
    };
    if (!includeItemTypes.isEmpty())
        q.append(QPair<QString,QString>{"IncludeItemTypes", includeItemTypes});
    QNetworkReply* reply = m_nam.get(makeRequest("/Users/" + m_userId + "/Items/Latest", q));
    connect(reply, &QNetworkReply::finished, this, [this, reply, parentId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }
        // /Items/Latest returns a JSON array directly, not wrapped in {Items: [...]}.
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit latestLoaded(parentId, parseItems(doc.array()));
    });
}

void JellyfinClient::fetchEpisodes(const QString& seriesId) {
    QList<QPair<QString,QString>> q = {
        {"UserId", m_userId},
        {"Fields", "PrimaryImageAspectRatio,UserData,SeriesPrimaryImageTag,Overview"},
        {"SortBy", "ParentIndexNumber,IndexNumber"},
        {"SortOrder", "Ascending"},
        {"ImageTypeLimit", "1"},
    };
    QNetworkReply* reply = m_nam.get(makeRequest("/Shows/" + seriesId + "/Episodes", q));
    connect(reply, &QNetworkReply::finished, this, [this, reply, seriesId]() {
        handleItemsReply(reply, seriesId);
    });
}

QUrl JellyfinClient::imageUrl(const QString& itemId, const QString& tag,
                              const QString& type, int maxHeight) const {
    QUrl u = m_server;
    QString base = u.path();
    if (base.endsWith('/')) base.chop(1);
    u.setPath(base + "/Items/" + itemId + "/Images/" + type);
    QUrlQuery q;
    if (!tag.isEmpty()) q.addQueryItem("tag", tag);
    q.addQueryItem("maxHeight", QString::number(maxHeight));
    q.addQueryItem("quality", "90");
    u.setQuery(q);
    return u;
}

QUrl JellyfinClient::streamUrl(const QString& itemId) const {
    return directPlayUrl(itemId, QString(), QString());
}

QUrl JellyfinClient::directPlayUrl(const QString& itemId,
                                   const QString& mediaSourceId,
                                   const QString& playSessionId) const {
    QUrl u = m_server;
    QString base = u.path();
    if (base.endsWith('/')) base.chop(1);
    u.setPath(base + "/Videos/" + itemId + "/stream");
    QUrlQuery q;
    q.addQueryItem("Static", "true");
    if (!mediaSourceId.isEmpty()) q.addQueryItem("MediaSourceId", mediaSourceId);
    q.addQueryItem("api_key", m_token);
    q.addQueryItem("DeviceId", m_deviceId);
    if (!playSessionId.isEmpty()) q.addQueryItem("PlaySessionId", playSessionId);
    u.setQuery(q);
    return u;
}

QUrl JellyfinClient::absoluteFromRelative(const QString& s) const {
    // Some Jellyfin builds return absolute URLs in TranscodingUrl /
    // DirectStreamUrl, others return server-relative paths. Anchor to
    // m_server when relative; preserve any embedded query (api_key,
    // PlaySessionId, codec params, …) verbatim.
    if (s.startsWith("http://") || s.startsWith("https://")) return QUrl(s);
    QUrl u = m_server;
    QString basePath = u.path();
    if (basePath.endsWith('/')) basePath.chop(1);
    const int qIdx = s.indexOf('?');
    const QString pathPart = qIdx >= 0 ? s.left(qIdx) : s;
    const QString queryPart = qIdx >= 0 ? s.mid(qIdx + 1) : QString();
    u.setPath(basePath + (pathPart.startsWith('/') ? pathPart
                                                    : QStringLiteral("/") + pathPart));
    if (!queryPart.isEmpty()) u.setQuery(queryPart);
    return u;
}

namespace {
// Permissive device profile: we let mpv attempt direct play for everything
// reasonable, and only fall back to a single H.264/AAC HLS transcode when
// the server insists.
QJsonObject buildMpvDeviceProfile() {
    auto videoDirect = [](const QString& container) {
        QJsonObject o;
        o["Container"] = container;
        o["Type"] = "Video";
        return o;
    };
    auto audioDirect = [](const QString& container) {
        QJsonObject o;
        o["Container"] = container;
        o["Type"] = "Audio";
        return o;
    };
    QJsonArray directPlay;
    directPlay << videoDirect("mp4,m4v,mov,3gp,3g2");
    directPlay << videoDirect("mkv,webm");
    directPlay << videoDirect("ts,mpegts,m2ts,mts");
    directPlay << videoDirect("avi,asf,wmv,flv,ogv,mxf,mpg,mpeg");
    directPlay << audioDirect("mp3,m4a,mp4,aac,wav,flac,ogg,oga,opus,wma,ape,aiff");

    QJsonArray transcoding;
    {
        QJsonObject o;
        o["Container"] = "ts";
        o["Type"] = "Video";
        o["Protocol"] = "hls";
        o["Context"] = "Streaming";
        o["VideoCodec"] = "h264";
        o["AudioCodec"] = "aac,mp3,ac3,eac3";
        o["MaxAudioChannels"] = "6";
        o["BreakOnNonKeyFrames"] = true;
        transcoding << o;
    }
    {
        QJsonObject o;
        o["Container"] = "mp3";
        o["Type"] = "Audio";
        o["Protocol"] = "http";
        o["AudioCodec"] = "mp3";
        o["MaxAudioChannels"] = "2";
        transcoding << o;
    }

    auto sub = [](const QString& fmt, const QString& method) {
        QJsonObject o;
        o["Format"] = fmt;
        o["Method"] = method;
        return o;
    };
    QJsonArray subtitles;
    for (const QString& fmt : {"srt", "ass", "ssa", "vtt", "subrip", "smi",
                               "sub", "idx"})
        subtitles << sub(fmt, "External");
    for (const QString& fmt : {"pgssub", "dvdsub", "dvbsub"})
        subtitles << sub(fmt, "Embed");

    QJsonObject profile;
    profile["MaxStreamingBitrate"] = 200000000;
    profile["MaxStaticBitrate"] = 200000000;
    profile["MusicStreamingTranscodingBitrate"] = 320000;
    profile["DirectPlayProfiles"] = directPlay;
    profile["TranscodingProfiles"] = transcoding;
    profile["ContainerProfiles"] = QJsonArray();
    profile["CodecProfiles"] = QJsonArray();
    profile["SubtitleProfiles"] = subtitles;
    profile["ResponseProfiles"] = QJsonArray();
    return profile;
}

QString playMethodString(JellyfinPlayback::Method m) {
    switch (m) {
        case JellyfinPlayback::DirectPlay:   return QStringLiteral("DirectPlay");
        case JellyfinPlayback::DirectStream: return QStringLiteral("DirectStream");
        case JellyfinPlayback::Transcode:    return QStringLiteral("Transcode");
    }
    return QStringLiteral("DirectPlay");
}
}

void JellyfinClient::resolvePlayback(const QString& itemId, qint64 startTicks) {
    QJsonObject body;
    body["DeviceProfile"] = buildMpvDeviceProfile();
    body["UserId"] = m_userId;
    body["MaxStreamingBitrate"] = 200000000;
    body["StartTimeTicks"] = (double) startTicks;
    body["AutoOpenLiveStream"] = true;
    body["EnableDirectPlay"] = true;
    body["EnableDirectStream"] = true;
    body["EnableTranscoding"] = true;
    body["AllowVideoStreamCopy"] = true;
    body["AllowAudioStreamCopy"] = true;

    QNetworkRequest req = makeRequest(
        QStringLiteral("/Items/") + itemId + QStringLiteral("/PlaybackInfo"));
    QNetworkReply* reply = m_nam.post(req,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, itemId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit playbackResolveFailed(itemId, reply->errorString());
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit playbackResolveFailed(itemId, tr("Respuesta no JSON"));
            return;
        }
        const QJsonObject obj = doc.object();
        const QJsonArray sources = obj.value(QStringLiteral("MediaSources"))
                                       .toArray();
        if (sources.isEmpty()) {
            emit playbackResolveFailed(itemId, tr("Sin MediaSources"));
            return;
        }
        const QJsonObject src = sources.first().toObject();
        JellyfinPlayback p;
        p.playSessionId = obj.value(QStringLiteral("PlaySessionId")).toString();
        p.mediaSourceId = src.value(QStringLiteral("Id")).toString();
        p.container = src.value(QStringLiteral("Container")).toString();

        const bool directPlay = src.value(QStringLiteral("SupportsDirectPlay"))
                                    .toBool();
        const bool directStream = src.value(QStringLiteral("SupportsDirectStream"))
                                      .toBool();
        const bool transcode = src.value(QStringLiteral("SupportsTranscoding"))
                                   .toBool();
        const QString transcodeUrl = src.value(QStringLiteral("TranscodingUrl"))
                                        .toString();
        const QString directStreamUrl = src.value(QStringLiteral("DirectStreamUrl"))
                                            .toString();

        if (directPlay) {
            p.method = JellyfinPlayback::DirectPlay;
            p.url = directPlayUrl(itemId, p.mediaSourceId, p.playSessionId);
        } else if (directStream && !directStreamUrl.isEmpty()) {
            p.method = JellyfinPlayback::DirectStream;
            p.url = absoluteFromRelative(directStreamUrl);
        } else if (transcode && !transcodeUrl.isEmpty()) {
            p.method = JellyfinPlayback::Transcode;
            p.url = absoluteFromRelative(transcodeUrl);
        } else {
            // Server gave us nothing usable — try direct play anyway. If the
            // file actually plays, great; otherwise mpv surfaces the error.
            p.method = JellyfinPlayback::DirectPlay;
            p.url = directPlayUrl(itemId, p.mediaSourceId, p.playSessionId);
        }
        emit playbackResolved(itemId, p);
    });
}

void JellyfinClient::stopActiveEncoding(const QString& playSessionId) {
    if (playSessionId.isEmpty()) return;
    QUrl u = m_server;
    QString base = u.path();
    if (base.endsWith('/')) base.chop(1);
    u.setPath(base + QStringLiteral("/Videos/ActiveEncodings"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("DeviceId"), m_deviceId);
    q.addQueryItem(QStringLiteral("PlaySessionId"), playSessionId);
    u.setQuery(q);
    QNetworkRequest req(u);
    req.setRawHeader("X-Emby-Authorization", authHeader(true).toUtf8());
    QNetworkReply* reply = m_nam.deleteResource(req);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void JellyfinClient::fetchItemDetails(const QString& itemId) {
    QList<QPair<QString,QString>> q = {
        {"Fields", "Overview,People,Genres,Studios,Tags,OfficialRating,"
                   "CommunityRating,UserData,BackdropImageTags,"
                   "SeriesPrimaryImageTag,MediaSources,MediaStreams"},
    };
    QNetworkReply* reply = m_nam.get(makeRequest("/Users/" + m_userId + "/Items/" + itemId, q));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit itemDetailsLoaded(parseSingleItem(doc.object()));
    });
}

void JellyfinClient::fetchSagaSiblings(const QString& itemId) {
    // Jellyfin doesn't expose a direct "find BoxSets that contain this item"
    // filter, so we list every BoxSet and probe each one's children in
    // parallel. The first BoxSet whose children contain `itemId` wins and we
    // emit its siblings; if none match we emit an empty list. QNAM caps
    // concurrent connections automatically, so even libraries with hundreds
    // of BoxSets stay polite.
    QList<QPair<QString,QString>> q = {
        {"Recursive", "true"},
        {"IncludeItemTypes", "BoxSet"},
        {"Limit", "500"},
        {"EnableImages", "false"},
    };
    QNetworkReply* reply = m_nam.get(makeRequest("/Users/" + m_userId + "/Items", q));
    connect(reply, &QNetworkReply::finished, this, [this, reply, itemId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit sagaSiblingsLoaded(itemId, {});
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto boxSets = doc.object().value("Items").toArray();
        if (boxSets.isEmpty()) {
            emit sagaSiblingsLoaded(itemId, {});
            return;
        }
        struct State { int pending; bool emitted; };
        auto state = std::make_shared<State>(State{(int)boxSets.size(), false});
        for (const auto& bs : boxSets) {
            const QString boxSetId = bs.toObject().value("Id").toString();
            if (boxSetId.isEmpty()) {
                if (--state->pending == 0 && !state->emitted)
                    emit sagaSiblingsLoaded(itemId, {});
                continue;
            }
            QList<QPair<QString,QString>> qChildren = {
                {"ParentId", boxSetId},
                {"Fields", "PrimaryImageAspectRatio,UserData,SeriesPrimaryImageTag"},
                {"SortBy", "ProductionYear,SortName"},
                {"SortOrder", "Ascending"},
                {"ImageTypeLimit", "1"},
            };
            QNetworkReply* r2 = m_nam.get(makeRequest("/Users/" + m_userId + "/Items", qChildren));
            connect(r2, &QNetworkReply::finished, this, [this, r2, state, itemId]() {
                r2->deleteLater();
                if (state->emitted) {
                    if (--state->pending == 0) { /* state auto-released */ }
                    return;
                }
                if (r2->error() == QNetworkReply::NoError) {
                    const auto d = QJsonDocument::fromJson(r2->readAll());
                    const auto children = parseItems(d.object().value("Items").toArray());
                    for (const auto& it : children) {
                        if (it.id == itemId) {
                            state->emitted = true;
                            emit sagaSiblingsLoaded(itemId, children);
                            break;
                        }
                    }
                }
                if (--state->pending == 0 && !state->emitted) {
                    emit sagaSiblingsLoaded(itemId, {});
                }
            });
        }
    });
}

void JellyfinClient::fetchGenrePeers(const QString& itemId,
                                     const QStringList& genres, int limit) {
    if (genres.isEmpty()) { emit genrePeersLoaded(itemId, {}); return; }
    QList<QPair<QString,QString>> q = {
        {"Recursive", "true"},
        {"IncludeItemTypes", "Movie"},
        {"Genres", genres.join('|')},  // Jellyfin treats '|' as OR
        {"Limit", QString::number(limit)},
        {"SortBy", "Random"},
        {"Fields", "PrimaryImageAspectRatio,UserData,SeriesPrimaryImageTag"},
        {"ImageTypeLimit", "1"},
        {"ExcludeItemIds", itemId},
    };
    QNetworkReply* reply = m_nam.get(makeRequest("/Users/" + m_userId + "/Items", q));
    connect(reply, &QNetworkReply::finished, this, [this, reply, itemId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit genrePeersLoaded(itemId, {});
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit genrePeersLoaded(itemId, parseItems(doc.object().value("Items").toArray()));
    });
}

void JellyfinClient::fetchSimilar(const QString& itemId, int limit) {
    QList<QPair<QString,QString>> q = {
        {"UserId", m_userId},
        {"Limit", QString::number(limit)},
        {"Fields", "PrimaryImageAspectRatio,UserData,SeriesPrimaryImageTag"},
    };
    QNetworkReply* reply = m_nam.get(makeRequest("/Items/" + itemId + "/Similar", q));
    connect(reply, &QNetworkReply::finished, this, [this, reply, itemId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit similarLoaded(itemId, {});
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        emit similarLoaded(itemId, parseItems(doc.object().value("Items").toArray()));
    });
}

void JellyfinClient::setFavorite(const QString& itemId, bool favorite) {
    QNetworkRequest req = makeRequest("/Users/" + m_userId + "/FavoriteItems/" + itemId);
    QNetworkReply* reply = favorite
        ? m_nam.post(req, QByteArray())
        : m_nam.deleteResource(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, itemId, favorite]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }
        emit favoriteToggled(itemId, favorite);
    });
}

void JellyfinClient::setPlayed(const QString& itemId, bool played) {
    QNetworkRequest req = makeRequest("/Users/" + m_userId + "/PlayedItems/" + itemId);
    QNetworkReply* reply = played
        ? m_nam.post(req, QByteArray())
        : m_nam.deleteResource(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, itemId, played]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit networkError(reply->errorString());
            return;
        }
        emit playedToggled(itemId, played);
    });
}

void JellyfinClient::reportPlaybackStart(const QString& itemId,
                                          const JellyfinPlayback& info,
                                          qint64 positionTicks) {
    QJsonObject body;
    body["ItemId"] = itemId;
    body["PositionTicks"] = (double) positionTicks;
    body["PlayMethod"] = playMethodString(info.method);
    body["PlaySessionId"] = info.playSessionId;
    body["MediaSourceId"] = info.mediaSourceId;
    body["CanSeek"] = true;
    QNetworkRequest req = makeRequest("/Sessions/Playing");
    QNetworkReply* reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void JellyfinClient::reportPlaybackProgress(const QString& itemId,
                                             const JellyfinPlayback& info,
                                             qint64 positionTicks, bool paused) {
    QJsonObject body;
    body["ItemId"] = itemId;
    body["PositionTicks"] = (double) positionTicks;
    body["IsPaused"] = paused;
    body["PlayMethod"] = playMethodString(info.method);
    body["PlaySessionId"] = info.playSessionId;
    body["MediaSourceId"] = info.mediaSourceId;
    body["EventName"] = "TimeUpdate";
    QNetworkRequest req = makeRequest("/Sessions/Playing/Progress");
    QNetworkReply* reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void JellyfinClient::reportPlaybackStopped(const QString& itemId,
                                            const JellyfinPlayback& info,
                                            qint64 positionTicks) {
    QJsonObject body;
    body["ItemId"] = itemId;
    body["PositionTicks"] = (double) positionTicks;
    body["PlayMethod"] = playMethodString(info.method);
    body["PlaySessionId"] = info.playSessionId;
    body["MediaSourceId"] = info.mediaSourceId;
    QNetworkRequest req = makeRequest("/Sessions/Playing/Stopped");
    QNetworkReply* reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}
