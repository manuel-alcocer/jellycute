#pragma once

#include "JellyfinClient.h"

#include <QObject>
#include <QString>
#include <QUrl>

// Singleton-ish controller that walks the playback FSM (idle → resolving →
// ready → playing → ended/error) so QML doesn't have to know JellyfinPlayback
// or thread it through page properties. DetailPage calls start(itemId);
// PlayerPage observes the resolved streamUrl and reports lifecycle events
// back through reportStarted/reportProgress/reportStopped, which translate
// into the matching JellyfinClient REST calls and (for transcodes) the
// stop-active-encoding teardown.
class PlaybackSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(JellyfinClient* client READ client WRITE setClient NOTIFY clientChanged)
    Q_PROPERTY(QString itemId READ itemId NOTIFY itemIdChanged)
    Q_PROPERTY(QUrl streamUrl READ streamUrl NOTIFY streamUrlChanged)
    Q_PROPERTY(qint64 startSeconds READ startSeconds NOTIFY startSecondsChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    // The error message attached to a state == "error" transition.
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)

public:
    explicit PlaybackSession(QObject* parent = nullptr);

    JellyfinClient* client() const { return m_client; }
    void setClient(JellyfinClient* c);

    QString itemId() const { return m_itemId; }
    QUrl streamUrl() const { return m_info.url; }
    qint64 startSeconds() const { return m_startSeconds; }
    QString state() const { return m_state; }
    QString errorMessage() const { return m_errorMessage; }

    Q_INVOKABLE void start(const QString& itemId, qint64 startSeconds = 0);
    // Lifecycle reports forwarded from PlayerPage / MpvObject. Position is in
    // whole seconds; the C++ side converts to ticks at the boundary.
    Q_INVOKABLE void reportStarted(qint64 positionSeconds);
    Q_INVOKABLE void reportProgress(qint64 positionSeconds, bool paused);
    Q_INVOKABLE void reportStopped(qint64 positionSeconds);
    // Tear down a session whose PlayerPage was popped before reaching
    // end-of-file (user clicked back). Tells the server to release any
    // active transcoding job.
    Q_INVOKABLE void cancel();

signals:
    void clientChanged();
    void itemIdChanged();
    void streamUrlChanged();
    void startSecondsChanged();
    void stateChanged();

private:
    void onResolved(const QString& itemId, const JellyfinPlayback& info);
    void onResolveFailed(const QString& itemId, const QString& message);
    void setState(const QString& s, const QString& errorMessage = {});

    JellyfinClient* m_client = nullptr;
    QString m_itemId;
    JellyfinPlayback m_info;
    qint64 m_startSeconds = 0;
    QString m_state = QStringLiteral("idle");
    QString m_errorMessage;
};
