//=============================================================================
//  nped Program Editor
//
//  Copyright (C) 2026 Werner Schweer
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2
//  as published by the Free Software Foundation and appearing in
//  the file LICENCE.GPL
//=============================================================================

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ColumnLayout {
    SettingsCategory { title: "GUI Appearance" }

    GridLayout {
        columns: 2
        rowSpacing: 20
        columnSpacing: 10
        Layout.topMargin: 20

        // --- FONT SELECTION ---
        Label {
            text: "Editor Font:"
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            }

        ComboBox {
            id: fontCombo
            Layout.fillWidth: true
            Layout.preferredWidth: 300 // Begrenzung, damit es nicht zu breit wird

            // 1. Daten aus C++ (Nur Monospace Fonts)
            model: config.monospacedFonts

            // 2. Aktuellen Font vorselektieren
            currentIndex: model.indexOf(config.fontFamily)

            // 3. Bei Auswahl speichern
            onActivated: {
                config.fontFamily = currentText
                }

            // Optional: Popup-Liste scrollbar machen bei vielen Fonts
            popup.height: 300
            }
        }
    CheckBox {
        text: "DarkMode"
        checked: config.darkMode
        onToggled: {
            config.darkMode = checked;
            }
        }
    Row {
        spacing: 20
        height: 50

        Label {
            text: "Background Color"
            Layout.alignment: Qt.AlignVCenter
            }
        Rectangle {
            width: 50
            height: 50
            radius: 25
            color: colorDialog.selectedColor
            border.color: "grey"
            border.width: 1

            MouseArea {
                anchors.fill: parent
                onClicked: colorDialog.open()
                cursorShape: Qt.PointingHandCursor
                }
            }
        }

    ColorDialog {
        id: colorDialog
        title: "Bitte wähle eine Farbe"
        selectedColor: config.bgColor

        onAccepted: {
            console.log("Benutzer hat gewählt: " + selectedColor)
            config.bgColor = selectedColor
            }
        }

    Item { Layout.fillHeight: true } // Spacer nach unten
    }
