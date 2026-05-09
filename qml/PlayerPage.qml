import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Jellycute 1.0

Kirigami.Page {
    id: page

    title: qsTr("Reproductor de prueba")
    padding: 0

    function fmt(secs) {
        if (secs <= 0) return "00:00";
        var h = Math.floor(secs / 3600);
        var m = Math.floor((secs % 3600) / 60);
        var s = Math.floor(secs % 60);
        var two = function(n) { return n < 10 ? "0" + n : "" + n; };
        return h > 0 ? h + ":" + two(m) + ":" + two(s) : two(m) + ":" + two(s);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        MpvObject {
            id: mpv
            Layout.fillWidth: true
            Layout.fillHeight: true

            Component.onCompleted: forceActiveFocus()

            // Black backdrop so the FBO area never shows the page background
            // when no frame has been rendered yet.
            Rectangle {
                anchors.fill: parent
                color: "black"
                z: -1
            }

            onEndReached: {
                // Phase 3 will route this back to the previous page; for now
                // we just stay on the player and reset position display.
            }
        }

        Pane {
            Layout.fillWidth: true
            padding: Kirigami.Units.largeSpacing
            background: Rectangle { color: Kirigami.Theme.alternateBackgroundColor }

            ColumnLayout {
                anchors.fill: parent
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    TextField {
                        id: urlField
                        Layout.fillWidth: true
                        placeholderText: qsTr("URL del vídeo (file://, http://, https://)")
                        text: Qt.application.arguments.length > 1
                              ? Qt.application.arguments[1] : ""
                    }
                    Button {
                        text: qsTr("Reproducir")
                        enabled: urlField.text.length > 0
                        icon.name: "media-playback-start"
                        onClicked: mpv.play(urlField.text, 0)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    ToolButton {
                        icon.name: mpv.paused ? "media-playback-start"
                                              : "media-playback-pause"
                        onClicked: mpv.togglePause()
                    }
                    ToolButton {
                        icon.name: "media-playback-stop"
                        onClicked: mpv.stop()
                    }
                    ToolButton {
                        icon.name: "media-skip-backward"
                        onClicked: mpv.seekRelative(-10)
                    }
                    ToolButton {
                        icon.name: "media-skip-forward"
                        onClicked: mpv.seekRelative(10)
                    }

                    Slider {
                        id: posSlider
                        Layout.fillWidth: true
                        from: 0
                        to: mpv.duration > 0 ? mpv.duration : 1
                        value: mpv.position
                        live: false
                        onMoved: mpv.seekAbsolute(value)
                    }

                    Label {
                        text: page.fmt(mpv.position) + " / " + page.fmt(mpv.duration)
                        font.family: "monospace"
                    }

                    ToolButton {
                        icon.name: mpv.muted ? "audio-volume-muted"
                                             : "audio-volume-high"
                        onClicked: mpv.toggleMute()
                    }
                    Slider {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                        from: 0
                        to: 100
                        value: mpv.volume
                        onMoved: mpv.volume = value
                    }
                }
            }
        }
    }
}
