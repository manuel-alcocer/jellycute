#include "MpvWidget.h"

#include <cstdlib>

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QScreen>
#include <QTimer>
#include <QVarLengthArray>
#include <QWheelEvent>
#include <QtGlobal>

namespace {
QByteArray toUtf8(const QString& s) { return s.toUtf8(); }

void mpvSetOption(mpv_handle* h, const char* name, const char* value) {
    mpv_set_option_string(h, name, value);
}

void mpvCommand(mpv_handle* h, std::initializer_list<const char*> args) {
    QVarLengthArray<const char*, 8> v;
    for (auto a : args) v.append(a);
    v.append(nullptr);
    mpv_command(h, v.data());
}
}

MpvWidget::MpvWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(160, 90);

    m_mpv = mpv_create();
    if (!m_mpv) qFatal("mpv_create failed");

    // Render API requires the libmpv VO; mpv will hand frames to us.
    // Set vo *first* and disable the user config so a stray vo= line in
    // ~/.config/mpv/mpv.conf can't pull mpv off into its own window.
    mpvSetOption(m_mpv, "vo", "libmpv");
    mpvSetOption(m_mpv, "config", "no");
    mpvSetOption(m_mpv, "force-window", "no");

    // Behave like real mpv: own bindings + OSC + keep handle alive on EOF.
    mpvSetOption(m_mpv, "input-default-bindings", "yes");
    mpvSetOption(m_mpv, "input-vo-keyboard", "yes");
    mpvSetOption(m_mpv, "osc", "yes");
    mpvSetOption(m_mpv, "keep-open", "no");
    mpvSetOption(m_mpv, "idle", "yes");
    mpvSetOption(m_mpv, "ytdl", "no");
    mpvSetOption(m_mpv, "audio-display", "no");
    // Hardware decoding: resolve once and pass to mpv. Order of precedence:
    //   1. JELLYCUTE_MPV_HWDEC env var (debug override).
    //   2. QSettings("hwdec") if non-empty (set via the menu).
    //   3. Auto-detected from PCI vendor of the active DRM card.
    if (const char* envHwdec = std::getenv("JELLYCUTE_MPV_HWDEC")) {
        m_hwdecSetting = QString::fromUtf8(envHwdec);
    } else {
        m_hwdecSetting = QSettings().value(QStringLiteral("hwdec")).toString();
    }
    m_resolvedHwdec = resolveHwdec(m_hwdecSetting);
    mpvSetOption(m_mpv, "hwdec", m_resolvedHwdec.toUtf8().constData());

    // Audio: a chunkier buffer prevents underruns on software decode.
    mpvSetOption(m_mpv, "audio-buffer", "1.0");        // 1.0 s

    mpvSetOption(m_mpv, "terminal", "yes");
    // libmpv_render's `gl_check_error` is overly chatty and prints
    // benign INVALID_ENUM entries after probe/init; mute that module.
    mpvSetOption(m_mpv, "msg-level", "all=warn,libmpv_render=fatal");

    if (mpv_initialize(m_mpv) < 0) qFatal("mpv_initialize failed");

    // Re-bind hard-quit keys to "stop" so the player handle stays alive
    // and the UI can return to the browser cleanly.
    mpvCommand(m_mpv, {"keybind", "q", "stop"});
    mpvCommand(m_mpv, {"keybind", "Q", "stop"});

    mpv_observe_property(m_mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 2, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 3, "idle-active", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 4, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 5, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 6, "mute",   MPV_FORMAT_FLAG);

    mpv_set_wakeup_callback(m_mpv, &MpvWidget::onMpvWakeup, this);
}

