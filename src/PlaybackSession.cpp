#include "PlaybackSession.h"

#include <QLoggingCategory>

namespace {
constexpr qint64 TICKS_PER_SECOND = 10'000'000LL;
Q_LOGGING_CATEGORY(lcPlay, "jellycute.playback")
}

PlaybackSession::PlaybackSession(QObject* parent) : QObject(parent) {}

void PlaybackSession::setClient(JellyfinClient* c) {
    if (m_client == c) return;
    m_client = c;
    emit clientChanged();
    if (!c) return;
    connect(c, &JellyfinClient::playbackResolved, this,
            &PlaybackSession::onResolved);
    connect(c, &JellyfinClient::playbackResolveFailed, this,
            &PlaybackSession::onResolveFailed);
}

void PlaybackSession::start(const QString& itemId, qint64 startSeconds) {
    qCDebug(lcPlay) << "start" << itemId << "startSeconds=" << startSeconds;
    if (!m_client || itemId.isEmpty()) return;
    m_itemId = itemId;
    m_startSeconds = startSeconds;
    m_info = JellyfinPlayback{};
    emit itemIdChanged();
    emit streamUrlChanged();
    emit startSecondsChanged();
    setState(QStringLiteral("resolving"));
    m_client->resolvePlayback(itemId, startSeconds * TICKS_PER_SECOND);
}

void PlaybackSession::reportStarted(qint64 positionSeconds) {
    if (!m_client || m_itemId.isEmpty()) return;
    m_client->reportPlaybackStart(m_itemId, m_info,
                                  positionSeconds * TICKS_PER_SECOND);
    setState(QStringLiteral("playing"));
}

void PlaybackSession::reportProgress(qint64 positionSeconds, bool paused) {
    if (!m_client || m_itemId.isEmpty()) return;
    m_client->reportPlaybackProgress(m_itemId, m_info,
                                     positionSeconds * TICKS_PER_SECOND,
                                     paused);
}

void PlaybackSession::reportStopped(qint64 positionSeconds) {
    if (!m_client || m_itemId.isEmpty()) return;
    m_client->reportPlaybackStopped(m_itemId, m_info,
                                    positionSeconds * TICKS_PER_SECOND);
    if (m_info.method == JellyfinPlayback::Transcode
        && !m_info.playSessionId.isEmpty()) {
        m_client->stopActiveEncoding(m_info.playSessionId);
    }
    setState(QStringLiteral("ended"));
}

void PlaybackSession::cancel() {
    if (!m_client || m_itemId.isEmpty()) {
        setState(QStringLiteral("idle"));
        return;
    }
    if (m_info.method == JellyfinPlayback::Transcode
        && !m_info.playSessionId.isEmpty()) {
        m_client->stopActiveEncoding(m_info.playSessionId);
    }
    m_itemId.clear();
    m_info = JellyfinPlayback{};
    m_startSeconds = 0;
    emit itemIdChanged();
    emit streamUrlChanged();
    emit startSecondsChanged();
    setState(QStringLiteral("idle"));
}

void PlaybackSession::onResolved(const QString& itemId,
                                 const JellyfinPlayback& info) {
    qCDebug(lcPlay) << "onResolved" << itemId << "method=" << int(info.method);
    if (itemId != m_itemId) return;
    m_info = info;
    emit streamUrlChanged();
    setState(QStringLiteral("ready"));
}

void PlaybackSession::onResolveFailed(const QString& itemId,
                                      const QString& message) {
    qCWarning(lcPlay) << "onResolveFailed" << itemId << message;
    if (itemId != m_itemId) return;
    setState(QStringLiteral("error"), message);
}

void PlaybackSession::setState(const QString& s, const QString& errorMessage) {
    if (m_state == s && m_errorMessage == errorMessage) return;
    m_state = s;
    m_errorMessage = errorMessage;
    emit stateChanged();
}
