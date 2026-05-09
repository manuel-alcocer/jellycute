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

    pageStack.initialPage: Kirigami.ScrollablePage {
        id: page
        title: qsTr("Bibliotecas")

        actions: [
            Kirigami.Action {
                text: qsTr("Cargar")
                icon.name: "view-refresh"
                onTriggered: viewsModel.loadUserViews()
            }
        ]

        ColumnLayout {
            anchors.fill: parent
            spacing: Kirigami.Units.largeSpacing

            Label {
                Layout.fillWidth: true
                text: viewsModel.loading
                      ? qsTr("Cargando…")
                      : qsTr("Vistas: %1").arg(viewsModel.count)
                font.pointSize: Kirigami.Theme.defaultFont.pointSize + 1
                opacity: 0.75
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: viewsModel
                clip: true
                spacing: Kirigami.Units.smallSpacing

                delegate: ItemDelegate {
                    width: ListView.view.width

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        Image {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 6
                            source: model.posterUrl
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: model.name
                                elide: Text.ElideRight
                                font.bold: true
                            }
                            Label {
                                Layout.fillWidth: true
                                text: model.collectionType !== ""
                                      ? model.collectionType
                                      : model.type
                                elide: Text.ElideRight
                                opacity: 0.6
                            }
                        }
                    }
                }
            }

            Kirigami.PlaceholderMessage {
                Layout.alignment: Qt.AlignCenter
                visible: viewsModel.count === 0 && !viewsModel.loading
                text: qsTr("Pulsa Cargar para listar las bibliotecas del servidor")
                icon.name: "view-list-icons"
            }
        }
    }
}