MpvWidget::~MpvWidget() {
    if (m_renderCtx) {
        mpv_render_context_free(m_renderCtx);
        m_renderCtx = nullptr;
    }
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void* MpvWidget::getProcAddress(void* /*ctx*/, const char* name) {
    QOpenGLContext* glctx = QOpenGLContext::currentContext();
    if (!glctx) return nullptr;
    return reinterpret_cast<void*>(glctx->getProcAddress(QByteArray(name)));
}

void MpvWidget::initializeGL() {
    mpv_opengl_init_params glInit{
        /*get_proc_address=*/&MpvWidget::getProcAddress,
        /*get_proc_address_ctx=*/nullptr,
    };
    int advancedControl = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
        {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advancedControl},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    if (mpv_render_context_create(&m_renderCtx, m_mpv, params) < 0)
        qFatal("mpv_render_context_create failed");
    mpv_render_context_set_update_callback(m_renderCtx,
                                           &MpvWidget::onMpvRenderUpdate, this);
}

void MpvWidget::paintGL() {
    if (!m_renderCtx) return;
    // Use the physical FBO size: QOpenGLWidget keeps its FBO at
    // size() * devicePixelRatio(), so this stays in sync during resizes.
    const qreal dpr = devicePixelRatioF();
    mpv_opengl_fbo fbo{
        /*fbo=*/static_cast<int>(defaultFramebufferObject()),
        /*w=*/static_cast<int>(width() * dpr),
        /*h=*/static_cast<int>(height() * dpr),
        /*internal_format=*/0,
    };
    int flipY = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flipY},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    mpv_render_context_render(m_renderCtx, params);
}

void MpvWidget::resizeGL(int /*w*/, int /*h*/) {
    // QOpenGLWidget recreates the FBO on resize; force a fresh frame so
    // the video tracks the new widget size immediately, even when paused.
    update();
}

void MpvWidget::onMpvRenderUpdate(void* ctx) {
    auto* self = static_cast<MpvWidget*>(ctx);
    QMetaObject::invokeMethod(self, &MpvWidget::requestFrame, Qt::QueuedConnection);
}

void MpvWidget::requestFrame() {
    if (!m_renderCtx) return;
    const uint64_t flags = mpv_render_context_update(m_renderCtx);
    if (flags & MPV_RENDER_UPDATE_FRAME) update();
}

void MpvWidget::play(const QString& url, qint64 startSeconds) {
    if (!m_mpv) return;
    QByteArray u = toUtf8(url);
    if (startSeconds > 0) {
        QByteArray opt = QByteArray("start=+") + QByteArray::number((qlonglong) startSeconds);
        const char* args[] = {"loadfile", u.constData(), "replace", "0", opt.constData(), nullptr};
        mpv_command(m_mpv, args);
    } else {
        const char* args[] = {"loadfile", u.constData(), "replace", nullptr};
        mpv_command(m_mpv, args);
    }
    setFocus();
}

void MpvWidget::stop() {
    if (!m_mpv) return;
    mpvCommand(m_mpv, {"stop"});
}

void MpvWidget::setOscEnabled(bool on) {
    if (!m_mpv) return;
    mpv_set_property_string(m_mpv, "osc", on ? "yes" : "no");
}

void MpvWidget::togglePause() {
    if (!m_mpv) return;
    mpvCommand(m_mpv, {"cycle", "pause"});
}

void MpvWidget::seekAbsolute(qint64 seconds) {
    if (!m_mpv) return;
    QByteArray s = QByteArray::number((qlonglong) seconds);
    const char* args[] = {"seek", s.constData(), "absolute", nullptr};
    mpv_command(m_mpv, args);
}

void MpvWidget::seekRelative(qint64 deltaSeconds) {
    if (!m_mpv) return;
    QByteArray s = QByteArray::number((qlonglong) deltaSeconds);
    const char* args[] = {"seek", s.constData(), "relative", nullptr};
    mpv_command(m_mpv, args);
}

QList<MpvTrack> MpvWidget::tracks() const {
    QList<MpvTrack> out;
    if (!m_mpv) return out;
    char* s = mpv_get_property_string(m_mpv, "track-list");
    if (!s) return out;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(s));
    mpv_free(s);
    for (const auto& v : doc.array()) {
        const auto o = v.toObject();
        MpvTrack t;
        t.id = o.value("id").toInt();
        t.type = o.value("type").toString();
        t.title = o.value("title").toString();
        t.lang = o.value("lang").toString();
        t.selected = o.value("selected").toBool();
        out << t;
    }
    return out;
}

void MpvWidget::setAudioTrack(int id) {
    if (!m_mpv) return;
    QByteArray s = QByteArray::number(id);
    mpv_set_property_string(m_mpv, "aid", s.constData());
}

void MpvWidget::setSubtitleTrack(int id) {
    if (!m_mpv) return;
    QByteArray s = id <= 0 ? QByteArray("no") : QByteArray::number(id);
    mpv_set_property_string(m_mpv, "sid", s.constData());
}

void MpvWidget::setVolume(int v) {
    if (!m_mpv) return;
    QByteArray s = QByteArray::number(v);
    mpv_set_property_string(m_mpv, "volume", s.constData());
}

