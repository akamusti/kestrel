import QtQuick
import QtQuick.Controls.Basic

Item {
    id: delegate

    required property string name
    required property string subtitle
    required property string iconName
    required property string kind

    property bool selected: false
    property color textColor: "#f5f5f7"
    property color subtitleColor: "#8b8b96"
    property color kindColor: "#6e6e79"
    property color highlightColor: "#24ffffff"

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
        color: delegate.selected ? delegate.highlightColor : "transparent"

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
                    color: delegate.textColor
                    font.pixelSize: 15
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    wrapMode: Text.NoWrap
                }

                Text {
                    width: parent.width
                    text: delegate.subtitle
                    color: delegate.subtitleColor
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    maximumLineCount: 1
                    wrapMode: Text.NoWrap
                    visible: text.length > 0
                }
            }

            Text {
                id: kindLabel
                anchors.verticalCenter: parent.verticalCenter
                text: delegate.kind
                color: delegate.kindColor
                font.pixelSize: 11
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: delegate.clicked()
        }
    }
}
