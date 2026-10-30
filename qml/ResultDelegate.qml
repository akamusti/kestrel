import QtQuick
import QtQuick.Controls.Basic

Item {
    id: delegate

    required property string name
    required property string subtitle
    required property string iconName
    required property string kind

    property bool selected: false

    signal clicked()

    height: 56

    Rectangle {
        id: highlight
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        radius: 10
        color: delegate.selected ? "#24ffffff" : "transparent"

        Behavior on color {
            ColorAnimation {
                duration: 90
                easing.type: Easing.OutCubic
            }
        }

        Row {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 14
            spacing: 14

            Image {
                width: 28
                height: 28
                anchors.verticalCenter: parent.verticalCenter
                source: "image://icon/" + encodeURIComponent(delegate.iconName)
                sourceSize.width: 56
                sourceSize.height: 56
                smooth: true
            }

            Column {
                width: parent.width - 28 - 14 - kindLabel.width - 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    width: parent.width
                    text: delegate.name
                    color: "#f5f5f7"
                    font.pixelSize: 15
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: delegate.subtitle
                    color: "#8b8b96"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    visible: text.length > 0
                }
            }

            Text {
                id: kindLabel
                anchors.verticalCenter: parent.verticalCenter
                text: delegate.kind
                color: "#6e6e79"
                font.pixelSize: 11
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: delegate.clicked()
        }
    }
}
