import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

/// Raycast-inspired settings overlay: sidebar categories and a large content pane.
Item {
    id: settingsView

    anchors.fill: parent
    visible: false
    focus: visible

    property color panelBg: "#14141a"
    property color panelBorder: "#26ffffff"
    property color textColor: "#f5f5f7"
    property color mutedColor: "#8b8b96"
    property color faintColor: "#7a7a85"
    property color chipBg: "#1affffff"
    property color chipBorder: "#22ffffff"
    property color highlightColor: "#24ffffff"
    property color accentColor: "#7c9cff"

    property string theme: "dark"
    property string webSearchUrl: ""
    property var autostart: null

    signal themeSelected(string themeId)
    signal webSearchUrlEdited(string url)
    signal closeRequested()

    readonly property var themes: [
        { id: "dark", name: qsTr("Dark"), bg: "#14141a", fg: "#f5f5f7" },
        { id: "light", name: qsTr("Light"), bg: "#f5f5f5", fg: "#1a1a1a" },
        { id: "breeze-dark", name: qsTr("Breeze Dark"), bg: "#232629", fg: "#eff0f1" },
        { id: "breeze-light", name: qsTr("Breeze Light"), bg: "#f0f4f8", fg: "#232629" },
        { id: "nord", name: qsTr("Nord"), bg: "#2e3440", fg: "#eceff4" },
        { id: "dracula", name: qsTr("Dracula"), bg: "#282a36", fg: "#f8f8f2" },
        { id: "solarized", name: qsTr("Solarized"), bg: "#002b36", fg: "#eee8d5" }
    ]

    property int currentPage: 0
    readonly property var pages: [qsTr("Appearance"), qsTr("Search"), qsTr("Startup"), qsTr("About")]

    function open() {
        visible = true
        currentPage = 0
        webSearchField.text = settingsView.webSearchUrl
        forceActiveFocus()
    }

    function close() {
        visible = false
        settingsView.closeRequested()
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Escape) {
            settingsView.close()
            event.accepted = true
        }
    }

    // Dim the palette behind the sheet.
    Rectangle {
        anchors.fill: parent
        color: "#66000000"
        MouseArea {
            anchors.fill: parent
            onClicked: settingsView.close()
        }
    }

    Rectangle {
        id: sheet
        width: Math.min(760, parent.width - 48)
        height: Math.min(500, parent.height - 48)
        anchors.centerIn: parent
        radius: 16
        color: settingsView.panelBg
        border.width: 1
        border.color: settingsView.panelBorder

        MouseArea {
            anchors.fill: parent
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Title bar
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 56

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Settings")
                    color: settingsView.textColor
                    font.pixelSize: 18
                    font.bold: true
                }

                Button {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    height: 30
                    text: "✕"
                    font.pixelSize: 12
                    onClicked: settingsView.close()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: settingsView.panelBorder
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // Sidebar
                Rectangle {
                    Layout.preferredWidth: 190
                    Layout.fillHeight: true
                    color: settingsView.chipBg

                    Column {
                        anchors.fill: parent
                        anchors.topMargin: 14
                        spacing: 4

                        Repeater {
                            model: settingsView.pages
                            delegate: Rectangle {
                                required property string modelData
                                required property int index

                                width: parent.width - 20
                                height: 36
                                x: 10
                                radius: 8
                                color: settingsView.currentPage === index
                                       ? settingsView.highlightColor : "transparent"

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData
                                    color: settingsView.currentPage === index
                                           ? settingsView.textColor : settingsView.mutedColor
                                    font.pixelSize: 13
                                    font.bold: settingsView.currentPage === index
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: settingsView.currentPage = index
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: settingsView.panelBorder
                }

                // Content
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 24

                    // Appearance
                    Column {
                        anchors.fill: parent
                        spacing: 14
                        visible: settingsView.currentPage === 0

                        Text {
                            text: qsTr("Theme")
                            color: settingsView.textColor
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Text {
                            text: qsTr("Pick a color scheme. Changes apply immediately.")
                            color: settingsView.mutedColor
                            font.pixelSize: 12
                        }

                        Grid {
                            width: parent.width
                            columns: 3
                            spacing: 10

                            Repeater {
                                model: settingsView.themes
                                delegate: Rectangle {
                                    required property var modelData
                                    width: (parent.width - 20) / 3
                                    height: 76
                                    radius: 10
                                    color: modelData.bg
                                    border.width: settingsView.theme === modelData.id ? 2 : 1
                                    border.color: settingsView.theme === modelData.id
                                                  ? settingsView.accentColor : settingsView.panelBorder

                                    Text {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 12
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 10
                                        text: modelData.name
                                        color: modelData.fg
                                        font.pixelSize: 12
                                        font.bold: settingsView.theme === modelData.id
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: settingsView.themeSelected(modelData.id)
                                    }
                                }
                            }
                        }
                    }

                    // Search
                    Column {
                        anchors.fill: parent
                        spacing: 12
                        visible: settingsView.currentPage === 1

                        Text {
                            text: qsTr("Web Search")
                            color: settingsView.textColor
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Text {
                            text: qsTr("Used when no local result matches. Use %1 for the query.")
                            color: settingsView.mutedColor
                            font.pixelSize: 12
                        }

                        TextField {
                            id: webSearchField
                            width: parent.width
                            text: settingsView.webSearchUrl
                            placeholderText: "https://duckduckgo.com/?q=%1"
                            onEditingFinished: settingsView.webSearchUrlEdited(text)
                        }

                        Button {
                            text: qsTr("Reset to DuckDuckGo")
                            onClicked: {
                                webSearchField.text = "https://duckduckgo.com/?q=%1"
                                settingsView.webSearchUrlEdited(webSearchField.text)
                            }
                        }
                    }

                    // Startup
                    Column {
                        anchors.fill: parent
                        spacing: 12
                        visible: settingsView.currentPage === 2

                        Text {
                            text: qsTr("Start at Login")
                            color: settingsView.textColor
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            text: qsTr("Kestrel must be running for its global shortcut to work. "
                                       + "Enable autostart so the palette is ready after sign-in.")
                            color: settingsView.mutedColor
                            font.pixelSize: 12
                        }
                        Button {
                            text: qsTr("Enable Autostart")
                            enabled: settingsView.autostart !== null
                            onClicked: {
                                if (settingsView.autostart)
                                    settingsView.autostart.enableAtLogin()
                            }
                        }
                        Text {
                            visible: settingsView.autostart !== null
                                     && settingsView.autostart.errorMessage.length > 0
                            text: settingsView.autostart ? settingsView.autostart.errorMessage : ""
                            color: "#e57373"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            width: parent.width
                        }
                    }

                    // About
                    Column {
                        anchors.fill: parent
                        spacing: 10
                        visible: settingsView.currentPage === 3

                        Text {
                            text: qsTr("Kestrel")
                            color: settingsView.textColor
                            font.pixelSize: 18
                            font.bold: true
                        }
                        Text {
                            text: qsTr("Version 0.1.3 · GPL-2.0-or-later")
                            color: settingsView.mutedColor
                            font.pixelSize: 12
                        }
                        Text {
                            width: parent.width
                            wrapMode: Text.Wrap
                            text: qsTr("A keyboard-first launcher for KDE Plasma. "
                                       + "Press your shortcut, type, hit Enter.")
                            color: settingsView.mutedColor
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
    }
}
