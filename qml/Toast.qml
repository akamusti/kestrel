import QtQuick

/// Small transient confirmation ("Copied") shown above the footer.
Rectangle {
    id: toast

    property string message: ""

    readonly property int duration: 1200

    function show(text) {
        message = text
    }

    width: label.implicitWidth + 24
    height: 28
    radius: 8
    color: "#f01f1f26"
    border.width: 1
    border.color: "#26ffffff"
    opacity: message.length > 0 ? 1 : 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation {
            duration: 120
            easing.type: Easing.OutCubic
        }
    }

    onMessageChanged: {
        if (message.length > 0)
            hideTimer.restart()
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: toast.message
        color: "#f5f5f7"
        font.pixelSize: 11
    }

    Timer {
        id: hideTimer
        interval: toast.duration
        onTriggered: toast.message = ""
    }
}
