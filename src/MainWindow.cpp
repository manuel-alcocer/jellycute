#include "MainWindow.h"
#include "AccountStore.h"
#include "BrowserWidget.h"
#include "LoginDialog.h"
#include "MpvWidget.h"
#include "SettingsDialog.h"
#include "Theme.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QtMath>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QSettings>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

namespace {
constexpr qint64 TICKS_PER_SECOND = 10'000'000LL;

// Monochrome line-art icons rendered with QPainter so the look stays
// consistent regardless of the system icon theme.
// Recolours an existing QPainter-drawn icon by SourceIn-compositing a flat
// fill over its alpha mask. Lets us reuse the dark icons as a light-tinted
// variant when the control bar overlays the video.
QIcon tintedIcon(const QIcon& src, QColor color, int size = 24) {
    QPixmap pm = src.pixmap(QSize(size, size));
    if (pm.isNull()) return src;
    QPixmap out(pm.size());
    out.setDevicePixelRatio(pm.devicePixelRatio());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawPixmap(0, 0, pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(QRect(0, 0, size, size), color);
    p.end();
    return QIcon(out);
}

QIcon makeIcon(int size, std::function<void(QPainter&)> draw,
               QColor color = Theme::foregroundColor()) {
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color);
    pen.setWidthF(1.5);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    draw(p);
    p.end();
    return QIcon(pm);
}

QIcon iconSidebar() {
    return makeIcon(24, [](QPainter& p) {
        p.drawRoundedRect(QRectF(3, 4, 18, 16), 2, 2);
        p.drawLine(QPointF(9, 4), QPointF(9, 20));
    });
}
QIcon iconBack() {
    return makeIcon(24, [](QPainter& p) {
        p.drawLine(QPointF(4, 12), QPointF(20, 12));
        p.drawLine(QPointF(4, 12), QPointF(10, 6));
        p.drawLine(QPointF(4, 12), QPointF(10, 18));
    });
}
QIcon iconUsers() {
    return makeIcon(24, [](QPainter& p) {
        p.drawEllipse(QPointF(9, 9), 3, 3);
        QPainterPath b1;
        b1.moveTo(3, 20); b1.cubicTo(3, 14, 15, 14, 15, 20);
        p.drawPath(b1);
        p.drawEllipse(QPointF(16, 8), 2.5, 2.5);
        QPainterPath b2;
        b2.moveTo(12, 19); b2.cubicTo(13, 14, 21, 14, 21, 19);
        p.drawPath(b2);
    });
}
QIcon iconLogout() {
    return makeIcon(24, [](QPainter& p) {
        p.drawRoundedRect(QRectF(3, 3, 10, 18), 1, 1);
        p.drawLine(QPointF(13, 12), QPointF(21, 12));
        p.drawLine(QPointF(17, 8),  QPointF(21, 12));
        p.drawLine(QPointF(17, 16), QPointF(21, 12));
    });
}
QIcon iconFullscreen() {
    return makeIcon(24, [](QPainter& p) {
        p.drawLine(QPointF(3, 8),  QPointF(3, 3));
        p.drawLine(QPointF(3, 3),  QPointF(8, 3));
        p.drawLine(QPointF(16, 3), QPointF(21, 3));
        p.drawLine(QPointF(21, 3), QPointF(21, 8));
        p.drawLine(QPointF(21, 16), QPointF(21, 21));
        p.drawLine(QPointF(21, 21), QPointF(16, 21));
        p.drawLine(QPointF(8, 21),  QPointF(3, 21));
        p.drawLine(QPointF(3, 21),  QPointF(3, 16));
    });
}
QIcon iconOsc() {
    return makeIcon(28, [](QPainter& p) {
        p.drawRoundedRect(QRectF(2.5, 6, 23, 16), 3, 3);
        QFont f = p.font();
        f.setPointSizeF(7.5);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(2.5, 6, 23, 16), Qt::AlignCenter, "OSC");
    });
}
QIcon iconPlay() {
    return makeIcon(24, [](QPainter& p) {
        p.setBrush(p.pen().color());
        QPolygonF tri;
        tri << QPointF(7, 5) << QPointF(7, 19) << QPointF(19, 12);
        p.drawPolygon(tri);
    });
}
QIcon iconPause() {
    return makeIcon(24, [](QPainter& p) {
        p.setBrush(p.pen().color());
        p.drawRoundedRect(QRectF(7, 5, 3.5, 14), 1, 1);
        p.drawRoundedRect(QRectF(13.5, 5, 3.5, 14), 1, 1);
    });
}
QIcon iconSeekBack() {
    return makeIcon(24, [](QPainter& p) {
        p.setBrush(p.pen().color());
        QPolygonF t1; t1 << QPointF(11, 6) << QPointF(11, 18) << QPointF(4, 12);
        QPolygonF t2; t2 << QPointF(20, 6) << QPointF(20, 18) << QPointF(13, 12);
        p.drawPolygon(t1); p.drawPolygon(t2);
    });
}
QIcon iconSeekFwd() {
    return makeIcon(24, [](QPainter& p) {
        p.setBrush(p.pen().color());
        QPolygonF t1; t1 << QPointF(4, 6) << QPointF(4, 18) << QPointF(11, 12);
        QPolygonF t2; t2 << QPointF(13, 6) << QPointF(13, 18) << QPointF(20, 12);
        p.drawPolygon(t1); p.drawPolygon(t2);
    });
}
QIcon iconAudio() {
    return makeIcon(24, [](QPainter& p) {
        QPainterPath s;
        s.moveTo(3, 9); s.lineTo(8, 9); s.lineTo(13, 5);
        s.lineTo(13, 19); s.lineTo(8, 15); s.lineTo(3, 15);
        s.closeSubpath();
        p.drawPath(s);
        QPainterPath w1; w1.moveTo(16, 9);  w1.cubicTo(19, 10.5, 19, 13.5, 16, 15);
        QPainterPath w2; w2.moveTo(18, 6);  w2.cubicTo(23, 9,    23, 15,   18, 18);
        p.drawPath(w1); p.drawPath(w2);
    });
}
QIcon iconSubs() {
    return makeIcon(24, [](QPainter& p) {
        p.drawRoundedRect(QRectF(2.5, 4, 19, 16), 2, 2);
        p.drawLine(QPointF(5.5, 11), QPointF(13, 11));
        p.drawLine(QPointF(5.5, 15), QPointF(18, 15));
    });
}
QIcon iconVolume() {
    return makeIcon(24, [](QPainter& p) {
        QPainterPath s;
        s.moveTo(3, 9); s.lineTo(7, 9); s.lineTo(11, 5);
        s.lineTo(11, 19); s.lineTo(7, 15); s.lineTo(3, 15);
        s.closeSubpath();
        p.drawPath(s);
        QPainterPath w; w.moveTo(14, 9); w.cubicTo(17, 10.5, 17, 13.5, 14, 15);
        p.drawPath(w);
    });
}
QIcon iconVolumeMuted() {
    // Speaker silhouette + diagonal slash to communicate the muted state.
    return makeIcon(24, [](QPainter& p) {
        QPainterPath s;
        s.moveTo(3, 9); s.lineTo(7, 9); s.lineTo(11, 5);
        s.lineTo(11, 19); s.lineTo(7, 15); s.lineTo(3, 15);
        s.closeSubpath();
        p.drawPath(s);
        QPen pen = p.pen();
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.drawLine(QPointF(14, 8), QPointF(21, 16));
        p.drawLine(QPointF(21, 8), QPointF(14, 16));
    });
}
QIcon iconAutoHide() {
    // A pushpin: filled head + needle below. The button's :checked styling
    // (sunken/blue) communicates that the auto-hide mode is active.
    return makeIcon(24, [](QPainter& p) {
        p.setBrush(p.pen().color());
        QPainterPath head;
        head.moveTo(8, 4);
        head.lineTo(16, 4);
        head.lineTo(15, 9);
        head.lineTo(17, 9);
        head.lineTo(17, 12);
        head.lineTo(7, 12);
        head.lineTo(7, 9);
        head.lineTo(9, 9);
        head.closeSubpath();
        p.drawPath(head);
        QPen pen = p.pen();
        pen.setWidthF(2.0);
        p.setPen(pen);
        p.drawLine(QPointF(12, 12), QPointF(12, 20));
    });
}
QIcon iconAspect() {
    return makeIcon(24, [](QPainter& p) {
        p.drawRoundedRect(QRectF(2.5, 5, 19, 14), 1.5, 1.5);
        // Two diagonal arrows.
        p.drawLine(QPointF(6, 9),  QPointF(10, 9));
        p.drawLine(QPointF(6, 9),  QPointF(6, 13));
        p.drawLine(QPointF(18, 15), QPointF(14, 15));
        p.drawLine(QPointF(18, 15), QPointF(18, 11));
    });
}
QIcon iconSettings() {
    // Line-art gear: 8-tooth cog outline + small concentric hole. Built as
    // a star polygon (16 alternating outer/inner vertices) stroked without
    // fill so it matches the rest of the toolbar's line-art icons.
    return makeIcon(24, [](QPainter& p) {
        const QPointF c(12, 12);
        constexpr int teeth = 8;
        constexpr qreal rOuter = 10.8;
        constexpr qreal rInner = 7.6;
        QPolygonF poly;
        for (int i = 0; i < teeth * 2; ++i) {
            const qreal angle =
                qDegreesToRadians(-90.0 + i * (180.0 / teeth));
            const qreal r = (i % 2 == 0) ? rOuter : rInner;
            poly << QPointF(c.x() + r * qCos(angle),
                            c.y() + r * qSin(angle));
        }
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(poly);
        p.drawEllipse(c, 3.4, 3.4);
    });
}
QIcon iconExit() {
    // Power symbol: open ring + vertical bar at the top.
    return makeIcon(24, [](QPainter& p) {
        QRectF ring(4, 5, 16, 16);
        // Arc starting at -60° spanning -240° (i.e. opens facing up).
        p.drawArc(ring, -150 * 16, -240 * 16);
        p.drawLine(QPointF(12, 3), QPointF(12, 11));
    });
}
}

