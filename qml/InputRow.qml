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

RowLayout {
    id: root
    property alias label: labelObj.text

    // Das Input-Element (TextField, ComboBox, etc.) ist das Child dieses Objekts
    default property alias content: container.data

    spacing: 10
    Layout.fillWidth: true

    Label {
        id: labelObj
        Layout.preferredWidth: 130 // Feste Breite für bündige Ausrichtung
        Layout.alignment: Qt.AlignVCenter
        elide: Text.ElideRight
        color: palette.text
        }

    // Container für das eigentliche Control (TextField, CheckBox etc.)
    Item {
        id: container
        Layout.fillWidth: true
        Layout.preferredHeight: 32 // Standardhöhe
        Layout.alignment: Qt.AlignVCenter

        // Trick: Layout-Properties von Kindern (z.B. TextField) werden respektiert
        // Das erlaubt dem Kind 'Layout.fillWidth: true' zu setzen.
        }
    }