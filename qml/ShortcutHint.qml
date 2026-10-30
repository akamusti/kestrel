import QtQuick

Row {
    id: hint

    property string keys: ""
    property string label: ""

    spacing: 6

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: keysText.implicitWidth + 12
        height: 20
        radius: 5
        color: "#1affffff"
        border.width: 1
        border.color: "#22ffffff"

        Text {
            id: keysText
            anchors.centerIn: parent
            text: hint.keys
            color: "#c9c9d1"
            font.pixelSize: 10
            font.bold: true
        }
    }

    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: hint.label
        color: "#6e6e79"
        font.pixelSize: 11
    }
}
