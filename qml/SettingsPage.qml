import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Jellycute 1.0

// One-page Settings surface with three stacked sections (Cuentas, Vídeo,
// Apariencia). Heavier KCM-style sub-pages can come later; this layout
// covers the immediate needs (account switch + hwdec selector) without
// requiring a sub-PageRow.
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
    }

    title: qsTr("Configuración")

    // accountStore.changed has no Qt-property NOTIFY, so we drive a manual
    // refresh counter and have the bindings depend on it.
    property int refreshTick: 0
    Connections {
        target: accountStore
        function onChanged() { page.refreshTick++; }
    }

    function switchToAccount(accountId) {
        var acc = accountStore.accountAsMap(accountId);
        if (!acc.id) return;
        var srv = accountStore.serverAsMap(acc.serverId);
        if (!srv.id) return;
        jellyfin.setServer(srv.url);
        jellyfin.setCredentials(acc.userId, acc.token);
        accountStore.setCurrentAccountId(accountId);
        // Re-pull the home models with the new credentials in place.
        viewsModel.clear();
        resumeModel.clear();
        viewsModel.loadUserViews();
        resumeModel.loadResume();
        page.refreshTick++;
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        // ------------------------------------------------------------------
        // Cuentas
        // ------------------------------------------------------------------
        Kirigami.Heading {
            Layout.fillWidth: true
            level: 2
            text: qsTr("Cuentas")
        }

        Repeater {
            id: serverRepeater
            // Refresh whenever AccountStore emits changed().
            model: page.refreshTick, accountStore.serverList()

            delegate: Kirigami.Card {
                Layout.fillWidth: true

                readonly property var serverData: modelData
                readonly property string serverId: serverData.id
                readonly property string serverUrl: serverData.url.toString()

                contentItem: ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: Kirigami.Units.smallSpacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                text: serverData.name
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: serverUrl
                                opacity: 0.6
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                        }

                        ToolButton {
                            icon.name: "edit-delete"
                            text: qsTr("Eliminar servidor")
                            display: AbstractButton.IconOnly
                            ToolTip.visible: hovered
                            ToolTip.text: text
                            onClicked: accountStore.removeServer(serverId)
                        }
                    }

                    Repeater {
                        // For each account belonging to this server.
                        model: page.refreshTick, accountStore.accountList()

                        delegate: ItemDelegate {
                            Layout.fillWidth: true
                            visible: modelData.serverId === serverId

                            readonly property var accountData: modelData
                            readonly property bool active:
                                accountStore.currentAccountId() === accountData.id

                            onClicked: {
                                if (!active) page.switchToAccount(accountData.id);
                            }

                            contentItem: RowLayout {
                                spacing: Kirigami.Units.smallSpacing

                                Kirigami.Icon {
                                    source: "user-identity"
                                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
                                    Layout.preferredHeight: Kirigami.Units.iconSizes.small
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: accountData.username
                                    font.bold: active
                                    elide: Text.ElideRight
                                }
                                Kirigami.Chip {
                                    visible: active
                                    text: qsTr("Activa")
                                    closable: false
                                }
                                ToolButton {
                                    icon.name: "edit-delete"
                                    text: qsTr("Eliminar cuenta")
                                    display: AbstractButton.IconOnly
                                    ToolTip.visible: hovered
                                    ToolTip.text: text
                                    onClicked: accountStore.removeAccount(accountData.id)
                                }
                            }
                        }
                    }
                }
            }
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Añadir cuenta")
            icon.name: "list-add"
            onClicked: applicationWindow().pageStack.push(
                Qt.resolvedUrl("LoginPage.qml"))
        }

        Kirigami.PlaceholderMessage {
            Layout.alignment: Qt.AlignCenter
            Layout.preferredWidth: Math.min(parent.width * 0.6,
                                            Kirigami.Units.gridUnit * 30)
            visible: accountStore.serverList().length === 0
            text: qsTr("Aún no hay servidores configurados")
            icon.name: "network-disconnect"
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // ------------------------------------------------------------------
        // Vídeo
        // ------------------------------------------------------------------
        Kirigami.Heading {
            Layout.fillWidth: true
            level: 2
            text: qsTr("Vídeo")
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            ComboBox {
                id: hwdecCombo
                Kirigami.FormData.label: qsTr("Aceleración hardware")
                Layout.preferredWidth: Kirigami.Units.gridUnit * 16

                readonly property var choices: preferences.availableHwdecChoices()
                model: choices

                Component.onCompleted: {
                    var idx = choices.indexOf(preferences.hwdec);
                    if (idx < 0) idx = choices.indexOf("no");
                    if (idx >= 0) currentIndex = idx;
                }
                onActivated: preferences.hwdec = choices[currentIndex]
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Para activar tras cambiar el valor, cierra el reproductor"
                           + " y vuelve a abrirlo. Para tu iGPU Intel «vaapi» suele"
                           + " ser la opción más eficiente; «no» fuerza descodificación"
                           + " por software (más estable pero más CPU).")
                wrapMode: Text.WordWrap
                opacity: 0.7
                font.pointSize: Kirigami.Theme.smallFont.pointSize
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // ------------------------------------------------------------------
        // Apariencia (placeholder por ahora)
        // ------------------------------------------------------------------
        Kirigami.Heading {
            Layout.fillWidth: true
            level: 2
            text: qsTr("Apariencia")
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            opacity: 0.7
            text: qsTr("La aplicación usa el tema oscuro de Kirigami por defecto."
                       + " La conmutación a tema claro se conectará en una próxima"
                       + " versión; por ahora puedes ajustar el tema desde la"
                       + " configuración global de tu escritorio.")
        }

        Item { Layout.preferredHeight: Kirigami.Units.largeSpacing }
    }
}
