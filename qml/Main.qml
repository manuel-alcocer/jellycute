import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: qsTr("jellycute")
    width: 1280
    height: 800
    visible: true

    // Strip the system / compositor decoration so the entire frame ―
    // titlebar + edges ― is ours to paint in carbon. The custom CarbonTitle-
    // Bar in `header` provides the move/min/max/close controls; resize
    // handles on the edges below delegate to startSystemResize() which the
    // compositor honours via xdg-shell on Wayland and _NET_WM_MOVERESIZE
    // on X11.
    flags: Qt.Window | Qt.FramelessWindowHint

    // ---- Design tokens (carbon-only, blue strictly as accent) ------------
    // Reachable from anywhere as applicationWindow().jelly. Surfaces are
    // pure dark greys; the only blue in the visual language is the
    // fluorescent accent — used on the main panel border, on card hover,
    // and on focus highlights.
    readonly property QtObject jelly: QtObject {
        readonly property color carbonBase:     "#0a0d14"   // window background
        readonly property color carbonAlt:      "#11151c"   // titlebar, drawer, panes
        readonly property color carbonElevated: "#161a23"   // cards / raised surfaces
        readonly property color carbonHover:    "#1d2230"   // card hover background
        readonly property color borderSubtle:   "#222a36"   // faint card / pane outline
        readonly property color accent:         "#3daee9"   // fluorescent blue
        readonly property color accentHot:      "#5fb8ff"   // brighter accent for focus
        readonly property color textPrimary:    "#e6ecf3"
        readonly property color textDim:        "#8a98ad"
    }

    // ---- Kirigami palette override ---------------------------------------
    Kirigami.Theme.inherit: false
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.backgroundColor:           jelly.carbonBase
    Kirigami.Theme.alternateBackgroundColor:  jelly.carbonElevated
    Kirigami.Theme.textColor:                 jelly.textPrimary
    Kirigami.Theme.disabledTextColor:         jelly.textDim
    Kirigami.Theme.highlightColor:            jelly.accent
    Kirigami.Theme.highlightedTextColor:      "#0a0d14"
    Kirigami.Theme.focusColor:                jelly.accent
    Kirigami.Theme.hoverColor:                jelly.accentHot
    Kirigami.Theme.linkColor:                 jelly.accent
    Kirigami.Theme.negativeTextColor:         "#f78787"
    Kirigami.Theme.positiveTextColor:         "#7ce0a6"
    Kirigami.Theme.neutralTextColor:          "#f7c987"

    color: jelly.carbonBase

    // Carbon weave overlay: a tiny 2×2 checker tiled across the whole
    // window at low opacity. The pattern is an inline SVG data URL so we
    // don't ship a binary asset for it. Sits behind every page.
    Image {
        anchors.fill: parent
        z: -1
        fillMode: Image.Tile
        opacity: 0.55
        smooth: false
        source: "data:image/svg+xml;utf8,"
            + "<svg xmlns='http://www.w3.org/2000/svg' width='6' height='6'>"
            + "<rect width='3' height='3' fill='%23ffffff' fill-opacity='0.018'/>"
            + "<rect x='3' y='3' width='3' height='3' fill='%23ffffff' fill-opacity='0.018'/>"
            + "<rect x='3' width='3' height='3' fill='%23000000' fill-opacity='0.18'/>"
            + "<rect y='3' width='3' height='3' fill='%23000000' fill-opacity='0.18'/>"
            + "</svg>"
    }

    // Subtle vertical gradient on top of the weave to suggest depth: dark
    // at the very top and bottom, slightly lifted in the middle.
    Rectangle {
        anchors.fill: parent
        z: -1
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#000000" }
            GradientStop { position: 0.4; color: "#0a0d14" }
            GradientStop { position: 0.7; color: "#0d1118" }
            GradientStop { position: 1.0; color: "#000000" }
        }
        opacity: 0.5
    }

    globalDrawer: Kirigami.GlobalDrawer {
        id: drawer
        title: qsTr("jellycute")
        titleIcon: "applications-multimedia"
        modal: false
        collapsible: true
        showHeaderWhenCollapsed: true

        // Carbon backdrop with a faint dark hairline so the drawer reads as
        // a separate panel without competing with the main panel's blue
        // border.
        background: Rectangle {
            color: root.jelly.carbonAlt
            border.color: root.jelly.borderSubtle
            border.width: 1
        }

        actions: [
            Kirigami.Action {
                text: qsTr("Inicio")
                icon.name: "go-home"
                onTriggered: {
                    while (root.pageStack.depth > 1) root.pageStack.pop();
                }
            },
            Kirigami.Action { separator: true },
            Kirigami.Action {
                text: qsTr("Reproductor de prueba")
                icon.name: "media-playback-start"
                onTriggered: root.pageStack.push(Qt.resolvedUrl("PlayerPage.qml"))
            },
            Kirigami.Action {
                text: qsTr("Configuración")
                icon.name: "configure"
                onTriggered: root.pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
        ]

        // Library entries below the static actions. GlobalDrawer accepts
        // arbitrary content; we render libraries as ItemDelegates so they
        // pick up the Kirigami theme automatically.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Kirigami.Heading {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.topMargin: Kirigami.Units.largeSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing
                text: qsTr("Bibliotecas")
                level: 4
                visible: viewsModel.count > 0
                color: root.jelly.accent
                opacity: 0.85
            }

            Repeater {
                model: viewsModel

                delegate: ItemDelegate {
                    Layout.fillWidth: true
                    text: model.name
                    icon.name: model.collectionType === "tvshows"
                        ? "video-television"
                        : model.collectionType === "movies"
                            ? "applications-multimedia"
                            : "folder"

                    onClicked: {
                        root.pageStack.push(Qt.resolvedUrl("GridPage.qml"), {
                            parentId: model.id,
                            parentName: model.name,
                            collectionType: model.collectionType
                        });
                    }
                }
            }
        }
    }

    header: CarbonTitleBar {
        title: root.title
    }

    pageStack.initialPage: Qt.resolvedUrl("HomePage.qml")

    // Fluorescent blue hairline framing the main panel area (the page
    // stack region between header and footer). This is the single
    // prominent blue line the carbon design hangs off; everything else
    // is dark grey on dark grey.
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: root.jelly.accent
        z: 998
    }

    // Resize handles on the sides + bottom + bottom corners. Top resize
    // is intentionally skipped: the title bar already lives there and we
    // don't want the diagonal-cursor corners to compete with the close
    // button. Each handle hands the resize loop to the compositor via
    // startSystemResize() (xdg-shell on Wayland, _NET_WM_MOVERESIZE on
    // X11).
    Item {
        anchors.fill: parent
        z: 999

        readonly property int edge: 4
        readonly property int corner: 10

        MouseArea {                              // left
            x: 0; y: 0
            width: parent.edge
            height: parent.height - parent.corner
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.LeftEdge)
        }
        MouseArea {                              // right
            x: parent.width - parent.edge; y: 0
            width: parent.edge
            height: parent.height - parent.corner
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.RightEdge)
        }
        MouseArea {                              // bottom
            x: parent.corner; y: parent.height - parent.edge
            width: parent.width - 2 * parent.corner
            height: parent.edge
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.BottomEdge)
        }
        MouseArea {                              // bottom-left
            x: 0; y: parent.height - parent.corner
            width: parent.corner; height: parent.corner
            cursorShape: Qt.SizeBDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
        }
        MouseArea {                              // bottom-right
            x: parent.width - parent.corner; y: parent.height - parent.corner
            width: parent.corner; height: parent.corner
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
        }
    }

    Component.onCompleted: {
        // No active account on startup → push the login form on top of the
        // (still-empty) HomePage. LoginPage pops itself on success.
        if (!jellyfin.authenticated)
            pageStack.push(Qt.resolvedUrl("LoginPage.qml"));
    }
}
