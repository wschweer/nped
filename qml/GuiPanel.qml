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
            model: nped.monospacedFonts

            // 2. Aktuellen Font vorselektieren
            currentIndex: model.indexOf(nped.fontFamily)

            // 3. Bei Auswahl speichern
            onActivated: {
                nped.fontFamily = currentText
                nped.update()
                }

            // Optional: Popup-Liste scrollbar machen bei vielen Fonts
            popup.height: 300
            }
        }
    CheckBox {
        text: "DarkMode"
        checked: nped.darkMode
        onToggled: {
            nped.darkMode = checked;
            }
        }

    Item { Layout.fillHeight: true } // Spacer nach unten
    }
