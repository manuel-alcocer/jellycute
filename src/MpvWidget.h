#pragma once

#include <QOpenGLWidget>
#include <QStringList>
#include <mpv/client.h>
#include <mpv/render_gl.h>

struct MpvTrack {
    int id = 0;
    QString type;       // "audio", "sub", "video"
    QString title;
    QString lang;
    bool selected = false;
};

class MpvWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit MpvWidget(QWidget* parent = nullptr);
    ~MpvWidget() override;

    void play(const QString& url, qint64 startSeconds = 0);
    void stop();

    qint64 positionSeconds() const { return m_positionSeconds; }
    qint64 durationSeconds() const { return m_durationSeconds; }
    bool isPaused() const { return m_paused; }

    void setOscEnabled(bool on);
    void togglePause();
    void seekAbsolute(qint64 seconds);
    void seekRelative(qint64 deltaSeconds);

    QList<MpvTrack> tracks() const;
    void setAudioTrack(int id);
    void setSubtitleTrack(int id);   // 0 disables subs.

    int volume() const { return m_volume; }
    void setVolume(int v);
    void setVideoAspect(const QString& spec);   // "-1" = auto, e.g. "16/9"

    // Hardware-decode controls.
    QString hwdecSetting() const { return m_hwdecSetting; }   // "" = auto
    QString resolvedHwdec() const { return m_resolvedHwdec; }
    QString detectedDefaultHwdec() const;
    QStringList availableHwdecChoices() const;
    void setHwdecSetting(const QString& choice);

signals:
    void playbackStarted();
    void positionChanged(qint64 seconds);
    void durationChanged(qint64 seconds);
    void pausedChanged(bool paused);
    void tracksChanged();
    void volumeChanged(int volume);
    void idleChanged(bool idle);
    void endReached();
    void fullscreenToggleRequested();
    void fullscreenExitRequested();
    void hwdecChanged();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private slots:
    void drainEvents();
    void requestFrame();

private:
    static void onMpvWakeup(void* ctx);
    static void onMpvRenderUpdate(void* ctx);
    static void* getProcAddress(void* ctx, const char* name);
    void handleMpvEvent(mpv_event* ev);
    void sendKeyDown(const QString& mpvKey);
    QString qtKeyToMpv(QKeyEvent* e) const;

    QString resolveHwdec(const QString& pref) const;

    mpv_handle* m_mpv = nullptr;
    mpv_render_context* m_renderCtx = nullptr;
    qint64 m_positionSeconds = 0;
    qint64 m_durationSeconds = 0;
    int m_volume = 100;
    bool m_paused = false;
    bool m_idle = true;
    QString m_hwdecSetting;        // "" = auto
    QString m_resolvedHwdec;       // value passed to mpv
};
