import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: qsTr("jellycute")
    width: 1280
    height: 800
    visible: true

    globalDrawer: Kirigami.GlobalDrawer {
        id: drawer
        title: qsTr("jellycute")
        titleIcon: "applications-multimedia"
        modal: false
        collapsible: true
        showHeaderWhenCollapsed: true

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
                opacity: 0.7
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

    pageStack.initialPage: Qt.resolvedUrl("HomePage.qml")

    Component.onCompleted: {
        // No active account on startup → push the login form on top of the
        // (still-empty) HomePage. LoginPage pops itself on success.
        if (!jellyfin.authenticated)
            pageStack.push(Qt.resolvedUrl("LoginPage.qml"));
    }
}