void MpvWidget::setMuted(bool on) {
    if (!m_mpv) return;
    mpv_set_property_string(m_mpv, "mute", on ? "yes" : "no");
}

void MpvWidget::toggleMute() {
    setMuted(!m_muted);
}

void MpvWidget::setVideoAspect(const QString& spec) {
    if (!m_mpv) return;
    if (spec == QStringLiteral("auto")) {
        // Modern mpv: clear the override and let the video aspect be
        // governed by the container dimensions.
        mpv_set_property_string(m_mpv, "video-aspect-override", "no");
        mpv_set_property_string(m_mpv, "video-aspect-mode",     "container");
    } else {
        mpv_set_property_string(m_mpv, "video-aspect-override",
                                spec.toUtf8().constData());
    }
}

void MpvWidget::onMpvWakeup(void* ctx) {
    auto* self = static_cast<MpvWidget*>(ctx);
    QMetaObject::invokeMethod(self, &MpvWidget::drainEvents, Qt::QueuedConnection);
}

void MpvWidget::drainEvents() {
    if (!m_mpv) return;
    while (true) {
        mpv_event* ev = mpv_wait_event(m_mpv, 0);
        if (!ev || ev->event_id == MPV_EVENT_NONE) break;
        handleMpvEvent(ev);
    }
}

void MpvWidget::handleMpvEvent(mpv_event* ev) {
    switch (ev->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        auto* prop = static_cast<mpv_event_property*>(ev->data);
        if (!prop || !prop->name) break;
        QByteArray name = prop->name;
        if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE) {
            double t = *(double*) prop->data;
            qint64 secs = (qint64) t;
            if (secs != m_positionSeconds) {
                m_positionSeconds = secs;
                emit positionChanged(secs);
            }
        } else if (name == "pause" && prop->format == MPV_FORMAT_FLAG) {
            bool paused = *(int*) prop->data != 0;
            if (paused != m_paused) {
                m_paused = paused;
                emit pausedChanged(paused);
            }
        } else if (name == "idle-active" && prop->format == MPV_FORMAT_FLAG) {
            bool idle = *(int*) prop->data != 0;
            if (idle != m_idle) {
                m_idle = idle;
                emit idleChanged(idle);
            }
        } else if (name == "duration" && prop->format == MPV_FORMAT_DOUBLE) {
            double d = *(double*) prop->data;
            qint64 secs = (qint64) d;
            if (secs != m_durationSeconds) {
                m_durationSeconds = secs;
                emit durationChanged(secs);
            }
        } else if (name == "volume" && prop->format == MPV_FORMAT_DOUBLE) {
            int v = (int) *(double*) prop->data;
            if (v != m_volume) {
                m_volume = v;
                emit volumeChanged(v);
            }
        } else if (name == "mute" && prop->format == MPV_FORMAT_FLAG) {
            bool m = *(int*) prop->data != 0;
            if (m != m_muted) {
                m_muted = m;
                emit mutedChanged(m);
            }
        }
        break;
    }
    case MPV_EVENT_FILE_LOADED:
        emit playbackStarted();
        emit tracksChanged();
        break;
    case MPV_EVENT_END_FILE:
        emit endReached();
        break;
    default:
        break;
    }
}

void MpvWidget::sendKeyDown(const QString& mpvKey) {
    if (!m_mpv || mpvKey.isEmpty()) return;
    QByteArray k = toUtf8(mpvKey);
    const char* args[] = {"keypress", k.constData(), nullptr};
    mpv_command(m_mpv, args);
}