/* QSlider variant that jumps to the clicked position on press (instead of
   stepping by pageStep) and shows a tooltip describing the value under the
   cursor while hovering. The tooltip text is produced by an injected
   formatter so the same widget renders both the position-time string and
   the volume-percent string. */
class ClickableSlider : public QSlider {
public:
    using TooltipFormatter = std::function<QString(int)>;
    explicit ClickableSlider(Qt::Orientation o, QWidget* parent = nullptr)
        : QSlider(o, parent) {
        setMouseTracking(true);
    }
    void setTooltipFormatter(TooltipFormatter f) { m_fmt = std::move(f); }

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            const int v = valueFromPos(e->pos());
            setSliderDown(true);
            emit sliderPressed();
            setValue(v);
            emit sliderMoved(v);
            e->accept();
            return;
        }
        QSlider::mousePressEvent(e);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (isSliderDown()) {
            const int v = valueFromPos(e->pos());
            setValue(v);
            emit sliderMoved(v);
        }
        if (m_fmt)
            QToolTip::showText(e->globalPosition().toPoint(),
                               m_fmt(valueFromPos(e->pos())), this);
        QSlider::mouseMoveEvent(e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && isSliderDown()) {
            setSliderDown(false);
            emit sliderReleased();
            e->accept();
            return;
        }
        QSlider::mouseReleaseEvent(e);
    }

private:
    int valueFromPos(QPoint p) const {
        int x, span;
        if (orientation() == Qt::Horizontal) {
            x = p.x();
            span = width();
        } else {
            x = height() - p.y();
            span = height();
        }
        if (span <= 0) return minimum();
        const double t = double(qBound(0, x, span)) / double(span);
        return minimum() +
               int(std::round(t * double(maximum() - minimum())));
    }

    TooltipFormatter m_fmt;
};

/* Custom widget that owns its paintEvent fully: each setText() schedules a
   single repaint, the entire bounding rect is filled with the parent palette
   colour, and the new glyphs are drawn in one drawText() call. No partial
   redraws or surface composition that could leave ghost glyphs from the
   previous frame. */
class TimeDisplay : public QWidget {
public:
    explicit TimeDisplay(Qt::Alignment align, QWidget* parent = nullptr)
        : QWidget(parent), m_align(align) {
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
    }
    void setText(const QString& s) {
        if (s == m_text) return;
        m_text = s;
        update();
    }
    QSize sizeHint() const override {
        const QFontMetrics fm(font());
        return QSize(fm.horizontalAdvance("00:00:00") + 4, fm.height() + 4);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), palette().color(QPalette::Window));
        p.setPen(palette().color(QPalette::WindowText));
        p.setFont(font());
        p.drawText(rect(), int(m_align) | Qt::AlignVCenter, m_text);
    }
private:
    QString m_text = "00:00:00";
    Qt::Alignment m_align;
};

