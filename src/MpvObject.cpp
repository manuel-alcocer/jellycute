#include "MpvObject.h"

#include <cstdlib>

#include <QDir>
#include <QFile>
#include <QHoverEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMetaObject>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QProcess>
#include <QQuickWindow>
#include <QSet>
#include <QSettings>
#include <QVarLengthArray>
#include <QWheelEvent>

namespace {

void mpvSetOption(mpv_handle* h, const char* name, const char* value) {
    mpv_set_option_string(h, name, value);
}

void mpvCommand(mpv_handle* h, std::initializer_list<const char*> args) {
    QVarLengthArray<const char*, 8> v;
    for (auto a : args) v.append(a);
    v.append(nullptr);
    mpv_command(h, v.data());
}

void* getProcAddress(void* /*ctx*/, const char* name) {
    QOpenGLContext* glctx = QOpenGLContext::currentContext();
    if (!glctx) return nullptr;
    return reinterpret_cast<void*>(glctx->getProcAddress(QByteArray(name)));
}

QString readPciVendorId() {
    QDir drm(QStringLiteral("/sys/class/drm"));
    const auto entries = drm.entryList(QStringList{QStringLiteral("card[0-9]*")},
                                        QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : entries) {
        if (name.contains(QChar('-'))) continue;
        QFile f(drm.filePath(name + QStringLiteral("/device/vendor")));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QString v = QString::fromLatin1(f.readAll()).trimmed().toLower();
        if (!v.isEmpty()) return v;
    }
    return {};
}

const char* mpvMouseButtonName(Qt::MouseButton b) {
    switch (b) {
    case Qt::LeftButton:   return "MBTN_LEFT";
    case Qt::MiddleButton: return "MBTN_MID";
    case Qt::RightButton:  return "MBTN_RIGHT";
    default: return nullptr;
    }
}

}  // anonymous

// -----------------------------------------------------------------------------
// MpvRenderer — runs on the scene-graph render thread.
// -----------------------------------------------------------------------------

namespace {

class MpvRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit MpvRenderer(MpvObject* item)
        : m_item(item) {}

    ~MpvRenderer() override {
        if (m_renderCtx) {
            mpv_render_context_free(m_renderCtx);
            m_renderCtx = nullptr;
        }
    }

    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
        if (!m_renderCtx) {
            mpv_opengl_init_params glInit{
                /*get_proc_address=*/&getProcAddress,
                /*get_proc_address_ctx=*/nullptr,
            };
            int advancedControl = 1;
            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
                {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
                {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advancedControl},
                {MPV_RENDER_PARAM_INVALID, nullptr},
            };
            if (mpv_render_context_create(&m_renderCtx, m_item->handle(), params) < 0)
                qFatal("mpv_render_context_create failed (Qt Quick path)");

            // Bridge mpv's "I have a new frame" signal back to QQuickItem's
            // update() so the scene-graph schedules another render() pass.
            mpv_render_context_set_update_callback(
                m_renderCtx,
                [](void* ctx) {
                    auto* obj = static_cast<MpvObject*>(ctx);
                    QMetaObject::invokeMethod(obj, &MpvObject::update,
                                              Qt::QueuedConnection);
                },
                m_item);
        }
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::NoAttachment);
        return new QOpenGLFramebufferObject(size, fmt);
    }

    void render() override {
        if (!m_renderCtx) return;

        // Tell Qt Quick we're about to issue raw GL state changes so it
        // doesn't assume its own state survives across the render pass.
        if (auto* w = m_item->window()) w->beginExternalCommands();

        QOpenGLFramebufferObject* fbo = framebufferObject();
        mpv_opengl_fbo mpfbo{
            /*fbo=*/static_cast<int>(fbo->handle()),
            /*w=*/fbo->width(),
            /*h=*/fbo->height(),
            /*internal_format=*/0,
        };
        // QQuick FBOs are bottom-up in GL convention, no flip needed.
        int flipY = 0;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        mpv_render_context_render(m_renderCtx, params);

        if (auto* w = m_item->window()) w->endExternalCommands();
    }

private:
    MpvObject* m_item;
    mpv_render_context* m_renderCtx = nullptr;
};

}  // anonymous

// -----------------------------------------------------------------------------
// MpvObject
// -----------------------------------------------------------------------------

