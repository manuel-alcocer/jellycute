import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Jellycute 1.0

// Pushed onto the page stack at startup when no account is active. On
// successful authentication it pops itself, persists the (server, account)
// pair through AccountStore, and tells the existing models to refresh so
// HomePage repaints with real content.
Kirigami.Page {
    id: page

    title: qsTr("Conectar a Jellyfin")
    padding: 0

    property bool busy: false
    property string statusText: ""

    function sanitizeUrl(s) {
        var t = (s || "").trim();
        if (t.length === 0) return "";
        if (!t.startsWith("http://") && !t.startsWith("https://"))
            t = "http://" + t;
        while (t.endsWith("/")) t = t.slice(0, -1);
        return t;
    }

    function tryConnect() {
        var sanitized = sanitizeUrl(serverField.text);
        if (sanitized.length === 0) {
            page.statusText = qsTr("Introduce la URL del servidor");
            return;
        }
        page.busy = true;
        page.statusText = qsTr("Autenticando…");
        // Push the sanitized URL back into the field so the user sees what
        // we'll actually attempt against.
        serverField.text = sanitized;
        jellyfin.setServer(sanitized);
        jellyfin.authenticate(userField.text, passField.text);
    }

    Connections {
        target: jellyfin
        function onAuthenticated() {
            // Reuse an existing server entry when the URL already matches;
            // otherwise register a fresh one. Hostname is a serviceable
            // default name and the future Settings page can rename it.
            var serverUrl = jellyfin.server;
            var serverId = accountStore.findServerIdByUrl(serverUrl);
            if (serverId.length === 0) {
                var name = serverUrl.toString().replace(/^https?:\/\//, "")
                                                 .replace(/\/.*$/, "");
                serverId = accountStore.addServer(name || qsTr("Jellyfin"),
                                                  serverUrl);
            }
            var accId = accountStore.addAccountWith(
                serverId, userField.text,
                jellyfin.userId, jellyfin.accessToken);
            accountStore.setCurrentAccountId(accId);

            // Wake up the Home models with credentials in place.
            viewsModel.loadUserViews();
            resumeModel.loadResume();

            applicationWindow().pageStack.pop();
        }
        function onAuthenticationFailed(message) {
            page.busy = false;
            page.statusText = qsTr("Falló la autenticación: %1").arg(message);
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Kirigami.Units.gridUnit * 4,
                        Kirigami.Units.gridUnit * 30)
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Heading {
            Layout.fillWidth: true
            text: qsTr("Conectar a Jellyfin")
            level: 1
            horizontalAlignment: Text.AlignHCenter
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            TextField {
                id: serverField
                Kirigami.FormData.label: qsTr("Servidor")
                placeholderText: "https://jellyfin.example.com"
                inputMethodHints: Qt.ImhUrlCharactersOnly
                Layout.fillWidth: true
                onAccepted: userField.forceActiveFocus()
            }
            TextField {
                id: userField
                Kirigami.FormData.label: qsTr("Usuario")
                Layout.fillWidth: true
                onAccepted: passField.forceActiveFocus()
            }
            TextField {
                id: passField
                Kirigami.FormData.label: qsTr("Contraseña")
                echoMode: TextInput.Password
                Layout.fillWidth: true
                onAccepted: page.tryConnect()
            }
        }

        Label {
            Layout.fillWidth: true
            text: page.statusText
            color: page.busy
                   ? Kirigami.Theme.textColor
                   : Kirigami.Theme.negativeTextColor
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            visible: text.length > 0
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Kirigami.Units.gridUnit * 12
            text: page.busy ? qsTr("Conectando…") : qsTr("Conectar")
            icon.name: "network-connect"
            highlighted: true
            enabled: !page.busy
                     && serverField.text.trim().length > 0
                     && userField.text.length > 0
            onClicked: page.tryConnect()
        }
    }

    Component.onCompleted: {
        if (serverField.text.length === 0)
            serverField.forceActiveFocus();
        else if (userField.text.length === 0)
            userField.forceActiveFocus();
        else
            passField.forceActiveFocus();
    }
}
