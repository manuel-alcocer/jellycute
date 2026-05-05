#include "JellyfinClient.h"
#include "LoginDialog.h"
#include "MainWindow.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QUrl>
#include <locale>

int main(int argc, char** argv) {
    // Ask for a GL 3.3 (compatibility) context for the QOpenGLWidget that hosts
    // mpv's render API. Qt defaults to 2.0, which lacks features mpv probes
    // and produces "OpenGL error INVALID_ENUM after creating texture" noise.
    {
        QSurfaceFormat fmt;
        fmt.setVersion(3, 3);
        fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
        fmt.setRenderableType(QSurfaceFormat::OpenGL);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

    QApplication app(argc, argv);

    // libmpv requires the C numeric locale. QApplication's constructor
    // resets it to the user's locale, so this must run *after* it.
    std::setlocale(LC_NUMERIC, "C");

    QCoreApplication::setOrganizationName("jellycute");
    QCoreApplication::setOrganizationDomain("jellycute.local");
    QCoreApplication::setApplicationName("jellycute");

    // Dolphin-like stylesheet using KDE Breeze colors.
    //   Window  : #eff0f1   Base    : #fcfcfc   Highlight: #3daee9
    //   Sidebar : #f3f3f4   Border  : #d0d2d4   Disabled : #a0a4a7
    qApp->setStyleSheet(QStringLiteral(R"(
        QMainWindow, QStatusBar { background: #eff0f1; color: #232629; }

        /* Sidebar: panel with subtle gray border. */
        QWidget#sidebar {
            background-color: #f3f3f4;
            border: 1px solid #d0d2d4;
            border-radius: 4px;
        }

        /* Content area: framed with the active-view blue line.
           Background is intentionally a slightly darker off-white than the
           cards inside, so each section reads as a paper card on a tray. */
        QStackedWidget#contentPanel {
            background-color: #f0f1f2;
            border: 1px solid #3daee9;
            border-radius: 4px;
        }

        /* Each Home section sits in its own white card with a thin border. */
        QWidget#mediaRow,
        QFrame#contentCard,
        QFrame#detailCard {
            background-color: #fcfcfc;
            border: 1px solid #d0d2d4;
            border-radius: 6px;
        }

        /* Slide-over panels (alphabet filter and audio/sub selectors). */
        QFrame#letterPanel,
        QFrame#trackPanel {
            background-color: #fcfcfc;
            border: 1px solid #d0d2d4;
            border-radius: 8px;
        }

        /* Inner views inherit the page background. */
        QListWidget, QListView, QScrollArea, QAbstractScrollArea {
            background: transparent;
            border: none;
            outline: none;
        }
        QAbstractScrollArea > QWidget > QWidget,
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }

        /* List items: tight padding, 4px-rounded selection, soft hover. */
        QListView::item, QListWidget::item {
            border-radius: 4px;
            padding: 5px 10px;
            margin: 1px 4px;
            color: #232629;
        }
        QListView::item:hover, QListWidget::item:hover {
            background: rgba(61,174,233,40);
        }
        QListView::item:selected, QListWidget::item:selected {
            background: #3daee9;
            color: #fcfcfc;
        }
        QListView::item:selected:!active, QListWidget::item:selected:!active {
            background: #93cde7;
            color: #fcfcfc;
        }

        /* Buttons + tool buttons. */
        QPushButton, QToolButton {
            border: 1px solid #bdc3c7;
            border-radius: 4px;
            padding: 5px 14px;
            background: #fcfcfc;
            color: #232629;
        }
        QPushButton:hover, QToolButton:hover {
            background: #f4f4f4;
            border-color: #3daee9;
        }
        QPushButton:pressed, QToolButton:pressed {
            background: #e6e6e6;
        }
        QPushButton:checked, QToolButton:checked {
            background: #3daee9;
            color: #fcfcfc;
            border-color: #3daee9;
        }

        /* Toolbar buttons: borderless by default, sunken inset when checked. */
        QToolBar QToolButton {
            border: 1px solid transparent;
            background: transparent;
            padding: 4px;
            border-radius: 4px;
        }
        QToolBar QToolButton:hover {
            background: rgba(61,174,233,40);
            border-color: rgba(61,174,233,90);
        }
        QToolBar QToolButton:pressed,
        QToolBar QToolButton:checked {
            background: #c8e3f3;
            border: 1px solid #3daee9;
            padding-top: 5px;
            padding-bottom: 3px;
            padding-left: 5px;
            padding-right: 3px;
        }
        QPushButton:disabled, QToolButton:disabled {
            color: #a0a4a7;
            border-color: #d0d2d4;
            background: #f5f6f7;
        }
        QPushButton[flat="true"], QToolButton[autoRaise="true"] {
            border: none;
            background: transparent;
            padding: 4px 8px;
        }
        QPushButton[flat="true"]:hover, QToolButton[autoRaise="true"]:hover {
            background: rgba(61,174,233,40);
        }

        /* Inputs */
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            border: 1px solid #bdc3c7;
            border-radius: 4px;
            padding: 5px 8px;
            background: #fcfcfc;
            color: #232629;
            selection-background-color: #3daee9;
            selection-color: #fcfcfc;
        }
        QLineEdit:focus, QComboBox:focus,
        QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: #3daee9;
        }

        /* Discreet scrollbars. */
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 4px 2px;
        }
        QScrollBar::handle:vertical {
            background: #b0b4b7;
            min-height: 30px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover { background: #3daee9; }
        QScrollBar:horizontal {
            background: transparent;
            height: 10px;
            margin: 2px 4px;
        }
        QScrollBar::handle:horizontal {
            background: #b0b4b7;
            min-width: 30px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover { background: #3daee9; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

        /* Menu bar + popups. */
        QMenuBar {
            background: #eff0f1;
            color: #232629;
            border-bottom: 1px solid #d0d2d4;
        }
        QMenuBar::item {
            background: transparent;
            padding: 5px 10px;
            border-radius: 4px;
        }
        QMenuBar::item:selected, QMenuBar::item:pressed {
            background: rgba(61,174,233,60);
        }
        QMenu {
            border: 1px solid #d0d2d4;
            border-radius: 6px;
            padding: 4px;
            background: #fcfcfc;
            color: #232629;
        }
        QMenu::item { border-radius: 4px; padding: 5px 18px; }
        QMenu::item:selected { background: #3daee9; color: #fcfcfc; }
        QMenu::separator { height: 1px; background: #d0d2d4; margin: 4px 6px; }

        /* Status bar */
        QStatusBar { border-top: 1px solid #d0d2d4; }
        QStatusBar::item { border: none; }

        QFrame { border: none; }

        /* Tooltips */
        QToolTip {
            border: 1px solid #d0d2d4;
            border-radius: 4px;
            padding: 4px 8px;
            background: #fcfcfc;
            color: #232629;
        }
    )"));

    // Force Oxygen as the only Qt style (with Breeze/Fusion as fallback if
    // it's not installed). Theme switching is no longer user-configurable.
    {
        const QStringList prefer{"Oxygen", "Breeze", "Fusion"};
        for (const QString& s : prefer) {
            if (QStyleFactory::keys().contains(s, Qt::CaseInsensitive)) {
                if (auto* style = QStyleFactory::create(s)) {
                    QApplication::setStyle(style);
                    break;
                }
            }
        }

        QPalette p;
        p.setColor(QPalette::Window,            QColor("#eff0f1"));
        p.setColor(QPalette::WindowText,        QColor("#232629"));
        p.setColor(QPalette::Base,              QColor("#fcfcfc"));
        p.setColor(QPalette::AlternateBase,     QColor("#eff0f1"));
        p.setColor(QPalette::ToolTipBase,       QColor("#fcfcfc"));
        p.setColor(QPalette::ToolTipText,       QColor("#232629"));
        p.setColor(QPalette::Text,              QColor("#232629"));
        p.setColor(QPalette::Button,            QColor("#eff0f1"));
        p.setColor(QPalette::ButtonText,        QColor("#232629"));
        p.setColor(QPalette::BrightText,        QColor("#ffffff"));
        p.setColor(QPalette::Link,              QColor("#2980b9"));
        p.setColor(QPalette::Highlight,         QColor("#3daee9"));
        p.setColor(QPalette::HighlightedText,   QColor("#fcfcfc"));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#bdc3c7"));
        p.setColor(QPalette::Disabled, QPalette::Text,       QColor("#bdc3c7"));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#bdc3c7"));
        p.setColor(QPalette::Disabled, QPalette::Highlight,  QColor("#bdc3c7"));
        QApplication::setPalette(p);
    }

    JellyfinClient client;

    QSettings s;
    const QString srv = s.value("server").toString();
    const QString uid = s.value("userId").toString();
    const QString tok = s.value("token").toString();
    if (!srv.isEmpty() && !uid.isEmpty() && !tok.isEmpty()) {
        client.setServer(QUrl(srv));
        client.setCredentials(uid, tok);
    } else {
        LoginDialog dlg(&client);
        if (dlg.exec() != QDialog::Accepted) return 0;
    }

    MainWindow w(&client);
    w.show();
    return app.exec();
}
