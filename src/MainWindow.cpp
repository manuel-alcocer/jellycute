#include "MainWindow.h"
#include "BrowserWidget.h"
#include "LoginDialog.h"
#include "MpvWidget.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr qint64 TICKS_PER_SECOND = 10'000'000LL;

// Monochrome line-art icons rendered with QPainter so the look stays
// consistent regardless of the system icon theme.
QIcon makeIcon(int size, std::function<void(QPainter&)> draw) {
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor("#232629"));
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
        p.setBrush(QColor("#232629"));
        QPolygonF tri;
        tri << QPointF(7, 5) << QPointF(7, 19) << QPointF(19, 12);
        p.drawPolygon(tri);
    });
}
QIcon iconPause() {
    return makeIcon(24, [](QPainter& p) {
        p.setBrush(QColor("#232629"));
        p.drawRoundedRect(QRectF(7, 5, 3.5, 14), 1, 1);
        p.drawRoundedRect(QRectF(13.5, 5, 3.5, 14), 1, 1);
    });
}
QIcon iconSeekBack() {
    return makeIcon(24, [](QPainter& p) {
        p.setBrush(QColor("#232629"));
        QPolygonF t1; t1 << QPointF(11, 6) << QPointF(11, 18) << QPointF(4, 12);
        QPolygonF t2; t2 << QPointF(20, 6) << QPointF(20, 18) << QPointF(13, 12);
        p.drawPolygon(t1); p.drawPolygon(t2);
    });
}
QIcon iconSeekFwd() {
    return makeIcon(24, [](QPainter& p) {
        p.setBrush(QColor("#232629"));
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
}

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

    m_playerBar = new QWidget(m_playerPage);
    auto* barLayout = new QHBoxLayout(m_playerBar);
    barLayout->setContentsMargins(6, 4, 6, 4);
    barLayout->setSpacing(6);
    auto* fsBtn = new QToolButton(m_playerBar);
    fsBtn->setText(tr("⛶ Pantalla completa"));
    fsBtn->setToolTip(tr("Alternar pantalla completa (F)"));
    barLayout->addStretch();
    barLayout->addWidget(fsBtn);

    m_player = new MpvWidget(m_playerPage);
    playerLayout->addWidget(m_playerBar);
    playerLayout->addWidget(m_player, 1);

    buildPlayerControlBar();
    playerLayout->addWidget(m_controlBar);
    buildTrackPanels();

    // Click outside an open track panel closes it.
    qApp->installEventFilter(this);

    m_stack->addWidget(m_browser);
    m_stack->addWidget(m_playerPage);
    setCentralWidget(m_stack);

    connect(fsBtn, &QToolButton::clicked, this, &MainWindow::toggleFullscreen);

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
        if (m_playPauseBtn) m_playPauseBtn->setIcon(p ? iconPlay() : iconPause());
    });
    connect(m_player, &MpvWidget::tracksChanged, this, &MainWindow::rebuildTrackMenus);
    connect(m_player, &MpvWidget::volumeChanged, this, [this](int v) {
        if (m_volumeSlider) {
            QSignalBlocker b(m_volumeSlider);
            m_volumeSlider->setValue(v);
        }
    });
    connect(m_player, &MpvWidget::fullscreenToggleRequested,
            this, &MainWindow::toggleFullscreen);
    connect(m_player, &MpvWidget::fullscreenExitRequested,
            this, &MainWindow::exitFullscreen);
    connect(m_progressTimer, &QTimer::timeout, this, &MainWindow::reportProgressTick);

    buildMenus();
    buildToolBar();

    m_browser->start();
    statusBar()->showMessage(tr("Conectado a %1").arg(client->server().toString()));
}