MpvObject::MpvObject(QQuickItem* parent) : QQuickFramebufferObject(parent) {
    setMirrorVertically(true);  // QQuickFBO renders bottom-up; flip for screen.
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setActiveFocusOnTab(true);
    setFlag(ItemHasContents, true);

    m_mpv = mpv_create();
    if (!m_mpv) qFatal("mpv_create failed (Qt Quick path)");

    // Same baseline configuration as MpvWidget — see its constructor for
    // the rationale on each option.
    mpvSetOption(m_mpv, "vo", "libmpv");
    mpvSetOption(m_mpv, "config", "no");
    mpvSetOption(m_mpv, "force-window", "no");
    mpvSetOption(m_mpv, "input-default-bindings", "yes");
    mpvSetOption(m_mpv, "input-vo-keyboard", "yes");
    mpvSetOption(m_mpv, "osc", "yes");
    mpvSetOption(m_mpv, "keep-open", "no");
    mpvSetOption(m_mpv, "idle", "yes");
    mpvSetOption(m_mpv, "ytdl", "no");
    mpvSetOption(m_mpv, "audio-display", "no");

    if (const char* envHwdec = std::getenv("JELLYCUTE_MPV_HWDEC")) {
        m_hwdecSetting = QString::fromUtf8(envHwdec);
    } else {
        m_hwdecSetting = QSettings().value(QStringLiteral("hwdec")).toString();
    }
    m_resolvedHwdec = resolveHwdec(m_hwdecSetting);
    mpvSetOption(m_mpv, "hwdec", m_resolvedHwdec.toUtf8().constData());

    mpvSetOption(m_mpv, "audio-buffer", "1.0");
    mpvSetOption(m_mpv, "terminal", "yes");
    mpvSetOption(m_mpv, "msg-level", "all=warn,libmpv_render=fatal");

    if (mpv_initialize(m_mpv) < 0) qFatal("mpv_initialize failed (Qt Quick path)");

    mpvCommand(m_mpv, {"keybind", "q", "stop"});
    mpvCommand(m_mpv, {"keybind", "Q", "stop"});

    mpv_observe_property(m_mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 2, "pause",    MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 3, "idle-active", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 4, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 5, "volume",   MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 6, "mute",     MPV_FORMAT_FLAG);

    mpv_set_wakeup_callback(m_mpv, &MpvObject::onMpvWakeup, this);
}

