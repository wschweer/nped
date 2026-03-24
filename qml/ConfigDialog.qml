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
import Nped.Config

Rectangle {
    id: root

    Material.theme: config.darkMode ? Material.Dark : Material.Light

    // Haupt-Layout: Oben Inhalt, Unten Footer
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // --- 1. Der eigentliche Inhalt (Sidebar + Pages) ---
        // Dieser Teil nimmt den gesamten verfügbaren Platz ein (fillHeight: true)
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Sidebar
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 200
                color: Material.theme === Material.Dark ? "#424242" : "#eeeeee"

                ListView {
                    id: navList
                    anchors.fill: parent
                    anchors.topMargin: 20
                    focus: true
                    clip: true
                    model: ["General & GUI", "Editor Shortcuts", "File Types", "Language Servers", "AI Models"]
                    delegate: ItemDelegate {
                        width: ListView.view.width
                        height: 40
                        text: modelData
                        highlighted: ListView.isCurrentItem
                        onClicked: {
                            navList.currentIndex = index
                            viewStack.currentIndex = index
                            }
                        font.weight: ListView.isCurrentItem ? Font.DemiBold : Font.Normal
                        }
                    }
                }

            // Content Area
            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                padding: 0
                background: Rectangle {
                    color: Material.background
                    }

                StackLayout {
                    id: viewStack
                    anchors.fill: parent
                    anchors.margins: 20
                    currentIndex: navList.currentIndex

                    // --- Page 1: GUI ---
                    GuiPanel { }

                    // --- Page 2: Shortcuts ---
                    ShortcutConfig {}

                    // --- Page 3: File Types ---
                    FiletypeConfig {}

                    // --- Page 4: Language Servers ---
                    LsConfig {}

                    // --- Page 5: AI Models ---
                    AiConfig {}
                    }
                }
            }
        // --- 2. Der Footer (Manuell gebaut) ---
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
             color: Material.theme === Material.Dark ? "#424242" : "#eeeeee"
            Rectangle {
                width: parent.width
                height: 1
                color: Material.theme === Material.Dark ? "#616161" : "#bdbdbd"
                anchors.top: parent.top
                }
            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 10

                Button {
                    text: "Reset Defaults"
                    flat: true
                    onClicked: config.resetToDefaults()
                    }

                Item { Layout.fillWidth: true } // Spacer

                Button {
                    text: "Cancel"
                    // Ruft die C++ Methode reject() auf (via context property)
                    onClicked: dialog.reject()
                    }
                Button {
                    text: "Save"
                    highlighted: true
                    onClicked: {
                        config.apply()
                        dialog.accept()
                        }
                    }
                }
            }
        }
    }