void MainWindow::buildMenus() {
    // Files: app-level actions (logout, quit).
    auto* fileMenu = menuBar()->addMenu(tr("&Files"));

    auto* logoutAct = fileMenu->addAction(tr("Cerrar sesión"));
    connect(logoutAct, &QAction::triggered, this, [this]() {
        QSettings s;
        s.remove("token");
        s.remove("userId");
        close();
    });

    fileMenu->addSeparator();

    auto* quitAct = fileMenu->addAction(tr("Salir"));
    quitAct->setShortcut(QKeySequence::Quit);
    quitAct->setShortcutContext(Qt::ApplicationShortcut);
    connect(quitAct, &QAction::triggered, this, &QMainWindow::close);

    // Settings: HW decoding, toolbar visibility, fullscreen.
    auto* settingsMenu = menuBar()->addMenu(tr("&Settings"));

    // Library visibility — populated dynamically when views arrive.
    auto* libsMenu = settingsMenu->addMenu(tr("Bibliotecas"));
    auto rebuildLibsMenu = [this, libsMenu]() {
        libsMenu->clear();
        const auto libs = m_browser->allLibraries();
        if (libs.isEmpty()) {
            auto* a = libsMenu->addAction(tr("(sin bibliotecas)"));
            a->setEnabled(false);
            return;
        }
        for (const auto& lib : libs) {
            auto* a = libsMenu->addAction(lib.name);
            a->setCheckable(true);
            a->setChecked(!m_browser->isLibraryHidden(lib.id));
            const QString id = lib.id;
            connect(a, &QAction::toggled, this, [this, id](bool show) {
                m_browser->setLibraryHidden(id, !show);
            });
        }
    };
    rebuildLibsMenu();
    connect(m_browser, &BrowserWidget::librariesChanged,
            this, [rebuildLibsMenu](const QList<JellyfinItem>&) { rebuildLibsMenu(); });

    auto* hwMenu = settingsMenu->addMenu(tr("Decodificación de hardware"));
    auto* hwGroup = new QActionGroup(this);
    hwGroup->setExclusive(true);
    const QString detected = m_player->detectedDefaultHwdec();
    auto* autoAct = hwMenu->addAction(tr("Automático (detectado: %1)").arg(detected));
    autoAct->setCheckable(true);
    autoAct->setData(QString());
    hwGroup->addAction(autoAct);
    hwMenu->addSeparator();
    for (const QString& c : m_player->availableHwdecChoices()) {
        auto* a = hwMenu->addAction(c);
        a->setCheckable(true);
        a->setData(c);
        hwGroup->addAction(a);
    }
    const QString currentHwdec = m_player->hwdecSetting();
    for (auto* a : hwGroup->actions()) {
        if (a->data().toString() == currentHwdec) { a->setChecked(true); break; }
    }
    if (!hwGroup->checkedAction()) autoAct->setChecked(true);
    connect(hwGroup, &QActionGroup::triggered, this, [this](QAction* a) {
        m_player->setHwdecSetting(a->data().toString());
        statusBar()->showMessage(
            tr("Decodificación HW: %1").arg(m_player->resolvedHwdec()), 4000);
    });

    settingsMenu->addSeparator();

    m_toolBarToggleAct = settingsMenu->addAction(tr("Mostrar barra de herramientas"));
    m_toolBarToggleAct->setCheckable(true);
    const bool tbVisible = QSettings().value(QStringLiteral("toolbarVisible"), true).toBool();
    m_toolBarToggleAct->setChecked(tbVisible);
    connect(m_toolBarToggleAct, &QAction::toggled, this, [this](bool on) {
        if (m_toolBar) m_toolBar->setVisible(on);
        QSettings().setValue(QStringLiteral("toolbarVisible"), on);
    });

    m_fullscreenAct = settingsMenu->addAction(tr("Pantalla completa"));
    m_fullscreenAct->setShortcut(QKeySequence(Qt::Key_F11));
    m_fullscreenAct->setShortcutContext(Qt::ApplicationShortcut);
    m_fullscreenAct->setCheckable(true);
    connect(m_fullscreenAct, &QAction::toggled, this, [this](bool on) {
        if (on == isFullScreen() && menuBar()->isHidden() == on) return;
        menuBar()->setVisible(!on);
        statusBar()->setVisible(!on);
        if (m_playerBar) m_playerBar->setVisible(!on);
        if (m_toolBar && m_toolBarToggleAct && m_toolBarToggleAct->isChecked())
            m_toolBar->setVisible(!on);
        if (on) showFullScreen(); else showNormal();
        if (on) m_player->setFocus();
    });
}

