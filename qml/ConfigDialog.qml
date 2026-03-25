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
    width: 1100
    height: 650

    // Enhanced border and resizing area
    border.color: Material.theme === Material.Dark ? "#555555" : "#999999"
    border.width: 2
    radius: 4

    Material.theme: config.darkMode ? Material.Dark : Material.Light
    color: Material.background

    // Grip for visual indication
    Rectangle {
        width: 15
        height: 15
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: "transparent"

        // Small visual indicator for the resize handle
        Rectangle {
            width: 8
            height: 8
            anchors.centerIn: parent
            color: Material.theme === Material.Dark ? "#777777" : "#aaaaaa"
            radius: 2
            }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeFDiagCursor
            property point clickPos: "0,0"

            onPressed: {
                clickPos = Qt.point(mouseX, mouseY)
                }
            onPositionChanged: {
                if (pressed) {
                    let delta = Qt.point(mouseX - clickPos.x, mouseY - clickPos.y)
                    let newWidth = Math.max(600, root.width + delta.x)
                    let newHeight = Math.max(400, root.height + delta.y)

                    // Directly resize the QML component
                    root.width = newWidth
                    root.height = newHeight

                    // Signal the C++ wrapper to resize its container
                    // Since root is the root object, its size changes here,
                    // but the QQuickWidget size needs to sync.
                    // QQuickWidget.SizeRootObjectToView handles this automatically.
                }
            }
        }
    }

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
                    model: ["General & GUI", "Editor Shortcuts", "Light Text Styles",
                        "Dark Text Styles", "File Types", "Language Servers", "AI Models"]
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

                    GuiPanel { }
                    ShortcutConfig {}
                    TextStyleConfig { type: "Light" }
                    TextStyleConfig { type: "Dark" }
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
                    onClicked: config.resetToDefaults()
                    }

                Item { Layout.fillWidth: true } // Spacer

                Button {
                    text: "Cancel"
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
