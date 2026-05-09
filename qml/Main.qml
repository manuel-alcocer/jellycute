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
                enabled: false   // Phase 4 ports the settings dialog.
            }
        ]
    }

    pageStack.initialPage: Qt.resolvedUrl("HomePage.qml")
}