QString MpvWidget::qtKeyToMpv(QKeyEvent* e) const {
    int key = e->key();
    Qt::KeyboardModifiers mods = e->modifiers();

    QString prefix;
    if (mods & Qt::ShiftModifier)   prefix += "Shift+";
    if (mods & Qt::ControlModifier) prefix += "Ctrl+";
    if (mods & Qt::AltModifier)     prefix += "Alt+";
    if (mods & Qt::MetaModifier)    prefix += "Meta+";

    QString name;
    switch (key) {
    case Qt::Key_Left:      name = "LEFT"; break;
    case Qt::Key_Right:     name = "RIGHT"; break;
    case Qt::Key_Up:        name = "UP"; break;
    case Qt::Key_Down:      name = "DOWN"; break;
    case Qt::Key_Space:     name = "SPACE"; break;
    case Qt::Key_Return:    name = "ENTER"; break;
    case Qt::Key_Enter:     name = "ENTER"; break;
    case Qt::Key_Backspace: name = "BS"; break;
    case Qt::Key_Tab:       name = "TAB"; break;
    case Qt::Key_PageUp:    name = "PGUP"; break;
    case Qt::Key_PageDown:  name = "PGDWN"; break;
    case Qt::Key_Home:      name = "HOME"; break;
    case Qt::Key_End:       name = "END"; break;
    case Qt::Key_Insert:    name = "INS"; break;
    case Qt::Key_Delete:    name = "DEL"; break;
    case Qt::Key_F1: case Qt::Key_F2: case Qt::Key_F3: case Qt::Key_F4:
    case Qt::Key_F5: case Qt::Key_F6: case Qt::Key_F7: case Qt::Key_F8:
    case Qt::Key_F9: case Qt::Key_F10: case Qt::Key_F11: case Qt::Key_F12:
        name = QString("F%1").arg(key - Qt::Key_F1 + 1);
        break;
    case Qt::Key_VolumeUp:    name = "VOLUME_UP"; break;
    case Qt::Key_VolumeDown:  name = "VOLUME_DOWN"; break;
    case Qt::Key_VolumeMute:  name = "MUTE"; break;
    case Qt::Key_MediaPlay:   name = "PLAY"; break;
    case Qt::Key_MediaPause:  name = "PAUSE"; break;
    case Qt::Key_MediaTogglePlayPause: name = "PLAYPAUSE"; break;
    case Qt::Key_MediaStop:   name = "STOP"; break;
    case Qt::Key_MediaNext:   name = "NEXT"; break;
    case Qt::Key_MediaPrevious: name = "PREV"; break;
    default:
        const QString text = e->text();
        if (!text.isEmpty() && text[0].isPrint()) {
            QChar ch = text[0];
            if (mods & Qt::ShiftModifier) ch = ch.toLower();
            name = QString(ch);
        }
        break;
    }
    if (name.isEmpty()) return {};
    return prefix + name;
}

void MpvWidget::keyPressEvent(QKeyEvent* e) {
    const Qt::KeyboardModifiers noTextMods =
        e->modifiers() & ~(Qt::ShiftModifier | Qt::KeypadModifier);
    if (noTextMods == Qt::NoModifier) {
        if (e->key() == Qt::Key_F) {
            emit fullscreenToggleRequested();
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_Escape) {
            emit fullscreenExitRequested();
            // Also stop the player so q/ESC behave like before in windowed mode.
            sendKeyDown("ESC");
            e->accept();
            return;
        }
    }
    const QString k = qtKeyToMpv(e);
    if (k.isEmpty()) { QOpenGLWidget::keyPressEvent(e); return; }
    sendKeyDown(k);
    e->accept();
}

namespace {
const char* mpvMouseButtonName(Qt::MouseButton b) {
    switch (b) {
    case Qt::LeftButton:   return "MBTN_LEFT";
    case Qt::MiddleButton: return "MBTN_MID";
    case Qt::RightButton:  return "MBTN_RIGHT";
    default: return nullptr;
    }
}
}

void MpvWidget::mousePressEvent(QMouseEvent* e) {
    if (!m_mpv) { QOpenGLWidget::mousePressEvent(e); return; }
    // Send the position FIRST in mpv's coordinate space (which matches the
    // FBO size we hand to the render API — physical pixels), then a keydown
    // for the button. Using keydown/keyup (not keypress) lets the OSC handle
    // seekbar dragging properly.
    const qreal dpr = devicePixelRatioF();
    const QByteArray xs = QByteArray::number(int(e->position().x() * dpr));
    const QByteArray ys = QByteArray::number(int(e->position().y() * dpr));
    const char* mv[] = {"mouse", xs.constData(), ys.constData(), nullptr};
    mpv_command(m_mpv, mv);

    if (const char* name = mpvMouseButtonName(e->button())) {
        const char* args[] = {"keydown", name, nullptr};
        mpv_command(m_mpv, args);
    }
    setFocus();
    e->accept();
}

void MpvWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (!m_mpv) { QOpenGLWidget::mouseReleaseEvent(e); return; }
    if (const char* name = mpvMouseButtonName(e->button())) {
        const char* args[] = {"keyup", name, nullptr};
        mpv_command(m_mpv, args);
    }
    e->accept();
}

