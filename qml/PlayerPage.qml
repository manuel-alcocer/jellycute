import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Jellycute 1.0

// Two operating modes share this page:
//   - Session mode: pushed after playback.start() resolves a stream URL on
//     the server. Auto-plays, reports start/progress/stop back through the
//     PlaybackSession, and pops on end-of-file.
//   - Test mode: opened from the sidebar when playback is idle. Shows a URL
//     field + Play button so a raw stream can be tried without going through
//     a server resolution.
Kirigami.Page {
    id: page

    title: qsTr("Reproductor")
    padding: 0

    readonly property bool sessionMode: playback.state !== "idle"
    property bool _autoStarted: false

    function fmt(secs) {
        if (secs <= 0) return "00:00";
        var h = Math.floor(secs / 3600);
        var m = Math.floor((secs % 3600) / 60);
        var s = Math.floor(secs % 60);
        var two = function(n) { return n < 10 ? "0" + n : "" + n; };
        return h > 0 ? h + ":" + two(m) + ":" + two(s) : two(m) + ":" + two(s);
    }

    function maybeAutoPlay() {
        if (page._autoStarted) return;
        if (!page.sessionMode) return;
        if (playback.state !== "ready") return;
        page._autoStarted = true;
        mpv.play(playback.streamUrl, playback.startSeconds);
    }

    Component.onCompleted: {
        mpv.forceActiveFocus();
        maybeAutoPlay();
    }

    // Session-mode lifecycle: tell the server when playback actually starts
    // (mpv has the file loaded), every 10s while it's running, and on EOF.
    Connections {
        target: mpv
        function onPlaybackStarted() {
            if (page.sessionMode) playback.reportStarted(mpv.position);
        }
        function onEndReached() {
            if (page.sessionMode && playback.state === "playing")
                playback.reportStopped(mpv.position);
            applicationWindow().pageStack.pop();
        }
    }

    // If the page lands while playback is still resolving, kick off as soon
    // as the stream URL becomes ready.
    Connections {
        target: playback
        function onStateChanged() { page.maybeAutoPlay(); }
    }

    Timer {
        id: progressTimer
        interval: 10000
        repeat: true
        running: page.sessionMode && playback.state === "playing"
                 && mpv.duration > 0
        onTriggered: playback.reportProgress(mpv.position, mpv.paused)
    }

    // User pops back without reaching EOF — report a stop so the server
    // doesn't think we're still watching, and tear down any transcode.
    Component.onDestruction: {
        if (!page.sessionMode) return;
        if (playback.state === "playing")
            playback.reportStopped(mpv.position);
        else if (playback.state !== "ended")
            playback.cancel();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        MpvObject {
            id: mpv
            Layout.fillWidth: true
            Layout.fillHeight: true

            Rectangle {
                anchors.fill: parent
                color: "black"
                z: -1
            }
        }

        Pane {
            Layout.fillWidth: true
            padding: Kirigami.Units.largeSpacing
            background: Rectangle {
                color: applicationWindow().jelly.carbonAlt
                border.width: 1
                border.color: applicationWindow().jelly.borderSubtle
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: Kirigami.Units.smallSpacing

                // Test-mode URL bar. Hidden once a session is active to keep
                // the player chrome clean.
                RowLayout {
                    Layout.fillWidth: true
                    visible: !page.sessionMode
                    spacing: Kirigami.Units.smallSpacing

                    TextField {
                        id: urlField
                        Layout.fillWidth: true
                        placeholderText: qsTr("URL del vídeo (file://, http://, https://)")
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
                        onClicked: {
                            mpv.stop();
                            if (page.sessionMode)
                                applicationWindow().pageStack.pop();
                        }
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

    BusyIndicator {
        anchors.centerIn: parent
        running: page.sessionMode && playback.state === "resolving"
        z: 100
    }
}
