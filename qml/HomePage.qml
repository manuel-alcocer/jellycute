import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Jellycute 1.0

// Home: a vertical scroll of horizontal media rows. The first row is
// "Continue watching" (resumeModel, backdrop posters); below it, one row per
// library with that library's latest items, instantiated via a Repeater
// driven by viewsModel.
Kirigami.Page {
    id: page

    title: qsTr("Inicio")
    padding: 0

    Flickable {
        id: scroll
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true

        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            width: scroll.width
            spacing: Kirigami.Units.largeSpacing * 2

            // Spacer for visual breathing room at the top of the page.
            Item { Layout.preferredHeight: Kirigami.Units.smallSpacing }

            MediaRow {
                Layout.fillWidth: true
                title: qsTr("Continuar viendo")
                mediaModel: resumeModel
                useBackdropPosters: true
                visible: resumeModel.count > 0

                onItemClicked: function(idx) {
                    page.handleClick(resumeModel, idx)
                }
            }

            Repeater {
                model: viewsModel

                delegate: ColumnLayout {
                    id: libDelegate
                    Layout.fillWidth: true

                    // Snapshot the outer model fields so children referring
                    // to "model.X" don't accidentally hit a different scope
                    // (e.g. inside the inner BrowseModel).
                    readonly property string libId: model.id
                    readonly property string libName: model.name
                    readonly property string libCollection: model.collectionType

                    BrowseModel {
                        id: latestModel
                        client: jellyfin
                        Component.onCompleted: loadLatest(libDelegate.libId, "")
                    }

                    MediaRow {
                        Layout.fillWidth: true
                        title: qsTr("Lo último en %1").arg(libDelegate.libName)
                        mediaModel: latestModel
                        useBackdropPosters: libDelegate.libCollection === "tvshows"
                        visible: latestModel.count > 0

                        onItemClicked: function(idx) {
                            page.handleClick(latestModel, idx)
                        }
                    }
                }
            }

            Kirigami.PlaceholderMessage {
                Layout.alignment: Qt.AlignCenter
                Layout.preferredWidth: Math.min(parent.width * 0.6,
                                                Kirigami.Units.gridUnit * 30)
                visible: viewsModel.count === 0 && !viewsModel.loading
                text: qsTr("No hay bibliotecas disponibles. ¿Servidor configurado correctamente?")
                icon.name: "folder-open"
            }
        }
    }

    Component.onCompleted: {
        // First-load fan-out: viewsModel is needed for the Latest Repeater
        // and for the future sidebar; resumeModel populates the first row.
        if (viewsModel.count === 0) viewsModel.loadUserViews();
        if (resumeModel.count === 0) resumeModel.loadResume();
    }

    function handleClick(model, idx) {
        var item = model.get(idx);
        if (!item) return;
        var folderTypes = ["Series", "Season", "BoxSet", "Folder",
                           "CollectionFolder", "MusicAlbum", "MusicArtist"];
        var isFolder = item.isFolder || folderTypes.indexOf(item.type) >= 0;
        if (isFolder) {
            applicationWindow().pageStack.push(Qt.resolvedUrl("GridPage.qml"), {
                parentId: item.id,
                parentName: item.name,
                collectionType: item.collectionType || ""
            });
        } else {
            applicationWindow().pageStack.push(Qt.resolvedUrl("DetailPage.qml"), {
                itemId: item.id,
                initialItem: item
            });
        }
    }
}
