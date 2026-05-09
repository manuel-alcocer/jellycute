import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Jellycute 1.0

// Drill-in browser. Pushed onto the page stack with parentId set to a
// library, series, season, etc. Owns its own BrowseModel and reloads
// whenever pagination, the alphabet filter or the parent change. Page
// stack handles back/forward — we only push deeper from here.
Kirigami.Page {
    id: page

    property string parentId: ""
    property string parentName: ""
    property string collectionType: ""        // "movies"/"tvshows"/...
    property string includeItemTypes: ""      // "" = let the server decide
    property int pageSize: 60
    property int currentPage: 0
    property string nameStartsWith: ""
    property bool useBackdropPosters: collectionType === "tvshows"

    // Avoid double-reloading on initial property assignment vs onCompleted.
    property bool _ready: false

    title: parentName || qsTr("Biblioteca")
    padding: 0

    BrowseModel {
        id: childrenModel
        client: jellyfin
    }

    function reload() {
        var startIndex = page.currentPage * page.pageSize;
        childrenModel.loadChildren(page.parentId, page.includeItemTypes, false,
                                   "SortName", page.nameStartsWith,
                                   startIndex, page.pageSize);
    }

    function navigate(itemIndex) {
        var item = childrenModel.get(itemIndex);
        if (!item) return;
        var folderTypes = ["Series", "Season", "BoxSet", "Folder",
                           "CollectionFolder", "MusicAlbum", "MusicArtist"];
        var isFolder = item.isFolder || folderTypes.indexOf(item.type) >= 0;
        if (isFolder) {
            applicationWindow().pageStack.push(Qt.resolvedUrl("GridPage.qml"), {
                parentId: item.id,
                parentName: item.name,
                collectionType: item.collectionType || page.collectionType
            });
        } else {
            // Phase 3c routes playable items to DetailPage; for now, just log.
            console.log("DetailPage TBD:", item.id, item.type, item.name);
        }
    }

    onParentIdChanged: if (_ready) { currentPage = 0; reload() }
    onNameStartsWithChanged: if (_ready) { currentPage = 0; reload() }
    onCurrentPageChanged: if (_ready) reload()

    Component.onCompleted: { _ready = true; reload() }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Alphabet filter ----------------------------------------------
        Pane {
            Layout.fillWidth: true
            padding: Kirigami.Units.smallSpacing
            background: Rectangle { color: Kirigami.Theme.alternateBackgroundColor }

            Flow {
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 2

                Button {
                    text: qsTr("Todo")
                    flat: page.nameStartsWith !== ""
                    highlighted: page.nameStartsWith === ""
                    onClicked: page.nameStartsWith = ""
                }
                Repeater {
                    model: ["A","B","C","D","E","F","G","H","I","J","K","L","M",
                            "N","O","P","Q","R","S","T","U","V","W","X","Y","Z"]
                    delegate: Button {
                        text: modelData
                        flat: page.nameStartsWith !== modelData
                        highlighted: page.nameStartsWith === modelData
                        onClicked: page.nameStartsWith = modelData
                    }
                }
            }
        }

        // ---- Grid ---------------------------------------------------------
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: childrenModel
            cellWidth: Kirigami.Units.gridUnit * (page.useBackdropPosters ? 18 : 10)
            cellHeight: page.useBackdropPosters
                        ? Kirigami.Units.gridUnit * 14
                        : Kirigami.Units.gridUnit * 18
            reuseItems: true

            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                width: grid.cellWidth - Kirigami.Units.smallSpacing
                height: grid.cellHeight - Kirigami.Units.smallSpacing
                padding: 0

                onClicked: page.navigate(index)

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: page.useBackdropPosters
                            ? width * 9 / 16
                            : width * 3 / 2
                        color: Kirigami.Theme.alternateBackgroundColor
                        radius: Kirigami.Units.smallSpacing

                        Image {
                            anchors.fill: parent
                            source: page.useBackdropPosters && model.backdropUrl
                                    ? model.backdropUrl
                                    : model.posterUrl
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            smooth: true
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.smallSpacing
                        Layout.rightMargin: Kirigami.Units.smallSpacing
                        text: model.name
                        elide: Text.ElideRight
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.smallSpacing
                        Layout.rightMargin: Kirigami.Units.smallSpacing
                        text: {
                            if (model.seriesName && model.seriesName.length > 0)
                                return model.seriesName;
                            if (model.productionYear > 0)
                                return model.productionYear;
                            return "";
                        }
                        elide: Text.ElideRight
                        opacity: 0.6
                        visible: text.length > 0
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }

        // ---- Loading + empty placeholders ---------------------------------
        Kirigami.PlaceholderMessage {
            Layout.alignment: Qt.AlignCenter
            Layout.preferredWidth: Math.min(parent.width * 0.6,
                                            Kirigami.Units.gridUnit * 30)
            visible: !childrenModel.loading && childrenModel.totalCount === 0
            text: page.nameStartsWith === ""
                  ? qsTr("Sin elementos")
                  : qsTr("Sin coincidencias para «%1»").arg(page.nameStartsWith)
            icon.name: "edit-clear"
        }

        // ---- Pagination ---------------------------------------------------
        Pane {
            Layout.fillWidth: true
            padding: Kirigami.Units.smallSpacing
            background: Rectangle { color: Kirigami.Theme.alternateBackgroundColor }
            visible: childrenModel.totalCount > page.pageSize

            RowLayout {
                anchors.fill: parent

                Item { Layout.fillWidth: true }

                ToolButton {
                    icon.name: "go-previous"
                    enabled: page.currentPage > 0
                    onClicked: page.currentPage = page.currentPage - 1
                }

                Label {
                    text: {
                        var totalPages =
                            Math.max(1, Math.ceil(childrenModel.totalCount
                                                  / page.pageSize));
                        return qsTr("Página %1 de %2 — %3 elementos")
                            .arg(page.currentPage + 1)
                            .arg(totalPages)
                            .arg(childrenModel.totalCount);
                    }
                }

                ToolButton {
                    icon.name: "go-next"
                    enabled: (page.currentPage + 1) * page.pageSize
                             < childrenModel.totalCount
                    onClicked: page.currentPage = page.currentPage + 1
                }

                Item { Layout.fillWidth: true }
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: childrenModel.loading && childrenModel.count === 0
        z: 10
    }
}
