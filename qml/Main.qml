import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: qsTr("jellycute")
    width: 1280
    height: 800
    visible: true

    pageStack.initialPage: Kirigami.ScrollablePage {
        title: qsTr("Inicio")

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4

            text: qsTr("Kirigami shell ready.\nPhase 0 OK.")
            icon.name: "checkmark"
        }
    }
}