void MainWindow::buildToolBar() {
    m_toolBar = new QToolBar(tr("Herramientas"), this);
    m_toolBar->setObjectName(QStringLiteral("mainToolbar"));
    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(20, 20));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    addToolBar(Qt::TopToolBarArea, m_toolBar);

    auto* sidebarAct = m_toolBar->addAction(iconSidebar(),
                                            tr("Ocultar/mostrar sidebar"));
    connect(sidebarAct, &QAction::triggered, this, [this]() {
        m_browser->setSidebarVisible(!m_browser->isSidebarVisible());
    });

    auto* backAct = m_toolBar->addAction(iconBack(), tr("Atrás"));
    connect(backAct, &QAction::triggered, this, [this]() {
        // From the player → stop and go back to the browser. Otherwise pop
        // one breadcrumb level inside the browser.
        if (m_stack && m_stack->currentWidget() == m_playerPage)
            switchToBrowser();
        else
            m_browser->goBack();
    });

    m_toolBar->addSeparator();

    auto* changeUserAct = m_toolBar->addAction(iconUsers(), tr("Cambiar usuario"));
    connect(changeUserAct, &QAction::triggered, this, &MainWindow::changeUser);

    auto* logoutAct = m_toolBar->addAction(iconLogout(), tr("Cerrar sesión"));
    connect(logoutAct, &QAction::triggered, this, [this]() {
        QSettings s;
        s.remove("token");
        s.remove("userId");
        close();
    });

    m_toolBar->addSeparator();

    if (m_fullscreenAct) {
        m_fullscreenAct->setIcon(iconFullscreen());
        m_toolBar->addAction(m_fullscreenAct);
    }

    m_oscToggleAct = m_toolBar->addAction(iconOsc(),
                                          tr("Alternar OSC mpv / controles propios"));
    m_oscToggleAct->setCheckable(true);
    const bool useNativeOsc =
        QSettings().value(QStringLiteral("useNativeOsc"), true).toBool();
    m_oscToggleAct->setChecked(useNativeOsc);
    connect(m_oscToggleAct, &QAction::toggled, this,
            [this](bool on) { applyOscMode(on); });
    applyOscMode(useNativeOsc);

    const bool show = QSettings().value(QStringLiteral("toolbarVisible"), true).toBool();
    m_toolBar->setVisible(show);
    if (m_toolBarToggleAct) m_toolBarToggleAct->setChecked(show);
}