MainWindow::MainWindow(JellyfinClient* client, QWidget* parent)
    : QMainWindow(parent), m_client(client) {
    setWindowTitle(tr("jellycute"));
    resize(1280, 800);

    m_stack = new QStackedWidget(this);
    m_browser = new BrowserWidget(client, this);

    m_playerPage = new QWidget(this);
    auto* playerLayout = new QVBoxLayout(m_playerPage);
    playerLayout->setContentsMargins(0, 0, 0, 0);
    playerLayout->setSpacing(0);

    m_player = new MpvWidget(m_playerPage);
    playerLayout->addWidget(m_player, 1);

    buildPlayerControlBar();
    playerLayout->addWidget(m_controlBar);
    buildTrackPanels();

    // Click outside an open track panel closes it.
    qApp->installEventFilter(this);

    m_stack->addWidget(m_browser);
    m_stack->addWidget(m_playerPage);
    setCentralWidget(m_stack);

    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(5000);

    connect(m_browser, &BrowserWidget::playRequested, this, &MainWindow::onPlayWithStart);
    connect(m_player, &MpvWidget::idleChanged, this, &MainWindow::onMpvIdleChanged);
    connect(m_player, &MpvWidget::endReached, this, &MainWindow::onMpvEndReached);
    connect(m_player, &MpvWidget::pausedChanged, this, &MainWindow::onMpvPaused);
    connect(m_player, &MpvWidget::positionChanged, this, &MainWindow::onMpvPosition);
    connect(m_player, &MpvWidget::positionChanged, this, [this](qint64 s) {
        if (!m_userSeeking && m_positionSlider) {
            QSignalBlocker b(m_positionSlider);
            m_positionSlider->setValue(int(s));
        }
        if (m_posLabel) m_posLabel->setText(hms(s));
    });
    connect(m_player, &MpvWidget::durationChanged, this, [this](qint64 d) {
        if (m_positionSlider) {
            QSignalBlocker b(m_positionSlider);
            m_positionSlider->setRange(0, int(d));
        }
        if (m_durLabel) m_durLabel->setText(hms(d));
    });
    connect(m_player, &MpvWidget::pausedChanged, this, [this](bool p) {
        if (!m_playPauseBtn) return;
        QIcon icon = p ? iconPlay() : iconPause();
        if (m_autoHideEnabled) icon = tintedIcon(icon, QColor("#eff0f1"));
        m_playPauseBtn->setIcon(icon);
    });
    connect(m_player, &MpvWidget::tracksChanged, this, &MainWindow::rebuildTrackMenus);
    connect(m_player, &MpvWidget::volumeChanged, this, [this](int v) {
        if (m_volumeSlider) {
            QSignalBlocker b(m_volumeSlider);
            m_volumeSlider->setValue(v);
        }
    });
    connect(m_player, &MpvWidget::mutedChanged, this, [this](bool muted) {
        if (m_volumeIconBtn)
            m_volumeIconBtn->setToolTip(muted ? tr("Activar sonido")
                                              : tr("Silenciar"));
        applyControlBarIcons();
    });
    connect(m_player, &MpvWidget::fullscreenToggleRequested,
            this, &MainWindow::toggleFullscreen);
    connect(m_player, &MpvWidget::fullscreenExitRequested,
            this, &MainWindow::exitFullscreen);
    connect(m_progressTimer, &QTimer::timeout, this, &MainWindow::reportProgressTick);
    connect(m_client, &JellyfinClient::playbackResolved,
            this, &MainWindow::onPlaybackResolved);
    connect(m_client, &JellyfinClient::playbackResolveFailed,
            this, &MainWindow::onPlaybackResolveFailed);

    // No menu bar: all configuration is reachable from the toolbar's
    // settings dialog.
    setMenuBar(nullptr);
    buildToolBar();

    // Restore the auto-hide preference. setChecked() emits toggled which
    // routes through setAutoHideEnabled and applies the overlay layout.
    const bool autoHide = QSettings()
        .value(QStringLiteral("autoHideControls"), false).toBool();
    if (autoHide && m_autoHideBtn) m_autoHideBtn->setChecked(true);

    m_browser->start();
    statusBar()->showMessage(tr("Conectado a %1").arg(client->server().toString()));
}

void MainWindow::buildToolBar() {
    m_toolBar = new QToolBar(tr("Herramientas"), this);
    m_toolBar->setObjectName(QStringLiteral("mainToolbar"));
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(20, 20));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    addToolBar(Qt::TopToolBarArea, m_toolBar);

    // Each toolbar action is paired with the icon factory that produced it
    // so refreshThemedIcons() can rebuild the pixmap with the new theme
    // colour without rebuilding the toolbar itself.
    auto track = [this](QAction* act, std::function<QIcon()> make) {
        m_iconRefreshers << [act, make]() { act->setIcon(make()); };
        return act;
    };

    auto* sidebarAct = track(
        m_toolBar->addAction(iconSidebar(), tr("Ocultar/mostrar sidebar")),
        &iconSidebar);
    connect(sidebarAct, &QAction::triggered, this, [this]() {
        m_browser->setSidebarVisible(!m_browser->isSidebarVisible());
    });

    auto* backAct = track(m_toolBar->addAction(iconBack(), tr("Atrás")),
                          &iconBack);
    connect(backAct, &QAction::triggered, this, [this]() {
        // From the player → stop and go back to the browser. Otherwise pop
        // one breadcrumb level inside the browser.
        if (m_stack && m_stack->currentWidget() == m_playerPage)
            switchToBrowser();
        else
            m_browser->goBack();
    });

    m_toolBar->addSeparator();

    // User account picker. The button stays in the toolbar; its drop-down
    // is rebuilt every time it opens so newly added/removed accounts show
    // up without any explicit refresh wiring.
    m_userBtn = new QToolButton(m_toolBar);
    m_userBtn->setIcon(iconUsers());
    m_userBtn->setToolTip(tr("Cuenta activa"));
    m_userBtn->setAutoRaise(true);
    m_userBtn->setPopupMode(QToolButton::InstantPopup);
    m_accountsMenu = new QMenu(m_userBtn);
    m_userBtn->setMenu(m_accountsMenu);
    connect(m_accountsMenu, &QMenu::aboutToShow, this,
            &MainWindow::rebuildAccountsMenu);
    m_toolBar->addWidget(m_userBtn);
    m_iconRefreshers << [this]() { m_userBtn->setIcon(iconUsers()); };

    auto* logoutAct = track(
        m_toolBar->addAction(iconLogout(), tr("Cerrar sesión")), &iconLogout);
    connect(logoutAct, &QAction::triggered, this, [this]() { close(); });

    m_toolBar->addSeparator();

    m_fullscreenAct = new QAction(iconFullscreen(), tr("Pantalla completa"), this);
    m_fullscreenAct->setShortcut(QKeySequence(Qt::Key_F11));
    m_fullscreenAct->setShortcutContext(Qt::ApplicationShortcut);
    m_fullscreenAct->setCheckable(true);
    connect(m_fullscreenAct, &QAction::toggled, this, [this](bool on) {
        if (on == isFullScreen()) return;
        statusBar()->setVisible(!on);
        if (m_toolBar) m_toolBar->setVisible(!on);
        if (on) showFullScreen(); else showNormal();
        if (on) m_player->setFocus();
    });
    m_toolBar->addAction(m_fullscreenAct);
    track(m_fullscreenAct, &iconFullscreen);

    m_oscToggleAct = track(
        m_toolBar->addAction(iconOsc(),
                             tr("Alternar OSC mpv / controles propios")),
        &iconOsc);
    m_oscToggleAct->setCheckable(true);
    const bool useNativeOsc =
        QSettings().value(QStringLiteral("useNativeOsc"), true).toBool();
    m_oscToggleAct->setChecked(useNativeOsc);
    connect(m_oscToggleAct, &QAction::toggled, this,
            [this](bool on) { applyOscMode(on); });
    applyOscMode(useNativeOsc);

    m_toolBar->addSeparator();

    auto* settingsAct = track(
        m_toolBar->addAction(iconSettings(), tr("Ajustes")), &iconSettings);
    connect(settingsAct, &QAction::triggered, this, &MainWindow::openSettings);

    auto* exitAct = track(m_toolBar->addAction(iconExit(), tr("Salir")),
                          &iconExit);
    exitAct->setShortcut(QKeySequence::Quit);
    exitAct->setShortcutContext(Qt::ApplicationShortcut);
    connect(exitAct, &QAction::triggered, this, &QMainWindow::close);
}

