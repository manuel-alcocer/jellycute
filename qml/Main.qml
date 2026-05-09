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

    // ---- Design tokens (carbon fibre + blue glass) -----------------------
    // Reachable from anywhere as applicationWindow().jelly. Used by pages
    // that need a richer surface than what Kirigami.Theme exposes (custom
    // borders, glass tint, hover glow).
    readonly property QtObject jelly: QtObject {
        readonly property color carbonBase:    "#0a0d14"
        readonly property color carbonAlt:     "#11151c"
        readonly property color carbonWeave:   "#161a23"
        readonly property color glassSurface:  "#152035"
        readonly property color glassSurfaceAlt:"#1d2c47"
        readonly property color glassHover:    "#243a5e"
        readonly property color glassBorder:   "#2c5a8a"
        readonly property color glassBorderHot:"#3daee9"
        readonly property color accent:        "#3daee9"
        readonly property color accentDim:     "#1e6e9e"
        readonly property color accentHot:     "#5fb8ff"
        readonly property color textPrimary:   "#e6ecf3"
        readonly property color textDim:       "#8a98ad"
    }

    // ---- Kirigami palette override ---------------------------------------
    Kirigami.Theme.inherit: false
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.backgroundColor:           jelly.carbonBase
    Kirigami.Theme.alternateBackgroundColor:  jelly.glassSurface
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

        // Glass-ish background for the drawer.
        background: Rectangle {
            color: root.jelly.carbonAlt
            border.color: root.jelly.glassBorder
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

    // Saturated blue hairline on the four window edges, drawn on top of
    // everything so the carbon frame reads as a single unit.
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: root.jelly.glassBorder
        z: 998
    }

    // Resize handles on the four edges + four corners. Each is a thin
    // MouseArea anchored to a single side of the window; on press it
    // hands the resize loop to the compositor via startSystemResize().
    // Corners overlap the edges and have higher z so the diagonal cursor
    // takes priority near the corner.
    Item {
        anchors.fill: parent
        z: 999

        readonly property int edge: 4
        readonly property int corner: 10

        MouseArea {                              // left
            x: 0; y: parent.corner
            width: parent.edge
            height: parent.height - 2 * parent.corner
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.LeftEdge)
        }
        MouseArea {                              // right
            x: parent.width - parent.edge; y: parent.corner
            width: parent.edge
            height: parent.height - 2 * parent.corner
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.RightEdge)
        }
        MouseArea {                              // top
            x: parent.corner; y: 0
            width: parent.width - 2 * parent.corner
            height: parent.edge
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.TopEdge)
        }
        MouseArea {                              // bottom
            x: parent.corner; y: parent.height - parent.edge
            width: parent.width - 2 * parent.corner
            height: parent.edge
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.BottomEdge)
        }

        MouseArea {                              // top-left
            x: 0; y: 0
            width: parent.corner; height: parent.corner
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
        }
        MouseArea {                              // top-right
            x: parent.width - parent.corner; y: 0
            width: parent.corner; height: parent.corner
            cursorShape: Qt.SizeBDiagCursor
            onPressed: root.startSystemResize(Qt.TopEdge | Qt.RightEdge)
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
