#include "Theme.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>

namespace Theme {
namespace {

Name fromKey(const QString& k) {
    return (k.compare("carbon", Qt::CaseInsensitive) == 0) ? Carbon : Light;
}

QString toKey(Name n) {
    return n == Carbon ? QStringLiteral("carbon") : QStringLiteral("light");
}

QString lightStylesheet() {
    return QStringLiteral(R"(
        QMainWindow, QStatusBar { background: #eff0f1; color: #232629; }

        QWidget#sidebar {
            background-color: #f3f3f4;
            border: 1px solid #d0d2d4;
            border-radius: 4px;
        }

        QStackedWidget#contentPanel {
            background-color: #f0f1f2;
            border: 1px solid #3daee9;
            border-radius: 4px;
        }

        QWidget#mediaRow,
        QFrame#contentCard,
        QFrame#detailCard {
            background-color: #fcfcfc;
            border: 1px solid #d0d2d4;
            border-radius: 6px;
        }

        QFrame#letterPanel,
        QFrame#trackPanel {
            background-color: #fcfcfc;
            border: 1px solid #d0d2d4;
            border-radius: 8px;
        }

        QListWidget, QListView, QScrollArea, QAbstractScrollArea {
            background: transparent;
            border: none;
            outline: none;
        }
        QAbstractScrollArea > QWidget > QWidget,
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }

        QWidget#gridViewport { background-color: #fcfcfc; }

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
        QPushButton:pressed, QToolButton:pressed { background: #e6e6e6; }
        QPushButton:checked, QToolButton:checked {
            background: #3daee9;
            color: #fcfcfc;
            border-color: #3daee9;
        }

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
        QToolBar::separator {
            background: #d0d2d4;
            width: 1px;
            margin: 6px 6px;
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
        QSpinBox:focus, QDoubleSpinBox:focus { border-color: #3daee9; }
        QComboBox::drop-down {
            border: none;
            width: 22px;
            background: transparent;
        }
        QComboBox::down-arrow {
            width: 0;
            height: 0;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #232629;
            margin-right: 6px;
        }
        QComboBox QAbstractItemView {
            background: #fcfcfc;
            border: 1px solid #d0d2d4;
            color: #232629;
            selection-background-color: #3daee9;
            selection-color: #fcfcfc;
            outline: none;
            padding: 2px;
        }

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

        QStatusBar { border-top: 1px solid #d0d2d4; }
        QStatusBar::item { border: none; }

        QFrame { border: none; }

        QToolTip {
            border: 1px solid #d0d2d4;
            border-radius: 4px;
            padding: 4px 8px;
            background: #fcfcfc;
            color: #232629;
        }
    )");
}

QString carbonStylesheet() {
    // Carbon-fibre: deep dark base with subtle vertical gradient on cards to
    // suggest a woven texture, off-white (#e8e8e8) for text/icons, and the
    // same Breeze accent so the brand colour stays consistent.
    return QStringLiteral(R"(
        QMainWindow, QStatusBar { background: #16181b; color: #e8e8e8; }

        QWidget#sidebar {
            background-color: #1c1e22;
            border: 1px solid #303339;
            border-radius: 4px;
        }

        QStackedWidget#contentPanel {
            background-color: #14161a;
            border: 1px solid #3daee9;
            border-radius: 4px;
        }

        QWidget#mediaRow,
        QFrame#contentCard,
        QFrame#detailCard {
            background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                stop: 0 #25272c, stop: 1 #1a1c20);
            border: 1px solid #303339;
            border-radius: 6px;
        }

        QFrame#letterPanel,
        QFrame#trackPanel {
            background-color: #1c1e22;
            border: 1px solid #404349;
            border-radius: 8px;
            color: #e8e8e8;
        }

        QListWidget, QListView, QScrollArea, QAbstractScrollArea {
            background: transparent;
            border: none;
            outline: none;
        }
        QAbstractScrollArea > QWidget > QWidget,
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }

        QWidget#gridViewport { background-color: #1a1c20; }

        QListView::item, QListWidget::item {
            border-radius: 4px;
            padding: 5px 10px;
            margin: 1px 4px;
            color: #e8e8e8;
        }
        QListView::item:hover, QListWidget::item:hover {
            background: rgba(61,174,233,55);
        }
        QListView::item:selected, QListWidget::item:selected {
            background: #3daee9;
            color: #16181b;
        }
        QListView::item:selected:!active, QListWidget::item:selected:!active {
            background: #2c6f8d;
            color: #e8e8e8;
        }

        QPushButton, QToolButton {
            border: 1px solid #404349;
            border-radius: 4px;
            padding: 5px 14px;
            background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,
                stop: 0 #2a2c30, stop: 1 #1f2125);
            color: #e8e8e8;
        }
        QPushButton:hover, QToolButton:hover {
            background: #2f3236;
            border-color: #3daee9;
        }
        QPushButton:pressed, QToolButton:pressed { background: #16181b; }
        QPushButton:checked, QToolButton:checked {
            background: #3daee9;
            color: #16181b;
            border-color: #3daee9;
        }

        QToolBar QToolButton {
            border: 1px solid transparent;
            background: transparent;
            padding: 4px;
            border-radius: 4px;
        }
        QToolBar QToolButton:hover {
            background: rgba(61,174,233,55);
            border-color: rgba(61,174,233,140);
        }
        QToolBar QToolButton:pressed,
        QToolBar QToolButton:checked {
            background: rgba(61,174,233,80);
            border: 1px solid #3daee9;
            padding-top: 5px;
            padding-bottom: 3px;
            padding-left: 5px;
            padding-right: 3px;
        }
        QToolBar::separator {
            background: #404349;
            width: 1px;
            margin: 6px 6px;
        }
        QPushButton:disabled, QToolButton:disabled {
            color: #6c7076;
            border-color: #2a2c30;
            background: #1a1c20;
        }
        QPushButton[flat="true"], QToolButton[autoRaise="true"] {
            border: none;
            background: transparent;
            padding: 4px 8px;
            color: #e8e8e8;
        }
        QPushButton[flat="true"]:hover, QToolButton[autoRaise="true"]:hover {
            background: rgba(61,174,233,55);
        }

        QLabel { color: #e8e8e8; }

        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
            border: 1px solid #404349;
            border-radius: 4px;
            padding: 5px 8px;
            background: #1d1f23;
            color: #e8e8e8;
            selection-background-color: #3daee9;
            selection-color: #16181b;
        }
        QLineEdit:focus, QComboBox:focus,
        QSpinBox:focus, QDoubleSpinBox:focus { border-color: #3daee9; }
        QComboBox::drop-down {
            border: none;
            width: 22px;
            background: transparent;
        }
        QComboBox::down-arrow {
            width: 0;
            height: 0;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #e8e8e8;
            margin-right: 6px;
        }
        QComboBox QAbstractItemView {
            background: #1d1f23;
            border: 1px solid #404349;
            color: #e8e8e8;
            selection-background-color: #3daee9;
            selection-color: #16181b;
            outline: none;
            padding: 2px;
        }

        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 4px 2px;
        }
        QScrollBar::handle:vertical {
            background: #404349;
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
            background: #404349;
            min-width: 30px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover { background: #3daee9; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

        QMenuBar {
            background: #16181b;
            color: #e8e8e8;
            border-bottom: 1px solid #303339;
        }
        QMenuBar::item {
            background: transparent;
            padding: 5px 10px;
            border-radius: 4px;
        }
        QMenuBar::item:selected, QMenuBar::item:pressed {
            background: rgba(61,174,233,80);
        }
        QMenu {
            border: 1px solid #404349;
            border-radius: 6px;
            padding: 4px;
            background: #1d1f23;
            color: #e8e8e8;
        }
        QMenu::item { border-radius: 4px; padding: 5px 18px; }
        QMenu::item:selected { background: #3daee9; color: #16181b; }
        QMenu::separator { height: 1px; background: #404349; margin: 4px 6px; }

        QStatusBar { border-top: 1px solid #303339; color: #e8e8e8; }
        QStatusBar::item { border: none; }

        QFrame { border: none; }

        QToolTip {
            border: 1px solid #404349;
            border-radius: 4px;
            padding: 4px 8px;
            background: #1d1f23;
            color: #e8e8e8;
        }
    )");
}

void applyLightPalette() {
    QPalette p;
    p.setColor(QPalette::Window,            QColor("#eff0f1"));
    p.setColor(QPalette::WindowText,        QColor("#232629"));
    p.setColor(QPalette::Base,              QColor("#fcfcfc"));
    p.setColor(QPalette::AlternateBase,     QColor("#eff0f1"));
    p.setColor(QPalette::ToolTipBase,       QColor("#fcfcfc"));
    p.setColor(QPalette::ToolTipText,       QColor("#232629"));
    p.setColor(QPalette::Text,              QColor("#232629"));
    p.setColor(QPalette::PlaceholderText,   QColor("#5d6164"));
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

void applyCarbonPalette() {
    QPalette p;
    p.setColor(QPalette::Window,            QColor("#16181b"));
    p.setColor(QPalette::WindowText,        QColor("#e8e8e8"));
    p.setColor(QPalette::Base,              QColor("#1d1f23"));
    p.setColor(QPalette::AlternateBase,     QColor("#22252a"));
    p.setColor(QPalette::ToolTipBase,       QColor("#1d1f23"));
    p.setColor(QPalette::ToolTipText,       QColor("#e8e8e8"));
    p.setColor(QPalette::Text,              QColor("#e8e8e8"));
    p.setColor(QPalette::PlaceholderText,   QColor("#9da0a6"));
    p.setColor(QPalette::Button,            QColor("#22252a"));
    p.setColor(QPalette::ButtonText,        QColor("#e8e8e8"));
    p.setColor(QPalette::BrightText,        QColor("#ffffff"));
    p.setColor(QPalette::Link,              QColor("#5fb8e8"));
    p.setColor(QPalette::Highlight,         QColor("#3daee9"));
    p.setColor(QPalette::HighlightedText,   QColor("#16181b"));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#6c7076"));
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor("#6c7076"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#6c7076"));
    p.setColor(QPalette::Disabled, QPalette::Highlight,  QColor("#404349"));
    QApplication::setPalette(p);
}

}  // anonymous

Name current() {
    // Dark ("carbon") is the default for new installs; existing users keep
    // whatever their QSettings already holds.
    return fromKey(QSettings()
                       .value(QStringLiteral("theme"), QStringLiteral("carbon"))
                       .toString());
}

QString currentKey() { return toKey(current()); }

void setCurrent(Name n) {
    QSettings().setValue(QStringLiteral("theme"), toKey(n));
}

void apply() {
    if (current() == Carbon) {
        qApp->setStyleSheet(carbonStylesheet());
        applyCarbonPalette();
    } else {
        qApp->setStyleSheet(lightStylesheet());
        applyLightPalette();
    }
}

QColor foregroundColor() {
    return current() == Carbon ? QColor("#e8e8e8") : QColor("#232629");
}

QColor accentColor() { return QColor("#3daee9"); }

}  // namespace Theme
