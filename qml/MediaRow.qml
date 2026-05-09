import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Reusable horizontal row with a heading and a scrollable strip of poster
// cards. Used on Home (Resume row, Latest-per-library) and on detail pages
// for related-content sections.
//
// The mediaModel must expose at least: id, name, type, posterUrl,
// backdropUrl, productionYear, seriesName roles — i.e. an ItemListModel /
// BrowseModel.
ColumnLayout {
    id: root

    property string title: ""
    property var mediaModel: null
    property bool useBackdropPosters: false   // false = vertical 2:3, true = backdrop 16:9
    property real posterUnits: useBackdropPosters ? 16 : 8

    signal itemClicked(string itemId, string itemType)

    spacing: Kirigami.Units.smallSpacing

    readonly property real posterWidth: Kirigami.Units.gridUnit * posterUnits
    readonly property real posterHeight: useBackdropPosters
        ? posterWidth * 9 / 16
        : posterWidth * 3 / 2
    // Card chrome around the poster — title + subtitle take ~3 grid units.
    readonly property real cardHeight: posterHeight + Kirigami.Units.gridUnit * 3

    Kirigami.Heading {
        Layout.leftMargin: Kirigami.Units.largeSpacing
        text: root.title
        level: 3
        visible: text.length > 0
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.preferredHeight: root.cardHeight
        orientation: ListView.Horizontal
        spacing: Kirigami.Units.smallSpacing
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        leftMargin: Kirigami.Units.largeSpacing
        rightMargin: Kirigami.Units.largeSpacing
        model: root.mediaModel
        reuseItems: true

        delegate: ItemDelegate {
            id: card
            width: root.posterWidth
            height: root.cardHeight
            padding: 0
            background: Rectangle {
                color: card.hovered
                       ? Kirigami.Theme.alternateBackgroundColor
                       : "transparent"
                radius: Kirigami.Units.smallSpacing
            }

            onClicked: root.itemClicked(model.id, model.type)

            contentItem: ColumnLayout {
                spacing: Kirigami.Units.smallSpacing

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.posterHeight
                    color: Kirigami.Theme.alternateBackgroundColor
                    radius: Kirigami.Units.smallSpacing

                    Image {
                        anchors.fill: parent
                        source: root.useBackdropPosters && model.backdropUrl
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
                    maximumLineCount: 1
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    text: {
                        if (model.seriesName && model.seriesName.length > 0)
                            return model.seriesName;
                        if (model.productionYear > 0)
                            return model.productionYear;
                        return "";
                    }
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    opacity: 0.6
                    visible: text.length > 0
                }
            }
        }
    }
}
