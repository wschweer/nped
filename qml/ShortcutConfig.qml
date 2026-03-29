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
    Layout.fillWidth: true
    Layout.fillHeight: true
    SettingsCategory { title: "Keyboard Shortcuts" }
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: nped.shortcuts
        spacing: 0

        delegate: ItemDelegate {
            width: ListView.view.width
            height: 50
            required property int index
            required property var modelData
            highlighted: false

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 20

                Label {
                    text: modelData.description
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    font.bold: true
                    }
                TextField {
                    text: modelData.sequence
                    Layout.preferredWidth: 400
                    placeholderText: "Shortcut..."
                    // font.pixelSize: 14
                    onEditingFinished: {
                        var list = nped.shortcuts
                        list[index].sequence = text
                        nped.shortcuts = list // Trigger update
                        }
                    }
                }
            }
        }
    }
