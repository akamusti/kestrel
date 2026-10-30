import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import Kestrel

Window {
    id: root

    readonly property int rowHeight: 56
    readonly property int headerHeight: 60
    readonly property int footerHeight: 44
    readonly property int pageRows: Math.max(1, Math.floor(list.height / rowHeight))
    readonly property bool hasQuery: searchField.text.trim().length > 0

    // Wayland refuses activation for some windows; only hide after the window has
    // actually been active once, otherwise it would vanish right after showing.
    property bool wasActive: false

    width: 760
    height: 560
    visible: true
    color: "transparent"
    title: qsTr("Kestrel")
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    x: Math.round((Screen.width - width) / 2)
    y: Math.round(Screen.height * 0.18)

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
        results.launch(row)
        root.hide()
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
    }

    Rectangle {
        id: panel
        anchors.fill: parent
        radius: 14
        color: "#ee14141a"
        border.width: 1
        border.color: "#26ffffff"
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
                height: header.height
                anchors.verticalCenter: parent.verticalCenter
                focus: true
                selectByMouse: true
                color: "#f5f5f7"
                placeholderText: qsTr("Search apps and commands…")
                placeholderTextColor: "#7a7a85"
                font.pixelSize: 20
                background: null
                padding: 0

                onTextChanged: list.currentIndex = results.count > 0 ? 0 : -1

                Keys.onPressed: (event) => {
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
                    } else if (event.key === Qt.Key_R && (event.modifiers & Qt.ControlModifier)) {
                        appIndex.reload()
                        event.accepted = true
                    } else if (event.key === Qt.Key_C
                               && (event.modifiers & Qt.ControlModifier)
                               && (event.modifiers & Qt.ShiftModifier)) {
                        clipboardHelper.text = results.nameAt(list.currentIndex)
                        clipboardHelper.selectAll()
                        clipboardHelper.copy()
                        event.accepted = true
                    }
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

        TextEdit {
            id: clipboardHelper
            visible: false
            width: 0
            height: 0
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
                    text: section === "suggestions" ? qsTr("SUGGESTIONS") : qsTr("RESULTS")
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
                                                       : qsTr("No applications found")
                color: "#7a7a85"
                font.pixelSize: 14
            }
        }

        Rectangle {
            id: footer
            width: parent.width
            height: root.footerHeight
            anchors.bottom: parent.bottom
            color: "#0dffffff"

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