void MainWindow::openSettings() {
    const QString themeBefore = Theme::currentKey();
    SettingsDialog dlg(m_browser, m_player, this);
    dlg.exec();
    if (Theme::currentKey() != themeBefore) {
        Theme::apply();
        refreshThemedIcons();
    }
}

void MainWindow::refreshThemedIcons() {
    for (const auto& f : m_iconRefreshers) f();
    applyControlBarIcons();
}

void MainWindow::buildPlayerControlBar() {
    m_controlBar = new QWidget(m_playerPage);
    m_controlBar->setObjectName(QStringLiteral("playerControlBar"));
    auto* cb = new QHBoxLayout(m_controlBar);
    cb->setContentsMargins(8, 4, 8, 8);
    cb->setSpacing(8);

    m_seekBackBtn = new QToolButton(m_controlBar);
    m_seekBackBtn->setIcon(iconSeekBack());
    m_seekBackBtn->setToolTip(tr("Atrás 10s"));
    m_seekBackBtn->setAutoRaise(true);

    m_playPauseBtn = new QToolButton(m_controlBar);
    m_playPauseBtn->setIcon(iconPause());
    m_playPauseBtn->setToolTip(tr("Reproducir / Pausa"));
    m_playPauseBtn->setAutoRaise(true);

    m_seekFwdBtn = new QToolButton(m_controlBar);
    m_seekFwdBtn->setIcon(iconSeekFwd());
    m_seekFwdBtn->setToolTip(tr("Adelantar 10s"));
    m_seekFwdBtn->setAutoRaise(true);

    auto* posSlider = new ClickableSlider(Qt::Horizontal, m_controlBar);
    posSlider->setRange(0, 0);
    posSlider->setSingleStep(5);
    posSlider->setPageStep(60);
    posSlider->setTooltipFormatter(
        [](int v) { return MainWindow::hms(qint64(v)); });
    m_positionSlider = posSlider;

    // Use the inherited (regular) font — same as menus/labels. Widths come
    // from the worst-case "88:88:88" so digits with different metrics don't
    // reflow the layout as seconds tick.
    const QFontMetrics fm(font());
    const int posWidth = fm.horizontalAdvance("88:88:88") + 12;

    m_posLabel = new TimeDisplay(Qt::AlignRight, m_controlBar);
    m_posLabel->setFixedWidth(posWidth);

    auto* sep = new QLabel("/", m_controlBar);
    sep->setAlignment(Qt::AlignCenter);

    m_durLabel = new TimeDisplay(Qt::AlignLeft, m_controlBar);
    m_durLabel->setFixedWidth(posWidth);

    m_audioBtn = new QToolButton(m_controlBar);
    m_audioBtn->setIcon(iconAudio());
    m_audioBtn->setToolTip(tr("Pista de audio"));
    m_audioBtn->setAutoRaise(true);
    m_audioBtn->setVisible(false);
    connect(m_audioBtn, &QToolButton::clicked, this, [this]() {
        if (m_audioPanel && m_audioPanel->isVisible())
            slideDownPanel(m_audioPanel);
        else
            slideUpPanel(m_audioPanel);
    });

    m_subBtn = new QToolButton(m_controlBar);
    m_subBtn->setIcon(iconSubs());
    m_subBtn->setToolTip(tr("Subtítulos"));
    m_subBtn->setAutoRaise(true);
    m_subBtn->setVisible(false);
    connect(m_subBtn, &QToolButton::clicked, this, [this]() {
        if (m_subPanel && m_subPanel->isVisible())
            slideDownPanel(m_subPanel);
        else
            slideUpPanel(m_subPanel);
    });

    m_volumeIconBtn = new QToolButton(m_controlBar);
    m_volumeIconBtn->setIcon(iconVolume());
    m_volumeIconBtn->setAutoRaise(true);
    m_volumeIconBtn->setToolTip(tr("Silenciar"));
    connect(m_volumeIconBtn, &QToolButton::clicked, this,
            [this]() { m_player->toggleMute(); });

    auto* volSlider = new ClickableSlider(Qt::Horizontal, m_controlBar);
    volSlider->setRange(0, 100);
    volSlider->setValue(100);
    volSlider->setFixedWidth(90);
    volSlider->setTooltipFormatter(
        [](int v) { return QStringLiteral("%1%").arg(v); });
    m_volumeSlider = volSlider;
    connect(m_volumeSlider, &QSlider::valueChanged, this,
            [this](int v) { m_player->setVolume(v); });

    m_autoHideBtn = new QToolButton(m_controlBar);
    m_autoHideBtn->setIcon(iconAutoHide());
    m_autoHideBtn->setAutoRaise(true);
    m_autoHideBtn->setCheckable(true);
    m_autoHideBtn->setToolTip(tr("Auto-ocultar controles"));
    connect(m_autoHideBtn, &QToolButton::toggled, this,
            [this](bool on) { setAutoHideEnabled(on); });

    m_fullscreenBtn = new QToolButton(m_controlBar);
    m_fullscreenBtn->setIcon(iconFullscreen());
    m_fullscreenBtn->setAutoRaise(true);
    m_fullscreenBtn->setToolTip(tr("Alternar pantalla completa (F11)"));
    connect(m_fullscreenBtn, &QToolButton::clicked, this,
            &MainWindow::toggleFullscreen);

    m_aspectBtn = new QToolButton(m_controlBar);
    m_aspectBtn->setIcon(iconAspect());
    m_aspectBtn->setAutoRaise(true);
    m_aspectBtn->setToolTip(tr("Relación de aspecto"));
    m_aspectBtn->setPopupMode(QToolButton::InstantPopup);
    auto* aspectMenu = new QMenu(m_aspectBtn);
    struct AspectChoice { const char* label; const char* value; };
    static const AspectChoice choices[] = {
        {QT_TR_NOOP("Auto"),    "auto"},
        {QT_TR_NOOP("16:9"),    "16/9"},
        {QT_TR_NOOP("4:3"),     "4/3"},
        {QT_TR_NOOP("21:9"),    "21/9"},
        {QT_TR_NOOP("2.35:1"),  "2.35"},
        {QT_TR_NOOP("1:1"),     "1"},
    };
    for (const auto& c : choices) {
        const QString val = QString::fromLatin1(c.value);
        const QString lbl = tr(c.label);
        auto* a = aspectMenu->addAction(lbl);
        connect(a, &QAction::triggered, this, [this, val]() {
            m_player->setVideoAspect(val);
        });
    }
    m_aspectBtn->setMenu(aspectMenu);

    cb->addWidget(m_seekBackBtn);
    cb->addWidget(m_playPauseBtn);
    cb->addWidget(m_seekFwdBtn);
    cb->addWidget(m_positionSlider, 1);
    cb->addWidget(m_volumeIconBtn);
    cb->addWidget(m_volumeSlider);
    cb->addWidget(m_audioBtn);
    cb->addWidget(m_subBtn);
    cb->addWidget(m_aspectBtn);
    cb->addWidget(m_autoHideBtn);
    cb->addWidget(m_fullscreenBtn);
    cb->addWidget(m_posLabel);
    cb->addWidget(sep);
    cb->addWidget(m_durLabel);

    connect(m_seekBackBtn, &QToolButton::clicked, this,
            [this]() { m_player->seekRelative(-10); });
    connect(m_seekFwdBtn, &QToolButton::clicked, this,
            [this]() { m_player->seekRelative(+10); });
    connect(m_playPauseBtn, &QToolButton::clicked, this,
            [this]() { m_player->togglePause(); });
    connect(m_positionSlider, &QSlider::sliderPressed, this,
            [this]() { m_userSeeking = true; });
    connect(m_positionSlider, &QSlider::sliderReleased, this, [this]() {
        m_player->seekAbsolute(m_positionSlider->value());
        m_userSeeking = false;
    });
    connect(m_positionSlider, &QSlider::sliderMoved, this, [this](int v) {
        if (m_posLabel) m_posLabel->setText(hms(v));
    });

    // Auto-hide infrastructure: opacity effect + fade animation + idle timer.
    // The effect stays installed in both modes (its initial opacity is 1.0
    // and inert until the fade animation drives it).
    m_controlBarOpacity = new QGraphicsOpacityEffect(m_controlBar);
    m_controlBarOpacity->setOpacity(1.0);
    m_controlBar->setGraphicsEffect(m_controlBarOpacity);
    m_controlBarFade = new QPropertyAnimation(m_controlBarOpacity, "opacity", this);
    m_controlBarFade->setDuration(220);
    m_controlBarFade->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_controlBarFade, &QPropertyAnimation::finished, this, [this]() {
        // After fade-out the bar must stop receiving mouse events so mpv
        // gets clicks/drags through the area it used to cover.
        if (m_controlBarOpacity && m_controlBarOpacity->opacity() <= 0.01)
            m_controlBar->setVisible(false);
    });

    m_autoHideTimer = new QTimer(this);
    m_autoHideTimer->setSingleShot(true);
    m_autoHideTimer->setInterval(2500);
    connect(m_autoHideTimer, &QTimer::timeout, this, &MainWindow::hideControlBar);
}

