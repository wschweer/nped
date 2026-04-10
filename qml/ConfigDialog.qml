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
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: root
    implicitWidth: 1100
    implicitHeight: 650

    // Enhanced border and resizing area
    border.color: Material.theme === Material.Dark ? "#555555" : "#999999"
    border.width: 2
    radius: 4

    Material.theme: nped.darkMode ? Material.Dark : Material.Light
    color: Material.background

    // Haupt-Layout: Oben Inhalt, Unten Footer
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 0

        // --- 1. Der eigentliche Inhalt (Sidebar + Pages) ---
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Sidebar
            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 200
                color: Material.theme === Material.Dark ? "#333333" : "#f0f0f0"

                ListView {
                    id: navList
                    anchors.fill: parent
                    anchors.topMargin: 20
                    focus: true
                    clip: true
                    model: ["General & GUI", "Editor Shortcuts", "Light Text Styles", "Dark Text Styles", "File Types", "Language Servers", "AI Models"]
                    delegate: ItemDelegate {
                        width: ListView.view.width
                        height: 40
                        text: modelData
                        highlighted: ListView.isCurrentItem
                        onClicked: {
                            navList.currentIndex = index;
                            viewStack.currentIndex = index;
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

                    GuiPanel {}
                    ShortcutConfig {}
                    TextStyleConfig {
                        type: "Light"
                        }
                    TextStyleConfig {
                        type: "Dark"
                        }
                    FiletypeConfig {}
                    LsConfig {}
                    AiConfig {}
                    }
                }
            }
        // --- 2. Der Footer (Manuell gebaut) ---
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: Material.theme === Material.Dark ? "#333333" : "#f0f0f0"
            Rectangle {
                width: parent.width
                height: 1
                color: Material.theme === Material.Dark ? "#555555" : "#cccccc"
                anchors.top: parent.top
                }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 10

                Button {
                    text: "Reset Defaults"
                    flat: true
                    onClicked: nped.resetToDefaults()
                    }

                Item {
                    Layout.fillWidth: true
                    } // Spacer

                Button {
                    text: "Cancel"
                    onClicked: dialog.reject()
                    }
                Button {
                    text: "Save"
                    highlighted: true
                    onClicked: {
                        nped.apply();
                        dialog.accept();
                        }
                    }
                }
            }
        }
    }
