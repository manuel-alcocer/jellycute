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

    // Transparent surface so the rounded corners painted by the title bar
    // and the page backgrounds become actual see-through to the desktop;
    // without this the corner pixels we don't paint would still come up
    // as solid black.
    color: "transparent"

    // Corner radius reused by the titlebar (top-left/top-right) and the
    // page backgrounds + main panel border (bottom-left/bottom-right).
    // Kept modest so the close-button hover area at the very top-right
    // doesn't visibly overlap the rounded cut.
    readonly property int cornerRadius: 8

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

    // No window-level weave/gradient overlay: clipping it to the rounded
    // shape adds complexity for a barely-visible texture. The titlebar
    // keeps its own weave (same SVG, clipped to its rounded backdrop),
    // which is plenty to convey the carbon-fibre look.

    globalDrawer: Kirigami.GlobalDrawer {
        id: drawer
        title: qsTr("jellycute")
        titleIcon: "applications-multimedia"
        modal: false
        collapsible: true
        showHeaderWhenCollapsed: true

        // Same KColorScheme-bypass treatment we apply to pages: kill
        // inheritance and override every colour role explicitly so the
        // drawer entries (Kirigami.Action / ItemDelegate) render in the
        // carbon palette regardless of the user's Plasma theme.
        Kirigami.Theme.inherit: false
        Kirigami.Theme.colorSet: Kirigami.Theme.Window
        Kirigami.Theme.backgroundColor:           root.jelly.carbonAlt
        Kirigami.Theme.textColor:                 root.jelly.textPrimary
        Kirigami.Theme.alternateBackgroundColor:  root.jelly.carbonElevated
        Kirigami.Theme.disabledTextColor:         root.jelly.textDim
        Kirigami.Theme.highlightColor:            root.jelly.accent
        Kirigami.Theme.highlightedTextColor:      "#0a0d14"
        Kirigami.Theme.focusColor:                root.jelly.accent
        Kirigami.Theme.hoverColor:                root.jelly.accentHot
        Kirigami.Theme.linkColor:                 root.jelly.accent
        Kirigami.Theme.negativeTextColor:         "#f78787"
        Kirigami.Theme.positiveTextColor:         "#7ce0a6"
        Kirigami.Theme.neutralTextColor:          "#f7c987"

        // Carbon backdrop with a faint dark hairline so the drawer reads as
        // a separate panel without competing with the main panel's blue
        // border.
        background: Rectangle {
            color: root.jelly.carbonAlt
            border.color: root.jelly.borderSubtle
            border.width: 1
        }

        // We don't use the actions: [...] list because Kirigami.Global-
        // Drawer renders Kirigami.Action items through an internal delegate
        // whose Kirigami.Theme bypasses the carbon overrides we set on the
        // drawer itself, which leaves the action labels in the system text
        // colour. Building the entries by hand from ItemDelegate gives us
        // direct control over the contentItem's Label colour.
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            // Static entries (Home / Player / Settings).
            Repeater {
                model: [
                    {label: qsTr("Inicio"),                 icon: "go-home",
                     act: "home"},
                    {label: qsTr("Reproductor de prueba"),  icon: "media-playback-start",
                     act: "player"},
                    {label: qsTr("Configuración"),          icon: "configure",
                     act: "settings"},
                ]
                delegate: ItemDelegate {
                    Layout.fillWidth: true
                    hoverEnabled: true

                    background: Rectangle {
                        color: hovered ? root.jelly.carbonHover : "transparent"
                    }
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        Kirigami.Icon {
                            Layout.leftMargin: Kirigami.Units.largeSpacing
                            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                            source: modelData.icon
                            color: root.jelly.textPrimary
                            isMask: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.label
                            color: root.jelly.textPrimary
                            elide: Text.ElideRight
                        }
                    }

                    onClicked: {
                        switch (modelData.act) {
                        case "home":
                            while (root.pageStack.depth > 1) root.pageStack.pop();
                            break;
                        case "player":
                            root.pageStack.push(Qt.resolvedUrl("PlayerPage.qml"));
                            break;
                        case "settings":
                            root.pageStack.push(Qt.resolvedUrl("SettingsPage.qml"));
                            break;
                        }
                    }
                }
            }

            // Subtle horizontal separator before the libraries section.
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing
                height: 1
                color: root.jelly.borderSubtle
            }

            Kirigami.Heading {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.topMargin: Kirigami.Units.smallSpacing
                Layout.bottomMargin: Kirigami.Units.smallSpacing
                text: qsTr("Bibliotecas")
                level: 4
                visible: viewsModel.count > 0
                color: root.jelly.accent
                opacity: 0.85
            }

            // Library entries — same recipe as the static entries above.
            Repeater {
                model: viewsModel

                delegate: ItemDelegate {
                    Layout.fillWidth: true
                    hoverEnabled: true

                    readonly property string libIcon:
                        model.collectionType === "tvshows"
                            ? "video-television"
                            : model.collectionType === "movies"
                                ? "applications-multimedia"
                                : "folder"

                    background: Rectangle {
                        color: hovered ? root.jelly.carbonHover : "transparent"
                    }
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        Kirigami.Icon {
                            Layout.leftMargin: Kirigami.Units.largeSpacing
                            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                            source: libIcon
                            color: root.jelly.textPrimary
                            isMask: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: model.name
                            color: root.jelly.textPrimary
                            elide: Text.ElideRight
                        }
                    }

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
        // Compose "jellycute — <current page title>" so we still surface
        // the active page name now that the global toolbar is gone.
        title: {
            var pageTitle = root.pageStack.currentItem
                            ? root.pageStack.currentItem.title : "";
            return pageTitle && pageTitle !== root.title
                ? root.title + " — " + pageTitle
                : root.title;
        }
    }

    // No global toolbar above the page content: it lived in its own
    // Kirigami.Theme.Header colorSet that's awkward to override on top of
    // our Window overrides, so it kept landing as a light grey strip. Our
    // CarbonTitleBar already shows the page title alongside the app name.
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    pageStack.initialPage: Qt.resolvedUrl("HomePage.qml")

    // Fluorescent blue hairline framing the main panel area (the page
    // stack region between header and footer). Square at the top (where
    // it meets the titlebar) and rounded at the bottom (where it meets
    // the window's bottom corners).
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: root.jelly.accent
        bottomLeftRadius: root.cornerRadius
        bottomRightRadius: root.cornerRadius
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
