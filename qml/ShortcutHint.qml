import QtQuick

Row {
    id: hint

    property string keys: ""
    property string label: ""
    property color chipBg: "#1affffff"
    property color chipBorder: "#22ffffff"
    property color chipText: "#c9c9d1"
    property color labelColor: "#6e6e79"

    spacing: 6

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: keysText.implicitWidth + 12
        height: 20
        radius: 5
        color: hint.chipBg
        border.width: 1
        border.color: hint.chipBorder

        Text {
            id: keysText
            anchors.centerIn: parent
            text: hint.keys
            color: hint.chipText
            font.pixelSize: 10
            font.bold: true
        }
    }

    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: hint.label
        color: hint.labelColor
        font.pixelSize: 11
    }
}