void MainWindow::buildPlayerControlBar() {
    m_controlBar = new QWidget(m_playerPage);
    auto* cb = new QHBoxLayout(m_controlBar);
    cb->setContentsMargins(8, 4, 8, 8);
    cb->setSpacing(8);

    auto* seekBack = new QToolButton(m_controlBar);
    seekBack->setIcon(iconSeekBack());
    seekBack->setToolTip(tr("Atrás 10s"));
    seekBack->setAutoRaise(true);

    m_playPauseBtn = new QToolButton(m_controlBar);
    m_playPauseBtn->setIcon(iconPause());
    m_playPauseBtn->setToolTip(tr("Reproducir / Pausa"));
    m_playPauseBtn->setAutoRaise(true);

    auto* seekFwd = new QToolButton(m_controlBar);
    seekFwd->setIcon(iconSeekFwd());
    seekFwd->setToolTip(tr("Adelantar 10s"));
    seekFwd->setAutoRaise(true);

    m_positionSlider = new QSlider(Qt::Horizontal, m_controlBar);
    m_positionSlider->setRange(0, 0);
    m_positionSlider->setSingleStep(5);
    m_positionSlider->setPageStep(60);

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

    auto* volIcon = new QToolButton(m_controlBar);
    volIcon->setIcon(iconVolume());
    volIcon->setAutoRaise(true);
    volIcon->setEnabled(false);   // pure indicator, no click action.

    m_volumeSlider = new QSlider(Qt::Horizontal, m_controlBar);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(90);
    m_volumeSlider->setToolTip(tr("Volumen"));
    connect(m_volumeSlider, &QSlider::valueChanged, this,
            [this](int v) { m_player->setVolume(v); });

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

    cb->addWidget(seekBack);
    cb->addWidget(m_playPauseBtn);
    cb->addWidget(seekFwd);
    cb->addWidget(m_positionSlider, 1);
    cb->addWidget(volIcon);
    cb->addWidget(m_volumeSlider);
    cb->addWidget(m_audioBtn);
    cb->addWidget(m_subBtn);
    cb->addWidget(m_aspectBtn);
    cb->addWidget(m_posLabel);
    cb->addWidget(sep);
    cb->addWidget(m_durLabel);

    connect(seekBack, &QToolButton::clicked, this,
            [this]() { m_player->seekRelative(-10); });
    connect(seekFwd, &QToolButton::clicked, this,
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
    if (m_controlBar) m_controlBar->setVisible(!useNative);
    if (m_oscToggleAct) m_oscToggleAct->setChecked(useNative);
    QSettings().setValue(QStringLiteral("useNativeOsc"), useNative);
}

QString MainWindow::hms(qint64 s) {
    if (s < 0) s = 0;
    return QString("%1:%2:%3")
        .arg(s / 3600, 2, 10, QChar('0'))
        .arg((s % 3600) / 60, 2, 10, QChar('0'))
        .arg(s % 60, 2, 10, QChar('0'));
}

void MainWindow::changeUser() {
    if (m_playing && !m_currentItem.id.isEmpty()) {
        m_client->reportPlaybackStopped(
            m_currentItem.id,
            m_player->positionSeconds() * TICKS_PER_SECOND);
        m_player->stop();
        m_playing = false;
        m_currentItem = {};
        m_progressTimer->stop();
        m_stack->setCurrentWidget(m_browser);
    }
    LoginDialog dlg(m_client, this);
    if (dlg.exec() == QDialog::Accepted) {
        // LoginDialog already updated JellyfinClient credentials and saved
        // them to QSettings; just refresh the views.
        m_browser->start();
    }
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
    const qint64 startSeconds = startTicks / TICKS_PER_SECOND;
    m_lastReportedPositionSeconds = -1;
    m_stack->setCurrentWidget(m_playerPage);
    m_player->setFocus();
    m_playing = true;
    m_client->reportPlaybackStart(item.id, startTicks);
    m_player->play(m_client->streamUrl(item.id).toString(), startSeconds);
    m_progressTimer->start();
}


void MainWindow::onMpvPosition(qint64 seconds) {
    m_lastReportedPositionSeconds = seconds;
}

void MainWindow::onMpvPaused(bool /*paused*/) {
    // Report immediately on pause/resume so the server reflects state.
    if (m_playing && !m_currentItem.id.isEmpty()) {
        m_client->reportPlaybackProgress(
            m_currentItem.id,
            m_player->positionSeconds() * TICKS_PER_SECOND,
            m_player->isPaused());
    }
}

void MainWindow::reportProgressTick() {
    if (!m_playing || m_currentItem.id.isEmpty()) return;
    m_client->reportPlaybackProgress(
        m_currentItem.id,
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
            m_currentItem.id,
            m_player->positionSeconds() * TICKS_PER_SECOND);
    }
    m_player->stop();
    m_progressTimer->stop();
    m_playing = false;
    m_currentItem = {};
    m_stack->setCurrentWidget(m_browser);
    // Refresh the current view so updated resume positions / watched flags appear,
    // without losing the user's navigation stack.
    m_browser->refreshCurrent();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    Q_UNUSED(watched);
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
            m_currentItem.id,
            m_player->positionSeconds() * TICKS_PER_SECOND);
    }
    m_player->stop();
    e->accept();
}
