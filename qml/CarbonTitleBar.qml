import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami

// Custom client-side window decoration. Sits in Kirigami.ApplicationWindow's
// `header` slot and replaces the missing system titlebar:
//   - app icon + title
//   - drag region (calls QQuickWindow::startSystemMove on press, which
//     hands the move to xdg-shell on Wayland and _NET_WM_MOVERESIZE on X11)
//   - double-click toggles maximize/restore
//   - minimize, maximize/restore and close buttons drawn from text glyphs
//     (avoids relying on icon-theme names for the window-control trio,
//     which aren't always present on non-KDE setups)
Item {
    id: titleBar

    property string title: ""

    implicitHeight: Kirigami.Units.gridUnit * 2

    // Carbon backdrop with a thin blue separator at the bottom.
    Rectangle {
        anchors.fill: parent
        color: applicationWindow().jelly.carbonAlt
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: applicationWindow().jelly.glassBorder
    }

    // Carbon weave overlay — same SVG as the window background but a touch
    // more visible on the bar so it reads as actual fabric.
    Image {
        anchors.fill: parent
        opacity: 0.7
        fillMode: Image.Tile
        smooth: false
        source: "data:image/svg+xml;utf8,"
            + "<svg xmlns='http://www.w3.org/2000/svg' width='6' height='6'>"
            + "<rect width='3' height='3' fill='%23ffffff' fill-opacity='0.025'/>"
            + "<rect x='3' y='3' width='3' height='3' fill='%23ffffff' fill-opacity='0.025'/>"
            + "<rect x='3' width='3' height='3' fill='%23000000' fill-opacity='0.18'/>"
            + "<rect y='3' width='3' height='3' fill='%23000000' fill-opacity='0.18'/>"
            + "</svg>"
    }

    // Drag region: covers the whole bar but the controls Row sits on top
    // (later in z-order) and intercepts clicks where it needs to.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: applicationWindow().startSystemMove()
        onDoubleClicked: titleBar.toggleMaximize()
    }

    function toggleMaximize() {
        var w = applicationWindow();
        if (w.visibility === Window.Maximized || w.visibility === Window.FullScreen)
            w.showNormal();
        else
            w.showMaximized();
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Image {
            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
            Layout.leftMargin: Kirigami.Units.smallSpacing
            source: "qrc:/icon.png"
            mipmap: true
            smooth: true
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            text: titleBar.title
            color: applicationWindow().jelly.textPrimary
            elide: Text.ElideRight
            font.bold: true
            font.pointSize: Kirigami.Theme.defaultFont.pointSize
        }

        // Window-control trio. Each button intercepts mouse events so the
        // surrounding drag MouseArea doesn't try to start a system move
        // when the user just wants to click a button.
        Repeater {
            model: [
                {act: "min",   glyph: "–",   tip: qsTr("Minimizar")},
                {act: "max",   glyph: "□",   tip: qsTr("Maximizar/Restaurar")},
                {act: "close", glyph: "✕",   tip: qsTr("Cerrar")},
            ]
            delegate: Button {
                id: ctrl
                Layout.preferredWidth: Kirigami.Units.gridUnit * 2.5
                Layout.fillHeight: true
                flat: true
                hoverEnabled: true
                focusPolicy: Qt.NoFocus
                ToolTip.visible: hovered
                ToolTip.text: modelData.tip
                background: Rectangle {
                    color: {
                        if (!ctrl.hovered) return "transparent";
                        return modelData.act === "close"
                            ? "#a83232"
                            : applicationWindow().jelly.glassHover;
                    }
                    border.width: ctrl.hovered ? 1 : 0
                    border.color: applicationWindow().jelly.glassBorderHot
                    radius: 0
                }
                contentItem: Label {
                    text: modelData.glyph
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: applicationWindow().jelly.textPrimary
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize + 2
                }
                onClicked: {
                    var w = applicationWindow();
                    if (modelData.act === "min")        w.showMinimized();
                    else if (modelData.act === "max")   titleBar.toggleMaximize();
                    else                                w.close();
                }
            }
        }
    }
}
