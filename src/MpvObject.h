#pragma once

#include <QQuickFramebufferObject>
#include <QStringList>
#include <QVariantList>
#include <mpv/client.h>
#include <mpv/render_gl.h>

// QQuickItem-side mpv host. Mirrors MpvWidget's API but is implemented as a
// QQuickFramebufferObject so the same libmpv core can paint into the Qt
// Quick scene graph instead of a QOpenGLWidget. The rendering split is:
//   - MpvObject (GUI thread): owns mpv_handle, drains events, exposes
//     Q_INVOKABLE / Q_PROPERTY API to QML.
//   - MpvRenderer (scene-graph thread): owns mpv_render_context, lives in
//     the GL context provided by Qt Quick, renders frames into the item's
//     FBO. Created lazily on the first createFramebufferObject() call.
//
// MpvWidget remains in the codebase during the QML migration; it gets
// removed in phase 5 once the QML shell is the only entry point.
class MpvObject : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(qint64 position READ positionSeconds NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ durationSeconds NOTIFY durationChanged)
    Q_PROPERTY(bool paused READ isPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool idle READ isIdle NOTIFY idleChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QString hwdec READ hwdecSetting WRITE setHwdecSetting NOTIFY hwdecChanged)
    Q_PROPERTY(QString resolvedHwdec READ resolvedHwdec NOTIFY hwdecChanged)
public:
    explicit MpvObject(QQuickItem* parent = nullptr);
    ~MpvObject() override;

    Renderer* createRenderer() const override;

    // Borrowed by MpvRenderer (which runs on the SG render thread). The
    // render context is created lazily in the renderer but stored here on
    // the item so the item's destructor can free it before the libmpv core
    // is terminated — mpv aborts with "Broken API use:
    // mpv_render_context_free() not called" otherwise.
    mpv_handle* handle() const { return m_mpv; }
    mpv_render_context* renderCtx() const { return m_renderCtx; }
    void setRenderCtx(mpv_render_context* ctx) { m_renderCtx = ctx; }

    Q_INVOKABLE void play(const QString& url, qint64 startSeconds = 0);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seekAbsolute(qint64 seconds);
    Q_INVOKABLE void seekRelative(qint64 deltaSeconds);
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void setOscEnabled(bool on);
    Q_INVOKABLE void setVideoAspect(const QString& spec);
    Q_INVOKABLE QVariantList tracks() const;
    Q_INVOKABLE void setAudioTrack(int id);
    Q_INVOKABLE void setSubtitleTrack(int id);
    Q_INVOKABLE QStringList availableHwdecChoices() const;
    Q_INVOKABLE QString detectedDefaultHwdec() const;

    qint64 positionSeconds() const { return m_positionSeconds; }
    qint64 durationSeconds() const { return m_durationSeconds; }
    bool isPaused() const { return m_paused; }
    bool isIdle() const { return m_idle; }
    int volume() const { return m_volume; }
    bool isMuted() const { return m_muted; }
    QString hwdecSetting() const { return m_hwdecSetting; }
    QString resolvedHwdec() const { return m_resolvedHwdec; }

    void setVolume(int v);
    void setMuted(bool on);
    void setHwdecSetting(const QString& choice);

signals:
    void playbackStarted();
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void idleChanged();
    void volumeChanged();
    void mutedChanged();
    void hwdecChanged();
    void tracksChanged();
    void endReached();
    void fullscreenToggleRequested();
    void fullscreenExitRequested();

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void hoverMoveEvent(QHoverEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private slots:
    void drainEvents();

private:
    static void onMpvWakeup(void* ctx);
    void handleMpvEvent(mpv_event* ev);
    void sendKeyDown(const QString& mpvKey);
    QString qtKeyToMpv(QKeyEvent* e) const;
    QString resolveHwdec(const QString& pref) const;

    mpv_handle* m_mpv = nullptr;
    mpv_render_context* m_renderCtx = nullptr;
    qint64 m_positionSeconds = 0;
    qint64 m_durationSeconds = 0;
    int m_volume = 100;
    bool m_muted = false;
    bool m_paused = false;
    bool m_idle = true;
    QString m_hwdecSetting;
    QString m_resolvedHwdec;
};
