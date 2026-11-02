import QtQuick

/// Keyboard-driven overlay list used by the action panel (Ctrl+K).
/// The parent owns focus and forwards Up/Down/Enter/Esc.
Rectangle {
    id: panel

    property string title: ""
    property var items: []
    property int currentIndex: 0

    signal activated(int index)

    readonly property int rowHeight: 34
    readonly property int headerHeight: 26

    implicitHeight: headerHeight + Math.max(1, items.length) * rowHeight + 8
    radius: 12
    color: "#f216161c"
    border.width: 1
    border.color: "#26ffffff"

    Text {
        id: header
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.top: parent.top
        anchors.topMargin: 8
        text: panel.title
        color: "#7a7a85"
        font.pixelSize: 10
        font.letterSpacing: 0.8
        font.bold: true
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.topMargin: 4
        anchors.leftMargin: 6
        anchors.rightMargin: 6

        Repeater {
            model: panel.items

            delegate: Rectangle {
                id: row

                required property var modelData
                required property int index

                width: parent.width
                height: panel.rowHeight
                radius: 8
                color: row.index === panel.currentIndex ? "#26ffffff" : "transparent"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.title
                    color: "#f5f5f7"
                    font.pixelSize: 13
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.shortcut
                    color: "#6e6e79"
                    font.pixelSize: 10
                    visible: text.length > 0
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: panel.activated(row.index)
                }
            }
        }
    }
}