void MpvWidget::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) emit fullscreenToggleRequested();
    e->accept();
}

void MpvWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!m_mpv) { QOpenGLWidget::mouseMoveEvent(e); return; }
    const qreal dpr = devicePixelRatioF();
    const QByteArray xs = QByteArray::number(int(e->position().x() * dpr));
    const QByteArray ys = QByteArray::number(int(e->position().y() * dpr));
    const char* args[] = {"mouse", xs.constData(), ys.constData(), nullptr};
    mpv_command(m_mpv, args);
}

void MpvWidget::wheelEvent(QWheelEvent* e) {
    int dy = e->angleDelta().y();
    if (dy > 0) sendKeyDown("WHEEL_UP");
    else if (dy < 0) sendKeyDown("WHEEL_DOWN");
    int dx = e->angleDelta().x();
    if (dx > 0) sendKeyDown("WHEEL_RIGHT");
    else if (dx < 0) sendKeyDown("WHEEL_LEFT");
    e->accept();
}

// ---- Hardware decoding helpers ----

namespace {
QString readPciVendorId() {
    // Walk /sys/class/drm/cardN; return the first card's vendor as 0xNNNN.
    QDir drm(QStringLiteral("/sys/class/drm"));
    const auto entries = drm.entryList(QStringList{QStringLiteral("card[0-9]*")},
                                        QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : entries) {
        // Skip "cardN-..." connector entries.
        if (name.contains(QChar('-'))) continue;
        QFile f(drm.filePath(name + QStringLiteral("/device/vendor")));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QString v = QString::fromLatin1(f.readAll()).trimmed().toLower();
        if (!v.isEmpty()) return v;
    }
    return {};
}
}

QString MpvWidget::detectedDefaultHwdec() const {
    const QString vendor = readPciVendorId();
    if (vendor == QStringLiteral("0x10de")) return QStringLiteral("nvdec");
    if (vendor == QStringLiteral("0x1002")) return QStringLiteral("vaapi");   // AMD
    if (vendor == QStringLiteral("0x8086")) return QStringLiteral("vaapi");   // Intel
    return QStringLiteral("no");
}

QString MpvWidget::resolveHwdec(const QString& pref) const {
    if (pref.isEmpty()) return detectedDefaultHwdec();
    return pref;
}

QStringList MpvWidget::availableHwdecChoices() const {
    static QStringList cached;
    if (!cached.isEmpty()) return cached;

    // libmpv doesn't expose a choices list for hwdec via the property API,
    // so we shell out to `mpv --hwdec=help` once and parse its output. mpv
    // only prints the backends actually built into the running version, so
    // this matches what setting hwdec= will accept.
    QProcess p;
    p.start(QStringLiteral("mpv"), QStringList{QStringLiteral("--hwdec=help")});
    if (p.waitForStarted(1500) && p.waitForFinished(2500)) {
        const QString output = QString::fromUtf8(
            p.readAllStandardOutput() + p.readAllStandardError());
        QSet<QString> seen;
        for (const QString& line : output.split(QChar('\n'))) {
            if (line.isEmpty() || !line.at(0).isSpace()) continue;
            const QString t = line.trimmed();
            if (t.isEmpty() || !t.at(0).isLower()) continue;
            const int sp = t.indexOf(QChar(' '));
            const QString name = sp < 0 ? t : t.left(sp);
            if (!name.isEmpty()) seen.insert(name);
        }
        cached = QStringList(seen.begin(), seen.end());
    }
    if (cached.isEmpty()) {
        // Defensive fallback if `mpv` isn't on PATH.
        cached = QStringList{
            "no", "auto", "auto-safe", "auto-copy",
            "vaapi", "vaapi-copy", "nvdec", "nvdec-copy",
            "vdpau", "vdpau-copy", "drmprime", "drmprime-copy",
            "vulkan", "vulkan-copy",
        };
    }
    cached.sort();
    return cached;
}

void MpvWidget::setHwdecSetting(const QString& choice) {
    m_hwdecSetting = choice;
    QSettings().setValue(QStringLiteral("hwdec"), choice);
    m_resolvedHwdec = resolveHwdec(choice);
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "hwdec",
                                m_resolvedHwdec.toUtf8().constData());
    }
    emit hwdecChanged();
}