static QFrame* makeTrackPanel(QWidget* parent, const QString& title,
                              QVBoxLayout** outList, MainWindow* self) {
    auto* panel = new QFrame(parent);
    panel->setObjectName(QStringLiteral("trackPanel"));
    panel->setVisible(false);
    auto* root = new QVBoxLayout(panel);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(8);

    auto* header = new QHBoxLayout;
    auto* lbl = new QLabel(title, panel);
    QFont hf = lbl->font(); hf.setBold(true);
    hf.setPointSize(hf.pointSize() + 1);
    lbl->setFont(hf);
    auto* closeBtn = new QToolButton(panel);
    closeBtn->setText(QStringLiteral("✕"));
    closeBtn->setAutoRaise(true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    QObject::connect(closeBtn, &QToolButton::clicked, self,
                     [self, panel]() { self->slideDownPanel(panel); });
    header->addWidget(lbl, 1);
    header->addWidget(closeBtn);
    root->addLayout(header);

    *outList = new QVBoxLayout;
    (*outList)->setSpacing(2);
    root->addLayout(*outList);
    return panel;
}

void MainWindow::buildTrackPanels() {
    m_audioPanel = makeTrackPanel(m_playerPage, tr("Pista de audio"),
                                   &m_audioPanelList, this);
    m_subPanel   = makeTrackPanel(m_playerPage, tr("Subtítulos"),
                                   &m_subPanelList, this);
}

void MainWindow::slideUpPanel(QFrame* panel) {
    if (!panel) return;
    QToolButton* btn = (panel == m_audioPanel) ? m_audioBtn
                     : (panel == m_subPanel)   ? m_subBtn
                                                : nullptr;
    if (!btn) return;

    panel->adjustSize();   // make sizeHint reflect the freshly-built buttons.
    const QSize hint = panel->sizeHint();
    const int panelW = qBound(220, hint.width(),  m_playerPage->width() - 16);
    const int panelH = qBound( 80, hint.height(), m_playerPage->height() - 40);

    const QPoint btnTL = m_playerPage->mapFromGlobal(btn->mapToGlobal(QPoint(0, 0)));
    const int btnCenterX = btnTL.x() + btn->width() / 2;
    int targetX = btnCenterX - panelW / 2;
    targetX = qBound(8, targetX, m_playerPage->width() - panelW - 8);
    int targetY = btnTL.y() - panelH - 6;
    targetY = qMax(8, targetY);

    panel->setGeometry(targetX, m_playerPage->height(), panelW, panelH);
    panel->show();
    panel->raise();
    auto* anim = new QPropertyAnimation(panel, "geometry", this);
    anim->setDuration(220);
    anim->setStartValue(QRect(targetX, m_playerPage->height(), panelW, panelH));
    anim->setEndValue(QRect(targetX, targetY, panelW, panelH));
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::slideDownPanel(QFrame* panel) {
    if (!panel || !panel->isVisible()) return;
    const QRect start = panel->geometry();
    auto* anim = new QPropertyAnimation(panel, "geometry", this);
    anim->setDuration(180);
    anim->setStartValue(start);
    anim->setEndValue(QRect(start.x(), m_playerPage->height(),
                            start.width(), start.height()));
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, panel, &QWidget::hide);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::rebuildTrackMenus() {
    if (!m_audioBtn || !m_subBtn) return;
    const auto tracks = m_player->tracks();
    QList<MpvTrack> audios, subs;
    for (const auto& t : tracks) {
        if (t.type == "audio") audios << t;
        else if (t.type == "sub") subs << t;
    }
    auto trackLabel = [](const MpvTrack& t) {
        QStringList parts;
        if (!t.title.isEmpty()) parts << t.title;
        if (!t.lang.isEmpty())  parts << QString("[%1]").arg(t.lang);
        if (parts.isEmpty()) return tr("Pista %1").arg(t.id);
        return parts.join(QStringLiteral(" "));
    };
    auto clearLayout = [](QVBoxLayout* lay) {
        QLayoutItem* it;
        while ((it = lay->takeAt(0)) != nullptr) {
            if (auto* w = it->widget()) w->deleteLater();
            delete it;
        }
    };

    clearLayout(m_audioPanelList);
    for (const auto& t : audios) {
        auto* btn = new QPushButton(trackLabel(t), m_audioPanel);
        btn->setCheckable(true);
        btn->setChecked(t.selected);
        const int id = t.id;
        connect(btn, &QPushButton::clicked, this, [this, id]() {
            m_player->setAudioTrack(id);
            slideDownPanel(m_audioPanel);
        });
        m_audioPanelList->addWidget(btn);
    }
    m_audioBtn->setVisible(audios.size() > 1);

    clearLayout(m_subPanelList);
    bool subActive = false;
    for (const auto& t : subs) if (t.selected) { subActive = true; break; }
    auto* off = new QPushButton(tr("Sin subtítulos"), m_subPanel);
    off->setCheckable(true);
    off->setChecked(!subActive);
    connect(off, &QPushButton::clicked, this, [this]() {
        m_player->setSubtitleTrack(0);
        slideDownPanel(m_subPanel);
    });
    m_subPanelList->addWidget(off);
    for (const auto& t : subs) {
        auto* btn = new QPushButton(trackLabel(t), m_subPanel);
        btn->setCheckable(true);
        btn->setChecked(t.selected);
        const int id = t.id;
        connect(btn, &QPushButton::clicked, this, [this, id]() {
            m_player->setSubtitleTrack(id);
            slideDownPanel(m_subPanel);
        });
        m_subPanelList->addWidget(btn);
    }
    m_subBtn->setVisible(!subs.isEmpty());
}

void MainWindow::applyOscMode(bool useNative) {
    if (m_player) m_player->setOscEnabled(useNative);
    // Switching modes resets any in-flight auto-hide fade so the bar shows
    // up fully opaque the next time custom controls become active.
    if (m_controlBarFade) m_controlBarFade->stop();
    if (m_controlBarOpacity) m_controlBarOpacity->setOpacity(1.0);
    if (m_controlBar) m_controlBar->setVisible(!useNative);
    if (m_oscToggleAct) m_oscToggleAct->setChecked(useNative);
    QSettings().setValue(QStringLiteral("useNativeOsc"), useNative);
    if (!useNative && m_autoHideEnabled && m_autoHideTimer)
        m_autoHideTimer->start();
}

void MainWindow::setAutoHideEnabled(bool on) {
    if (m_autoHideEnabled == on) return;
    m_autoHideEnabled = on;
    QSettings().setValue(QStringLiteral("autoHideControls"), on);
    if (m_autoHideBtn && m_autoHideBtn->isChecked() != on)
        m_autoHideBtn->setChecked(on);

    auto* layout = qobject_cast<QVBoxLayout*>(m_playerPage->layout());
    if (!layout || !m_controlBar) return;

    if (on) {
        // Detach from layout and overlay on top of the video. Style gives the
        // bar an opaque dark backdrop so its widgets stay readable above the
        // moving video frame.
        if (m_controlBarFade) m_controlBarFade->stop();
        if (m_controlBarOpacity) m_controlBarOpacity->setOpacity(1.0);
        layout->removeWidget(m_controlBar);
        m_controlBar->setStyleSheet(QStringLiteral(
            "QWidget#playerControlBar {"
            "  background-color: rgba(40, 44, 50, 200);"
            "  border-top: 1px solid rgba(255, 255, 255, 30);"
            "}"));
        m_controlBar->raise();
        positionControlBarOverlay();
        const bool nativeOsc = m_oscToggleAct && m_oscToggleAct->isChecked();
        m_controlBar->setVisible(!nativeOsc);
        if (!nativeOsc && m_autoHideTimer) m_autoHideTimer->start();
    } else {
        if (m_controlBarFade) m_controlBarFade->stop();
        if (m_controlBarOpacity) m_controlBarOpacity->setOpacity(1.0);
        m_controlBar->setStyleSheet(QString());
        layout->addWidget(m_controlBar);
        const bool nativeOsc = m_oscToggleAct && m_oscToggleAct->isChecked();
        m_controlBar->setVisible(!nativeOsc);
        if (m_autoHideTimer) m_autoHideTimer->stop();
    }
    applyControlBarIcons();
}

void MainWindow::positionControlBarOverlay() {
    if (!m_autoHideEnabled || !m_controlBar || !m_playerPage) return;
    const int w = m_playerPage->width();
    const int h = m_controlBar->sizeHint().height();
    m_controlBar->setGeometry(0, m_playerPage->height() - h, w, h);
}

void MainWindow::showControlBar() {
    if (!m_autoHideEnabled || !m_controlBar || !m_controlBarOpacity) return;
    if (m_oscToggleAct && m_oscToggleAct->isChecked()) return;
    if (!m_controlBar->isVisible()) m_controlBar->setVisible(true);
    m_controlBar->raise();
    if (m_controlBarFade) {
        m_controlBarFade->stop();
        m_controlBarFade->setStartValue(m_controlBarOpacity->opacity());
        m_controlBarFade->setEndValue(1.0);
        m_controlBarFade->start();
    }
    if (m_autoHideTimer) m_autoHideTimer->start();
}

void MainWindow::hideControlBar() {
    if (!m_autoHideEnabled || !m_controlBar || !m_controlBarOpacity) return;
    if (!m_controlBar->isVisible()) return;
    // Postpone the hide while the cursor is still resting over the bar —
    // child widgets without mouse tracking don't deliver MouseMove, so we
    // recheck position here instead of relying on continuous events.
    const QRect rect(m_controlBar->mapToGlobal(QPoint(0, 0)),
                     m_controlBar->size());
    if (rect.contains(QCursor::pos())) {
        if (m_autoHideTimer) m_autoHideTimer->start();
        return;
    }
    if (m_controlBarFade) {
        m_controlBarFade->stop();
        m_controlBarFade->setStartValue(m_controlBarOpacity->opacity());
        m_controlBarFade->setEndValue(0.0);
        m_controlBarFade->start();
    }
}

void MainWindow::applyControlBarIcons() {
    auto themed = [this](QIcon dark) {
        if (!m_autoHideEnabled) return dark;
        return tintedIcon(dark, QColor("#eff0f1"));
    };
    if (m_seekBackBtn) m_seekBackBtn->setIcon(themed(iconSeekBack()));
    if (m_seekFwdBtn)  m_seekFwdBtn->setIcon(themed(iconSeekFwd()));
    if (m_volumeIconBtn) {
        const bool muted = m_player && m_player->isMuted();
        m_volumeIconBtn->setIcon(themed(muted ? iconVolumeMuted()
                                              : iconVolume()));
    }
    if (m_audioBtn)    m_audioBtn->setIcon(themed(iconAudio()));
    if (m_subBtn)      m_subBtn->setIcon(themed(iconSubs()));
    if (m_aspectBtn)   m_aspectBtn->setIcon(themed(iconAspect()));
    if (m_autoHideBtn) m_autoHideBtn->setIcon(themed(iconAutoHide()));
    if (m_fullscreenBtn) m_fullscreenBtn->setIcon(themed(iconFullscreen()));
    if (m_playPauseBtn && m_player)
        m_playPauseBtn->setIcon(themed(m_player->isPaused() ? iconPlay()
                                                            : iconPause()));
}

bool MainWindow::isInControlBarTree(QObject* w) const {
    if (!m_controlBar) return false;
    QWidget* c = qobject_cast<QWidget*>(w);
    while (c) {
        if (c == m_controlBar) return true;
        c = c->parentWidget();
    }
    return false;
}

QString MainWindow::hms(qint64 s) {
    if (s < 0) s = 0;
    return QString("%1:%2:%3")
        .arg(s / 3600, 2, 10, QChar('0'))
        .arg((s % 3600) / 60, 2, 10, QChar('0'))
        .arg(s % 60, 2, 10, QChar('0'));
}

void MainWindow::rebuildAccountsMenu() {
    if (!m_accountsMenu) return;
    m_accountsMenu->clear();
    auto& store = AccountStore::instance();
    const QString currentId = store.currentAccountId();
    const auto accs = store.accounts();
    if (accs.isEmpty()) {
        auto* a = m_accountsMenu->addAction(tr("(sin cuentas)"));
        a->setEnabled(false);
    } else {
        for (const auto& acc : accs) {
            const ServerEntry srv = store.server(acc.serverId);
            const QString srvLabel = srv.name.isEmpty() ? srv.url.host()
                                                        : srv.name;
            const QString text = srvLabel.isEmpty()
                                     ? acc.username
                                     : QStringLiteral("%1  ·  %2")
                                           .arg(acc.username, srvLabel);
            auto* a = m_accountsMenu->addAction(text);
            a->setCheckable(true);
            a->setChecked(acc.id == currentId);
            const QString id = acc.id;
            connect(a, &QAction::triggered, this,
                    [this, id]() { switchToAccount(id); });
        }
    }
    m_accountsMenu->addSeparator();
    auto* manage = m_accountsMenu->addAction(tr("Gestionar cuentas…"));
    connect(manage, &QAction::triggered, this, &MainWindow::openSettings);
}

void MainWindow::switchToAccount(const QString& accountId) {
    auto& store = AccountStore::instance();
    const AccountEntry acc = store.account(accountId);
    if (acc.id.isEmpty()) return;
    const ServerEntry srv = store.server(acc.serverId);
    if (srv.id.isEmpty()) return;
    if (acc.id == store.currentAccountId()) return;   // already active

    // Stop any playback and report the final position before swapping the
    // network identity, otherwise the report would land on the new server.
    if (m_playing && !m_currentItem.id.isEmpty()) {
        m_client->reportPlaybackStopped(
            m_currentItem.id, m_currentPlayback,
            m_player->positionSeconds() * TICKS_PER_SECOND);
        if (m_currentPlayback.method == JellyfinPlayback::Transcode)
            m_client->stopActiveEncoding(m_currentPlayback.playSessionId);
        m_player->stop();
        m_playing = false;
        m_currentItem = {};
        m_currentPlayback = {};
        m_progressTimer->stop();
    }
    if (m_stack && m_stack->currentWidget() == m_playerPage)
        m_stack->setCurrentWidget(m_browser);

    m_client->setServer(srv.url);
    m_client->setCredentials(acc.userId, acc.token);
    store.setCurrentAccountId(acc.id);
    m_browser->start();
    statusBar()->showMessage(
        tr("Conectado a %1 como %2")
            .arg(srv.name.isEmpty() ? srv.url.host() : srv.name, acc.username));
}

void MainWindow::toggleFullscreen() {
    if (m_fullscreenAct) m_fullscreenAct->toggle();
    else if (isFullScreen()) showNormal(); else showFullScreen();
}

void MainWindow::exitFullscreen() {
    if (!isFullScreen()) return;
    if (m_fullscreenAct) m_fullscreenAct->setChecked(false);
    else showNormal();
}

void MainWindow::onPlayWithStart(const JellyfinItem& item, qint64 startTicks) {
    m_currentItem = item;
    m_currentPlayback = {};
    m_pendingStartTicks = startTicks;
    m_lastReportedPositionSeconds = -1;
    // Negotiate with the server first; the actual mpv->play() happens once
    // PlaybackInfo answers (see onPlaybackResolved).
    statusBar()->showMessage(tr("Preparando reproducción…"));
    m_client->resolvePlayback(item.id, startTicks);
}

void MainWindow::onPlaybackResolved(const QString& itemId,
                                    const JellyfinPlayback& info) {
    if (m_currentItem.id != itemId) return;   // stale resolution
    m_currentPlayback = info;
    const qint64 startSeconds = m_pendingStartTicks / TICKS_PER_SECOND;
    m_stack->setCurrentWidget(m_playerPage);
    m_player->setFocus();
    m_playing = true;
    m_client->reportPlaybackStart(itemId, info, m_pendingStartTicks);
    m_player->play(info.url.toString(), startSeconds);
    m_progressTimer->start();
    if (m_autoHideEnabled) {
        positionControlBarOverlay();
        showControlBar();
    }
    QString modeLabel;
    switch (info.method) {
        case JellyfinPlayback::DirectPlay:   modeLabel = tr("reproducción directa"); break;
        case JellyfinPlayback::DirectStream: modeLabel = tr("remuxeo en servidor"); break;
        case JellyfinPlayback::Transcode:    modeLabel = tr("transcodificación"); break;
    }
    statusBar()->showMessage(tr("Reproduciendo (%1)").arg(modeLabel));
}

void MainWindow::onPlaybackResolveFailed(const QString& itemId,
                                          const QString& message) {
    if (m_currentItem.id != itemId) return;
    statusBar()->showMessage(
        tr("No se pudo iniciar la reproducción: %1").arg(message));
    m_playing = false;
    m_currentItem = {};
    m_currentPlayback = {};
}


void MainWindow::onMpvPosition(qint64 seconds) {
    m_lastReportedPositionSeconds = seconds;
}

void MainWindow::onMpvPaused(bool /*paused*/) {
    // Report immediately on pause/resume so the server reflects state.
    if (m_playing && !m_currentItem.id.isEmpty()) {
        m_client->reportPlaybackProgress(
            m_currentItem.id, m_currentPlayback,
            m_player->positionSeconds() * TICKS_PER_SECOND,
            m_player->isPaused());
    }
}

void MainWindow::reportProgressTick() {
    if (!m_playing || m_currentItem.id.isEmpty()) return;
    m_client->reportPlaybackProgress(
        m_currentItem.id, m_currentPlayback,
        m_player->positionSeconds() * TICKS_PER_SECOND,
        m_player->isPaused());
}

void MainWindow::onMpvIdleChanged(bool idle) {
    if (idle && m_playing) switchToBrowser();
}

void MainWindow::onMpvEndReached() {
    if (m_playing) switchToBrowser();
}

void MainWindow::switchToBrowser() {
    if (m_playing && !m_currentItem.id.isEmpty()) {
        m_client->reportPlaybackStopped(
            m_currentItem.id, m_currentPlayback,
            m_player->positionSeconds() * TICKS_PER_SECOND);
        if (m_currentPlayback.method == JellyfinPlayback::Transcode)
            m_client->stopActiveEncoding(m_currentPlayback.playSessionId);
    }
    m_player->stop();
    m_progressTimer->stop();
    m_playing = false;
    m_currentItem = {};
    m_currentPlayback = {};
    if (m_autoHideTimer) m_autoHideTimer->stop();
    m_stack->setCurrentWidget(m_browser);
    // Refresh the current view so updated resume positions / watched flags appear,
    // without losing the user's navigation stack.
    m_browser->refreshCurrent();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    // Reposition the overlay when the player page resizes.
    if (m_autoHideEnabled && watched == m_playerPage &&
        event->type() == QEvent::Resize) {
        positionControlBarOverlay();
        return false;
    }

    // Drive the auto-hide animation from mouse activity while the player
    // page is on top.
    if (m_autoHideEnabled && event->type() == QEvent::MouseMove &&
        m_stack && m_stack->currentWidget() == m_playerPage) {
        if (isInControlBarTree(watched)) {
            // Anything inside the bar (or its child widgets) keeps it shown.
            showControlBar();
        } else if (watched == m_player) {
            auto* mev = static_cast<QMouseEvent*>(event);
            const int yInPage = m_player->y() + int(mev->position().y());
            const int distFromBottom = m_playerPage->height() - yInPage;
            constexpr int kProximity = 80;
            if (distFromBottom <= kProximity) {
                showControlBar();
            } else if (m_controlBar->isVisible() && m_autoHideTimer &&
                       !m_autoHideTimer->isActive()) {
                m_autoHideTimer->start();
            }
        }
    }

    if (event->type() != QEvent::MouseButtonPress) return false;
    auto* me = static_cast<QMouseEvent*>(event);
    const QPoint gp = me->globalPosition().toPoint();

    auto closeIfOutside = [&](QFrame* panel, QToolButton* btn) {
        if (!panel || !panel->isVisible()) return;
        const QRect panelRect(panel->mapToGlobal(QPoint(0, 0)), panel->size());
        if (panelRect.contains(gp)) return;
        if (btn) {
            const QRect btnRect(btn->mapToGlobal(QPoint(0, 0)), btn->size());
            // Skip closing here when the click is on the toggle button —
            // its own slot will handle the dismiss.
            if (btnRect.contains(gp)) return;
        }
        slideDownPanel(panel);
    };
    closeIfOutside(m_audioPanel, m_audioBtn);
    closeIfOutside(m_subPanel, m_subBtn);
    return false;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (m_playing && !m_currentItem.id.isEmpty()) {
        m_client->reportPlaybackStopped(
            m_currentItem.id, m_currentPlayback,
            m_player->positionSeconds() * TICKS_PER_SECOND);
        if (m_currentPlayback.method == JellyfinPlayback::Transcode)
            m_client->stopActiveEncoding(m_currentPlayback.playSessionId);
    }
    m_player->stop();
    e->accept();
}
