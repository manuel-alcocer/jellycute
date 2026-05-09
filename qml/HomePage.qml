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

    // Pin the Kirigami palette to the carbon tokens regardless of the
    // system KColorScheme. Setting colorSet alone isn't enough because
    // Kirigami.Page hard-codes colorSet=View internally and Kirigami
    // re-resolves colours from KColorScheme on every colorSet change;
    // inherit:false plus explicit overrides short-circuits that path.
    Kirigami.Theme.inherit: false
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.backgroundColor:           applicationWindow().jelly.carbonBase
    Kirigami.Theme.textColor:                 applicationWindow().jelly.textPrimary
    Kirigami.Theme.alternateBackgroundColor:  applicationWindow().jelly.carbonElevated
    Kirigami.Theme.disabledTextColor:         applicationWindow().jelly.textDim
    Kirigami.Theme.highlightColor:            applicationWindow().jelly.accent
    Kirigami.Theme.highlightedTextColor:      "#0a0d14"
    Kirigami.Theme.focusColor:                applicationWindow().jelly.accent
    Kirigami.Theme.hoverColor:                applicationWindow().jelly.accentHot
    Kirigami.Theme.linkColor:                 applicationWindow().jelly.accent
    Kirigami.Theme.negativeTextColor:         "#f78787"
    Kirigami.Theme.positiveTextColor:         "#7ce0a6"
    Kirigami.Theme.neutralTextColor:          "#f7c987"

    // Belt-and-braces: paint the page background directly so it can't fall
    // back to the QStyle's window colour if anything goes wrong with the
    // theme override above.
    background: Rectangle {
        color: applicationWindow().jelly.carbonBase
        bottomLeftRadius: applicationWindow().cornerRadius
        bottomRightRadius: applicationWindow().cornerRadius
    }

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
