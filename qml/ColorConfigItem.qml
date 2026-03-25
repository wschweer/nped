  import QtQuick
import QtQuick.Controls
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
            root.color = selectedColor
            root.colorChangedByUser(selectedColor)
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
        width: 40
        height: 25
        border.color: "#888"
        border.width: 1
        radius: 2
        clip: true // Damit das Schachmuster nicht über den Radius ragt

        // 1. Das Schachbrettmuster (für Transparenz-Sichtbarkeit)
/*
        ShaderEffect {
            anchors.fill: parent
            fragmentShader: "
                varying highp vec2 qt_TexCoord0;
                void main() {
                    lowp float check = mod(floor(qt_TexCoord0.x * 8.0) + floor(qt_TexCoord0.y * 5.0), 2.0);
                    gl_FragColor = mix(vec4(1.0), vec4(0.8), check);
                }
            "
        }
*/
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