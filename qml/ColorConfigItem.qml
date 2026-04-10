import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

RowLayout {
    id: root
    spacing: 10

    property string name: "Parameter Name"
    property color color: "transparent"

    signal colorChangedByUser(color newColor)

    ColorDialog {
        id: colorDialog
        title: "Farbe auswählen für " + root.name
        selectedColor: root.color

        options: ColorDialog.ShowAlphaChannel | ColorDialog.DontUseNativeDialog

        onAccepted: {
            root.colorChangedByUser(selectedColor);
            }
        }

    Label {
        text: root.name
        Layout.fillWidth: true
        elide: Text.ElideRight
        color: Material.foreground
        }

    // Farbmuster-Container
    Rectangle {
        implicitWidth: 40
        implicitHeight: 25
        border.color: "#888"
        border.width: 1
        radius: 2
        clip: true

        // 2. Die eigentliche Farbe (darüber liegend)
        Rectangle {
            anchors.fill: parent
            color: root.color
            }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: colorDialog.open()
            }
        }
    }
