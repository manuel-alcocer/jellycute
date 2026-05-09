import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Jellycute 1.0

// Single-item view. Pushed onto the stack with itemId (required) and
// initialItem (optional QVariantMap from ItemListModel.get(idx)) so the
// page can render name/poster/year before the full details arrive from
// the server.
Kirigami.ScrollablePage {
    id: page

    // Pin the Kirigami palette to the carbon tokens regardless of the
    // system KColorScheme — see HomePage.qml for the rationale.
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

    background: Rectangle {
        color: applicationWindow().jelly.carbonBase
        bottomLeftRadius: applicationWindow().cornerRadius
        bottomRightRadius: applicationWindow().cornerRadius
    }

    property string itemId: ""
    property var initialItem: ({})

    title: detailModel.name || (initialItem ? initialItem.name : "")
       || qsTr("Detalles")

    ItemDetailModel {
        id: detailModel
        client: jellyfin
        Component.onCompleted: {
            if (page.initialItem) detailModel.prefill(page.initialItem);
            if (page.itemId) detailModel.load(page.itemId);
        }
    }

    function fmtRuntime(seconds) {
        if (seconds <= 0) return "";
        var h = Math.floor(seconds / 3600);
        var m = Math.floor((seconds % 3600) / 60);
        if (h > 0) return qsTr("%1 h %2 min").arg(h).arg(m);
        return qsTr("%1 min").arg(m);
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        // ---- Header: poster + metadata + actions --------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing * 2

            // Poster — sits in a dark-grey cradle with a saturated blue
            // outline so it reads as a "framed" specimen on the carbon
            // background.
            Rectangle {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 14
                Layout.preferredHeight: Kirigami.Units.gridUnit * 21
                color: applicationWindow().jelly.carbonElevated
                radius: Kirigami.Units.smallSpacing
                border.width: 1
                border.color: applicationWindow().jelly.accent

                Image {
                    anchors.fill: parent
                    anchors.margins: 1
                    source: detailModel.posterUrl.toString() !== ""
                            ? detailModel.posterUrl
                            : (initialItem ? initialItem.posterUrl : "")
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    smooth: true
                }
            }

            // Metadata + actions
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Heading {
                    Layout.fillWidth: true
                    text: detailModel.name
                          || (initialItem ? initialItem.name : "")
                    wrapMode: Text.Wrap
                    level: 1
                }

                // Series context (e.g. "Breaking Bad — T1·E3") for episodes.
                Label {
                    Layout.fillWidth: true
                    visible: detailModel.type === "Episode"
                             && detailModel.seriesName !== ""
                    text: {
                        var s = detailModel.seriesName;
                        if (detailModel.parentIndexNumber > 0
                            && detailModel.indexNumber > 0)
                            s += qsTr(" · T%1 E%2")
                                  .arg(detailModel.parentIndexNumber)
                                  .arg(detailModel.indexNumber);
                        return s;
                    }
                    opacity: 0.7
                    wrapMode: Text.Wrap
                }

                // Bullet line: year • runtime • rating • community rating.
                Label {
                    Layout.fillWidth: true
                    text: {
                        var parts = [];
                        if (detailModel.productionYear > 0)
                            parts.push(detailModel.productionYear);
                        var rt = page.fmtRuntime(detailModel.runTimeSeconds);
                        if (rt) parts.push(rt);
                        if (detailModel.officialRating !== "")
                            parts.push(detailModel.officialRating);
                        if (detailModel.communityRating > 0)
                            parts.push("★ " + detailModel.communityRating.toFixed(1));
                        return parts.join("  ·  ");
                    }
                    opacity: 0.7
                    visible: text.length > 0
                }

                // Genres as a row of small chips.
                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    visible: detailModel.genres.length > 0

                    Repeater {
                        model: detailModel.genres
                        delegate: Kirigami.Chip {
                            text: modelData
                            closable: false
                        }
                    }
                }

                // Action buttons.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    Button {
                        text: qsTr("Reproducir")
                        icon.name: "media-playback-start"
                        enabled: detailModel.playable
                                 && playback.state !== "resolving"
                        highlighted: true
                        onClicked: page.beginPlayback(0)
                    }
                    Button {
                        text: qsTr("Reanudar")
                        icon.name: "media-seek-forward"
                        visible: detailModel.resumeSeconds > 0
                        enabled: detailModel.playable
                                 && playback.state !== "resolving"
                        onClicked: page.beginPlayback(detailModel.resumeSeconds)
                    }
                    ToolButton {
                        icon.name: detailModel.isFavorite
                                   ? "favorite" : "non-starred-symbolic"
                        text: detailModel.isFavorite
                              ? qsTr("Favorito") : qsTr("Marcar favorito")
                        onClicked: detailModel.toggleFavorite()
                    }
                    ToolButton {
                        icon.name: detailModel.isPlayed
                                   ? "checkbox" : "checkbox-symbolic-symbolic"
                        text: detailModel.isPlayed
                              ? qsTr("Visto") : qsTr("Marcar visto")
                        onClicked: detailModel.togglePlayed()
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        // ---- Overview -----------------------------------------------------
        Kirigami.Heading {
            Layout.fillWidth: true
            level: 3
            text: qsTr("Sinopsis")
            visible: detailModel.overview.length > 0
        }
        Label {
            Layout.fillWidth: true
            text: detailModel.overview
            wrapMode: Text.WordWrap
            visible: text.length > 0
        }

        // ---- Loading state ------------------------------------------------
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: detailModel.loading && detailModel.name.length === 0
        }
    }

    // Surface a transient error from playback resolution.
    Connections {
        target: playback
        function onStateChanged() {
            if (playback.state === "error") {
                page.showPassiveNotification(
                    qsTr("No se pudo reproducir: %1")
                        .arg(playback.errorMessage),
                    "long");
                return;
            }
            if (playback.state === "ready"
                && playback.itemId === page.itemId) {
                applicationWindow().pageStack.push(
                    Qt.resolvedUrl("PlayerPage.qml"));
            }
        }
    }

    function beginPlayback(startSeconds) {
        playback.start(page.itemId, startSeconds || 0);
    }
}
