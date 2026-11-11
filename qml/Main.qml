import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import Kestrel
import org.kde.layershell 1.0 as LayerShell

Window {
    id: root

    readonly property int rowHeight: 56
    readonly property int headerHeight: 60
    readonly property int footerHeight: 44
    readonly property int pageRows: Math.max(1, Math.floor(list.height / rowHeight))
    readonly property bool hasQuery: searchField.text.trim().length > 0
    readonly property bool darkTheme: results.theme !== "light"
    readonly property color panelBg:
        results.theme === "light" ? "#eef5f5f5" :
        results.theme === "breeze-light" ? "#eef0f4f8" :
        results.theme === "nord" ? "#ee2e3440" :
        results.theme === "dracula" ? "#ee282a36" :
        results.theme === "solarized" ? "#ee002b36" :
        results.theme === "breeze-dark" ? "#ee232629" :
        "#ee14141a"
    readonly property color panelBorder:
        results.theme === "light" || results.theme === "breeze-light" ? "#26000000" : "#26ffffff"
    readonly property color textColor:
        results.theme === "light" || results.theme === "breeze-light" ? "#1a1a1a" : "#f5f5f7"
    readonly property color footerBg:
        results.theme === "light" || results.theme === "breeze-light" ? "#0d000000" : "#0dffffff"

    // Wayland refuses activation for some windows; only hide after the window has
    // actually been active once, otherwise it would vanish right after showing.
    property bool wasActive: false

    width: 760
    height: 560
    visible: !startHidden
    color: "transparent"
    title: qsTr("Kestrel")
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    LayerShell.Window.layer: LayerShell.Window.LayerOverlay
    LayerShell.Window.anchors: LayerShell.Window.AnchorNone
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityExclusive
    LayerShell.Window.activateOnShow: true
    LayerShell.Window.wantsToBeOnActiveScreen: true

    x: Screen.virtualX + Math.round((Screen.width - width) / 2)
    y: Screen.virtualY + Math.round((Screen.height - height) / 2)

    function toggle() {
        if (root.visible) {
            root.hide()
            return
        }
        wasActive = false
        root.show()
        root.raise()
        root.requestActivate()
        searchField.forceActiveFocus()
    }

    function activateCurrent() {
        if (results.count === 0)
            return
        const row = Math.max(0, list.currentIndex)
        const source = results.sourceAt(row)
        if (!results.launch(row))
            return
        if (source === "calculator") {
            toast.show(qsTr("Copied"))
        } else {
            root.hide()
        }
    }

    function openActions() {
        if (results.count === 0)
            return
        actionPanel.items = results.actionsAt(Math.max(0, list.currentIndex))
        actionPanel.currentIndex = 0
        actionPanel.visible = actionPanel.items.length > 0
    }

    function runAction(actionIndex) {
        const action = actionPanel.items[actionIndex]
        if (!action)
            return
        const row = Math.max(0, list.currentIndex)
        actionPanel.visible = false
        if (!results.runAction(row, action.id))
            return
        if (action.id.startsWith("copy")) {
            toast.show(qsTr("Copied"))
        } else {
            root.hide()
        }
    }

    function resetQuery() {
        searchField.text = ""
        list.currentIndex = results.count > 0 ? 0 : -1
    }

    // Focus loss hides the palette; after 90 s hidden, pop back to root.
    onActiveChanged: {
        if (active) {
            wasActive = true
        } else if (visible && wasActive) {
            wasActive = false
            root.hide()
        }
    }

    Timer {
        interval: 90000
        running: !root.visible && root.hasQuery
        onTriggered: root.resetQuery()
    }

    AppIndex {
        id: appIndex
    }

    SearchModel {
        id: results
        index: appIndex
        query: searchField.text

        onFilterChanged: list.currentIndex = results.count > 0 ? 0 : -1
    }

    AutostartManager {
        id: autostart
    }

    Dialog {
        id: settingsDialog
        width: 460
        anchors.centerIn: Overlay.overlay
        modal: true
        title: qsTr("Settings")

        contentItem: Column {
            spacing: 12
            width: parent.width

            Label {
                text: qsTr("Web search URL (use %1 for the query)")
                font.pixelSize: 12
            }
            TextField {
                id: webSearchField
                width: parent.width
                text: results.webSearchUrl
                onEditingFinished: results.webSearchUrl = text
            }

            Label {
                text: qsTr("Theme")
                font.pixelSize: 12
            }
            ComboBox {
                id: themeBox
                width: parent.width
                model: [qsTr("Dark"), qsTr("Light"), qsTr("Breeze Dark"), qsTr("Breeze Light"),
                        qsTr("Nord"), qsTr("Dracula"), qsTr("Solarized")]
                currentIndex: {
                    const t = results.theme
                    if (t === "light") return 1
                    if (t === "breeze-dark") return 2
                    if (t === "breeze-light") return 3
                    if (t === "nord") return 4
                    if (t === "dracula") return 5
                    if (t === "solarized") return 6
                    return 0
                }
                onActivated: {
                    const values = ["dark","light","breeze-dark","breeze-light","nord","dracula","solarized"]
                    results.theme = values[currentIndex]
                }
            }
        }

        footer: Item {
            implicitHeight: 40
            Button {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Close")
                onClicked: settingsDialog.close()
            }
        }
    }

    Dialog {
        id: autostartDialog
        width: 420
        anchors.centerIn: Overlay.overlay
        modal: true
        visible: autostart.setupPromptRequired
        title: qsTr("Start Kestrel automatically?")
        standardButtons: Dialog.NoButton

        contentItem: Column {
            spacing: 12

            Label {
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTr("Kestrel needs to be running for its global shortcut to work. "
                           + "Allow it to start in the background when you sign in?")
            }

            Label {
                width: parent.width
                visible: autostart.errorMessage.length > 0
                wrapMode: Text.Wrap
                color: "#e57373"
                text: autostart.errorMessage
            }
        }

        footer: Item {
            implicitHeight: buttons.implicitHeight

            Row {
                id: buttons
                anchors.right: parent.right
                spacing: 8

                Button {
                    text: qsTr("Not now")
                    onClicked: autostart.dismissSetupPrompt()
                }

                Button {
                    text: qsTr("Enable")
                    onClicked: autostart.enableAtLogin()
                }
            }
        }
    }

    Rectangle {
        id: panel
        anchors.fill: parent
        radius: 14
        color: root.panelBg
        border.width: 1
        border.color: root.panelBorder
        clip: true

        transform: Translate {
            id: slide
            y: 8
        }

        Component.onCompleted: slideIn.start()

        ParallelAnimation {
            id: slideIn
            NumberAnimation {
                target: slide
                property: "y"
                from: 8
                to: 0
                duration: 140
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: panel
                property: "opacity"
                from: 0
                to: 1
                duration: 110
                easing.type: Easing.OutCubic
            }
        }

        Row {
            id: header
            width: parent.width
            height: root.headerHeight
            leftPadding: 20
            rightPadding: 20
            spacing: 12

            Image {
                width: 18
                height: 18
                anchors.verticalCenter: parent.verticalCenter
                source: "image://icon/system-search"
                sourceSize.width: 36
                sourceSize.height: 36
                opacity: 0.55
            }

            TextField {
                id: searchField
                width: header.width - 40 - 18 - header.spacing
                       - (filterChip.visible ? filterChip.width + header.spacing : 0)
                height: header.height
                anchors.verticalCenter: parent.verticalCenter
                focus: true
                selectByMouse: true
                color: root.textColor
                placeholderText: qsTr("Search apps and commands…")
                placeholderTextColor: "#7a7a85"
                font.pixelSize: 20
                background: null
                padding: 0

                onTextChanged: {
                    actionPanel.visible = false
                    list.currentIndex = results.count > 0 ? 0 : -1
                }

                Keys.onPressed: (event) => {
                    if (actionPanel.visible) {
                        if (event.key === Qt.Key_Down) {
                            actionPanel.currentIndex = Math.min(actionPanel.items.length - 1,
                                                                actionPanel.currentIndex + 1)
                            event.accepted = true
                            return
                        }
                        if (event.key === Qt.Key_Up) {
                            actionPanel.currentIndex = Math.max(0, actionPanel.currentIndex - 1)
                            event.accepted = true
                            return
                        }
                        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.runAction(actionPanel.currentIndex)
                            event.accepted = true
                            return
                        }
                        if (event.key === Qt.Key_Escape
                                || (event.key === Qt.Key_K && (event.modifiers & Qt.ControlModifier))) {
                            actionPanel.visible = false
                            event.accepted = true
                            return
                        }
                        actionPanel.visible = false
                    }

                    if (event.key === Qt.Key_Down) {
                        if (list.currentIndex < results.count - 1)
                            list.currentIndex += 1
                        event.accepted = true
                    } else if (event.key === Qt.Key_Up) {
                        if (list.currentIndex > 0)
                            list.currentIndex -= 1
                        event.accepted = true
                    } else if (event.key === Qt.Key_PageDown) {
                        list.currentIndex = Math.min(results.count - 1, list.currentIndex + root.pageRows)
                        event.accepted = true
                    } else if (event.key === Qt.Key_PageUp) {
                        list.currentIndex = Math.max(0, list.currentIndex - root.pageRows)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.activateCurrent()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        if (event.modifiers & Qt.ShiftModifier || !root.hasQuery)
                            root.hide()
                        else
                            root.resetQuery()
                        event.accepted = true
                    } else if (event.key >= Qt.Key_1 && event.key <= Qt.Key_9
                               && (event.modifiers & Qt.ControlModifier)) {
                        const index = event.key - Qt.Key_1
                        if (index < results.count) {
                            list.currentIndex = index
                            root.activateCurrent()
                        }
                        event.accepted = true
                    } else if (event.key === Qt.Key_K && (event.modifiers & Qt.ControlModifier)) {
                        root.openActions()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Comma && (event.modifiers & Qt.ControlModifier)) {
                        settingsDialog.open()
                        webSearchField.text = results.webSearchUrl
                        event.accepted = true
                    } else if (event.key === Qt.Key_P && (event.modifiers & Qt.ControlModifier)) {
                        results.nextFilter()
                        event.accepted = true
                    } else if (event.key === Qt.Key_R && (event.modifiers & Qt.ControlModifier)) {
                        appIndex.reload()
                        event.accepted = true
                    } else if (event.key === Qt.Key_C
                               && (event.modifiers & Qt.ControlModifier)
                               && (event.modifiers & Qt.ShiftModifier)) {
                        if (results.runAction(list.currentIndex, "copy-name"))
                            toast.show(qsTr("Copied"))
                        event.accepted = true
                    }
                }
            }

            Rectangle {
                id: filterChip
                anchors.verticalCenter: parent.verticalCenter
                width: visible ? filterChipLabel.implicitWidth + 18 : 0
                height: 22
                radius: 11
                visible: results.filterActive
                color: "#1affffff"
                border.width: 1
                border.color: "#22ffffff"

                Text {
                    id: filterChipLabel
                    anchors.centerIn: parent
                    text: results.filterLabel
                    color: "#c9c9d1"
                    font.pixelSize: 10
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: results.nextFilter()
                }
            }
        }

        Rectangle {
            id: divider
            width: parent.width
            height: 1
            anchors.top: header.bottom
            color: "#1affffff"
        }

        ListView {
            id: list
            width: parent.width
            anchors.top: divider.bottom
            anchors.bottom: footer.top
            topMargin: 4
            bottomMargin: 4
            clip: true
            model: results
            currentIndex: results.count > 0 ? 0 : -1
            boundsBehavior: Flickable.StopAtBounds
            section.property: "section"
            section.delegate: Item {
                width: list.width
                height: 26
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 22
                    anchors.verticalCenter: parent.verticalCenter
                    text: section === "suggestions" ? qsTr("SUGGESTIONS")
                          : section === "calculator" ? qsTr("CALCULATOR")
                          : section === "krunner" ? qsTr("KRUNNER")
                          : section === "web" ? qsTr("WEB")
                          : section === "files" ? qsTr("FILES")
                          : section === "clipboard" ? qsTr("CLIPBOARD")
                          : qsTr("RESULTS")
                    color: "#7a7a85"
                    font.pixelSize: 10
                    font.letterSpacing: 0.8
                    font.bold: true
                }
            }

            delegate: ResultDelegate {
                width: list.width
                selected: ListView.isCurrentItem
                onClicked: {
                    list.currentIndex = index
                    root.activateCurrent()
                }
            }

            Text {
                anchors.centerIn: parent
                visible: results.count === 0
                text: appIndex.loading ? qsTr("Indexing applications…")
                                       : root.hasQuery ? qsTr("No results for “%1”").arg(searchField.text)
                                       : results.filterActive ? qsTr("Nothing in %1").arg(results.filterLabel)
                                                              : qsTr("No applications found")
                color: "#7a7a85"
                font.pixelSize: 14
            }
        }

        ActionPanel {
            id: actionPanel
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: footer.top
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottomMargin: 8
            title: qsTr("ACTIONS")
            visible: false
            onActivated: (index) => root.runAction(index)
        }

        Toast {
            id: toast
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: footer.top
            anchors.bottomMargin: 12
        }

        Rectangle {
            id: footer
            width: parent.width
            height: root.footerHeight
            anchors.bottom: parent.bottom
            color: root.footerBg

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 14

                ShortcutHint {
                    keys: "↑↓"
                    label: qsTr("Navigate")
                }
                ShortcutHint {
                    keys: "↵"
                    label: qsTr("Open")
                }
                ShortcutHint {
                    keys: "ctrl K"
                    label: qsTr("Actions")
                }
                ShortcutHint {
                    keys: "ctrl P"
                    label: qsTr("Filter")
                }
                ShortcutHint {
                    keys: "esc"
                    label: qsTr("Close")
                }
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Kestrel · %1 apps").arg(appIndex.count)
                color: "#7a7a85"
                font.pixelSize: 11
            }
        }
    }
}