MpvObject::~MpvObject() {
    if (m_mpv) {
        // Renderer (and its mpv_render_context) is destroyed by Qt Quick
        // when the scene graph tears down; the libmpv core is freed last.
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

QQuickFramebufferObject::Renderer* MpvObject::createRenderer() const {
    return new MpvRenderer(const_cast<MpvObject*>(this));
}

// ---- High-level commands ---------------------------------------------------

void MpvObject::play(const QString& url, qint64 startSeconds) {
    if (!m_mpv) return;
    QByteArray u = url.toUtf8();
    if (startSeconds > 0) {
        QByteArray opt = QByteArray("start=+") + QByteArray::number((qlonglong) startSeconds);
        const char* args[] = {"loadfile", u.constData(), "replace", "0", opt.constData(), nullptr};
        mpv_command(m_mpv, args);
    } else {
        const char* args[] = {"loadfile", u.constData(), "replace", nullptr};
        mpv_command(m_mpv, args);
    }
    forceActiveFocus();
}

void MpvObject::stop() { if (m_mpv) mpvCommand(m_mpv, {"stop"}); }

void MpvObject::togglePause() {
    if (m_mpv) mpvCommand(m_mpv, {"cycle", "pause"});
}

void MpvObject::seekAbsolute(qint64 seconds) {
    if (!m_mpv) return;
    QByteArray s = QByteArray::number((qlonglong) seconds);
    const char* args[] = {"seek", s.constData(), "absolute", nullptr};
    mpv_command(m_mpv, args);
}

void MpvObject::seekRelative(qint64 deltaSeconds) {
    if (!m_mpv) return;
    QByteArray s = QByteArray::number((qlonglong) deltaSeconds);
    const char* args[] = {"seek", s.constData(), "relative", nullptr};
    mpv_command(m_mpv, args);
}

void MpvObject::setVolume(int v) {
    if (!m_mpv) return;
    QByteArray s = QByteArray::number(v);
    mpv_set_property_string(m_mpv, "volume", s.constData());
}

void MpvObject::setMuted(bool on) {
    if (!m_mpv) return;
    mpv_set_property_string(m_mpv, "mute", on ? "yes" : "no");
}

void MpvObject::toggleMute() { setMuted(!m_muted); }

void MpvObject::setOscEnabled(bool on) {
    if (m_mpv) mpv_set_property_string(m_mpv, "osc", on ? "yes" : "no");
}

void MpvObject::setVideoAspect(const QString& spec) {
    if (!m_mpv) return;
    if (spec == QStringLiteral("auto")) {
        mpv_set_property_string(m_mpv, "video-aspect-override", "no");
        mpv_set_property_string(m_mpv, "video-aspect-mode",     "container");
    } else {
        mpv_set_property_string(m_mpv, "video-aspect-override",
                                spec.toUtf8().constData());
    }
}

QVariantList MpvObject::tracks() const {
    QVariantList out;
    if (!m_mpv) return out;
    char* s = mpv_get_property_string(m_mpv, "track-list");
    if (!s) return out;
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(s));
    mpv_free(s);
    for (const auto& v : doc.array()) {
        const auto o = v.toObject();
        QVariantMap m;
        m["id"]       = o.value("id").toInt();
        m["type"]     = o.value("type").toString();
        m["title"]    = o.value("title").toString();
        m["lang"]     = o.value("lang").toString();
        m["selected"] = o.value("selected").toBool();
        out.append(m);
    }
    return out;
}

void MpvObject::setAudioTrack(int id) {
    if (!m_mpv) return;
    QByteArray s = QByteArray::number(id);
    mpv_set_property_string(m_mpv, "aid", s.constData());
}

void MpvObject::setSubtitleTrack(int id) {
    if (!m_mpv) return;
    QByteArray s = id <= 0 ? QByteArray("no") : QByteArray::number(id);
    mpv_set_property_string(m_mpv, "sid", s.constData());
}

QString MpvObject::detectedDefaultHwdec() const {
    const QString vendor = readPciVendorId();
    if (vendor == QStringLiteral("0x10de")) return QStringLiteral("nvdec");
    if (vendor == QStringLiteral("0x1002")) return QStringLiteral("vaapi");
    if (vendor == QStringLiteral("0x8086")) return QStringLiteral("vaapi");
    return QStringLiteral("no");
}

QString MpvObject::resolveHwdec(const QString& pref) const {
    if (pref.isEmpty()) return detectedDefaultHwdec();
    return pref;
}

QStringList MpvObject::availableHwdecChoices() const {
    static QStringList cached;
    if (!cached.isEmpty()) return cached;
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

void MpvObject::setHwdecSetting(const QString& choice) {
    m_hwdecSetting = choice;
    QSettings().setValue(QStringLiteral("hwdec"), choice);
    m_resolvedHwdec = resolveHwdec(choice);
    if (m_mpv) {
        mpv_set_property_string(m_mpv, "hwdec",
                                m_resolvedHwdec.toUtf8().constData());
    }
    emit hwdecChanged();
}

// ---- Event drain -----------------------------------------------------------

void MpvObject::onMpvWakeup(void* ctx) {
    auto* self = static_cast<MpvObject*>(ctx);
    QMetaObject::invokeMethod(self, &MpvObject::drainEvents, Qt::QueuedConnection);
}

void MpvObject::drainEvents() {
    if (!m_mpv) return;
    while (true) {
        mpv_event* ev = mpv_wait_event(m_mpv, 0);
        if (!ev || ev->event_id == MPV_EVENT_NONE) break;
        handleMpvEvent(ev);
    }
}

void MpvObject::handleMpvEvent(mpv_event* ev) {
    switch (ev->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        auto* prop = static_cast<mpv_event_property*>(ev->data);
        if (!prop || !prop->name) break;
        QByteArray name = prop->name;
        if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE) {
            qint64 secs = (qint64) *(double*) prop->data;
            if (secs != m_positionSeconds) {
                m_positionSeconds = secs;
                emit positionChanged();
            }
        } else if (name == "pause" && prop->format == MPV_FORMAT_FLAG) {
            bool paused = *(int*) prop->data != 0;
            if (paused != m_paused) { m_paused = paused; emit pausedChanged(); }
        } else if (name == "idle-active" && prop->format == MPV_FORMAT_FLAG) {
            bool idle = *(int*) prop->data != 0;
            if (idle != m_idle) { m_idle = idle; emit idleChanged(); }
        } else if (name == "duration" && prop->format == MPV_FORMAT_DOUBLE) {
            qint64 secs = (qint64) *(double*) prop->data;
            if (secs != m_durationSeconds) {
                m_durationSeconds = secs;
                emit durationChanged();
            }
        } else if (name == "volume" && prop->format == MPV_FORMAT_DOUBLE) {
            int v = (int) *(double*) prop->data;
            if (v != m_volume) { m_volume = v; emit volumeChanged(); }
        } else if (name == "mute" && prop->format == MPV_FORMAT_FLAG) {
            bool m = *(int*) prop->data != 0;
            if (m != m_muted) { m_muted = m; emit mutedChanged(); }
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

// ---- Input plumbing --------------------------------------------------------

void MpvObject::sendKeyDown(const QString& mpvKey) {
    if (!m_mpv || mpvKey.isEmpty()) return;
    QByteArray k = mpvKey.toUtf8();
    const char* args[] = {"keypress", k.constData(), nullptr};
    mpv_command(m_mpv, args);
}

QString MpvObject::qtKeyToMpv(QKeyEvent* e) const {
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
    case Qt::Key_Return:
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

void MpvObject::keyPressEvent(QKeyEvent* e) {
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
            sendKeyDown("ESC");
            e->accept();
            return;
        }
    }
    const QString k = qtKeyToMpv(e);
    if (k.isEmpty()) { QQuickFramebufferObject::keyPressEvent(e); return; }
    sendKeyDown(k);
    e->accept();
}

void MpvObject::mousePressEvent(QMouseEvent* e) {
    if (!m_mpv) { QQuickFramebufferObject::mousePressEvent(e); return; }
    // mpv works in pixels; the FBO size is the item's QQuickItem size in
    // device pixels (Qt Quick handles DPR internally for FBOs).
    const QByteArray xs = QByteArray::number(int(e->position().x()));
    const QByteArray ys = QByteArray::number(int(e->position().y()));
    const char* mv[] = {"mouse", xs.constData(), ys.constData(), nullptr};
    mpv_command(m_mpv, mv);

    if (const char* name = mpvMouseButtonName(e->button())) {
        const char* args[] = {"keydown", name, nullptr};
        mpv_command(m_mpv, args);
    }
    forceActiveFocus();
    e->accept();
}

void MpvObject::mouseReleaseEvent(QMouseEvent* e) {
    if (!m_mpv) { QQuickFramebufferObject::mouseReleaseEvent(e); return; }
    if (const char* name = mpvMouseButtonName(e->button())) {
        const char* args[] = {"keyup", name, nullptr};
        mpv_command(m_mpv, args);
    }
    e->accept();
}

void MpvObject::mouseDoubleClickEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) emit fullscreenToggleRequested();
    e->accept();
}

void MpvObject::mouseMoveEvent(QMouseEvent* e) {
    if (!m_mpv) { QQuickFramebufferObject::mouseMoveEvent(e); return; }
    const QByteArray xs = QByteArray::number(int(e->position().x()));
    const QByteArray ys = QByteArray::number(int(e->position().y()));
    const char* args[] = {"mouse", xs.constData(), ys.constData(), nullptr};
    mpv_command(m_mpv, args);
}

void MpvObject::hoverMoveEvent(QHoverEvent* e) {
    // Qt Quick delivers movement-without-button via hover events; mpv's OSC
    // expects a continuous mouse position stream to track the seekbar.
    if (!m_mpv) return;
    const QByteArray xs = QByteArray::number(int(e->position().x()));
    const QByteArray ys = QByteArray::number(int(e->position().y()));
    const char* args[] = {"mouse", xs.constData(), ys.constData(), nullptr};
    mpv_command(m_mpv, args);
}

void MpvObject::wheelEvent(QWheelEvent* e) {
    int dy = e->angleDelta().y();
    if (dy > 0) sendKeyDown("WHEEL_UP");
    else if (dy < 0) sendKeyDown("WHEEL_DOWN");
    int dx = e->angleDelta().x();
    if (dx > 0) sendKeyDown("WHEEL_RIGHT");
    else if (dx < 0) sendKeyDown("WHEEL_LEFT");
    e->accept();
}
