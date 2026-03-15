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
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        SettingsCategory { title: "Keyboard Shortcuts" }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: config.shortcuts
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
                                            var list = config.shortcuts
                                            list[index].sequence = text
                                            config.shortcuts = list // Trigger update
                                            }
                                        }
                                    }
                                }
                            }
                        }

                    // --- Page 3: File Types ---
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Label {
                            text: "File Associations"
                            font.pointSize: 18; font.bold: true
                            color: Material.foreground
                            }
                        Rectangle { height: 1; Layout.fillWidth: true; color: Material.accent }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: config.fileTypes
                            spacing: 15

                            // Footer: Add Button
                            footer: Button {
                                text: "+ Add File Type"
                                flat: true
                                Material.foreground: Material.accent
                                onClicked: {
                                    var list = config.fileTypes
                                    // Default Werte für neuen Eintrag
                                    list.push({
                                        extensions: "*.new",
                                        languageId: "text",
                                        languageServer: "none",
                                        tabSize: 4,
                                        parse: false
                                        })
                                    config.fileTypes = list
                                    }
                                }
                            delegate: Rectangle {
                                width: ListView.view.width - 20
                                // Automatische Höhe berechnen
                                height: ftLayout.implicitHeight + 30

                                // Card Style (Dark Surface)
                                color: Material.theme === Material.Dark ? "#424242" : "#ffffff"
                                border.color: Material.theme === Material.Dark ? "#616161" : "#e0e0e0"
                                radius: 6

                                required property int index
                                required property var modelData

                                GridLayout {
                                    id: ftLayout
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    columns: 2
                                    columnSpacing: 15
                                    rowSpacing: 10

                                    // --- Extensions ---
                                    Label { text: "Extensions:"; Layout.alignment: Qt.AlignRight }
                                    TextField {
                                        text: modelData.extensions
                                        Layout.fillWidth: true
                                        placeholderText: "*.cpp;*.h"
                                        selectByMouse: true
                                        onEditingFinished: {
                                            var l=config.fileTypes; l[index].extensions=text; config.fileTypes=l
                                            }
                                        }

                                    // --- Language ID & Server ---
                                    Label { text: "Language ID:"; Layout.alignment: Qt.AlignRight }
                                    TextField {
                                        text: modelData.languageId
                                        Layout.fillWidth: true
                                        placeholderText: "cpp, python..."
                                        onEditingFinished: {
                                            var l=config.fileTypes; l[index].languageId=text; config.fileTypes=l
                                            }
                                        }

                                    Label { text: "LSP Server:"; Layout.alignment: Qt.AlignRight }
                                    TextField {
                                        text: modelData.languageServer
                                        Layout.fillWidth: true
                                        placeholderText: "clangd, pylsp..."
                                        onEditingFinished: {
                                            var l=config.fileTypes; l[index].languageServer=text; config.fileTypes=l
                                            }
                                        }

                                    // --- Settings (Row mit SpinBox und Switch) ---
                                    Label { text: "Tab Size:"; Layout.alignment: Qt.AlignRight }
                                    RowLayout {
                                        Layout.fillWidth: true

                                        SpinBox {
                                            from: 1; to: 16
                                            value: modelData.tabSize
                                            editable: true
                                            Layout.preferredWidth: 100
                                            onValueModified: {
                                                var l=config.fileTypes; l[index].tabSize=value; config.fileTypes=l
                                                }
                                            }

                                        // Spacer
                                        Item { width: 20 }

                                        Label { text: "Enable LSP:" }
                                        Switch {
                                            checked: modelData.parse
                                            onToggled: {
                                                var l=config.fileTypes; l[index].parse=checked; config.fileTypes=l
                                                }
                                            }

                                        Item { Layout.fillWidth: true } // Spacer nach rechts
                                        }

                                    // --- Action Buttons ---
                                    Item { Layout.columnSpan: 2; height: 5 } // Kleiner Abstand

                                    Button {
                                        Layout.columnSpan: 2
                                        Layout.alignment: Qt.AlignRight
                                        text: "Remove"
                                        flat: true
                                        Material.foreground: Material.Red
                                        onClicked: {
                                            var list = config.fileTypes
                                            list.splice(index, 1)
                                            config.fileTypes = list
                                            }
                                        }
                                    }
                                }
                            }
                        }

                    // --- Page 4: Language Servers ---
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Label {
                            text: "Language Servers (LSP)"
                            font.pointSize: 18; font.bold: true
                            color: Material.foreground
                            }
                        Rectangle { height: 1; Layout.fillWidth: true; color: Material.accent }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: config.languageServersConfig
                            spacing: 10

                            // Footer: Button zum Hinzufügen
                            footer: Button {
                                text: "+ Add Server"
                                flat: true
                                Material.foreground: Material.accent
                                onClicked: {
                                    var list = config.languageServersConfig
                                    list.push({name: "New Server", command: "/usr/bin/...", args: ""})
                                    config.languageServersConfig = list
                                    }
                                }

                            delegate: Rectangle {
                                width: ListView.view.width - 20 // Etwas Rand lassen
                                height: serverLayout.implicitHeight + 30
                                anchors.horizontalCenter: parent.horizontalCenter

                                // Card Styling (Surface Color)
                                color: Material.theme === Material.Dark ? "#424242" : "#ffffff"
                                border.color: Material.theme === Material.Dark ? "#616161" : "#e0e0e0"
                                radius: 6

                                required property int index
                                required property var modelData

                                GridLayout {
                                    id: serverLayout
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    columns: 2
                                    columnSpacing: 15
                                    rowSpacing: 10

                                    // Name
                                    Label { text: "Name:"; Layout.alignment: Qt.AlignRight }
                                    TextField {
                                        text: modelData.name
                                        Layout.fillWidth: true
                                        placeholderText: "e.g. clangd"
                                        selectByMouse: true
                                        onEditingFinished: {
                                            var l=config.languageServersConfig; l[index].name=text; config.languageServersConfig=l
                                            }
                                        }

                                    // Command
                                    Label { text: "Command:"; Layout.alignment: Qt.AlignRight }
                                    TextField {
                                        text: modelData.command
                                        Layout.fillWidth: true
                                        placeholderText: "/path/to/executable"
                                        selectByMouse: true
                                        onEditingFinished: {
                                            var l=config.languageServersConfig; l[index].command=text; config.languageServersConfig=l
                                            }
                                        }

                                    // Args
                                    Label { text: "Arguments:"; Layout.alignment: Qt.AlignRight }
                                    TextField {
                                        text: modelData.args
                                        Layout.fillWidth: true
                                        placeholderText: "--stdio --log=verbose"
                                        selectByMouse: true
                                        onEditingFinished: {
                                            var l=config.languageServersConfig; l[index].args=text; config.languageServersConfig=l
                                            }
                                        }

                                    // Delete Button (Optional, oben rechts in der Karte)
                                    Button {
                                        Layout.columnSpan: 2
                                        Layout.alignment: Qt.AlignRight
                                        text: "Remove"
                                        flat: true
                                        Material.foreground: Material.Red
                                        onClicked: {
                                            var list = config.languageServersConfig
                                            list.splice(index, 1)
                                            config.languageServersConfig = list
                                            }
                                        }
                                    }
                                }
                            }
                        }

                    // --- Page 5: AI Models ---
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Label {
                            text: "AI Models (LLM)"
                            font.pointSize: 18; font.bold: true
                            color: Material.foreground
                            }
                        Rectangle { height: 1; Layout.fillWidth: true; color: Material.accent }

                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: config.aiModels
                            spacing: 15

                            footer: Button {
                                text: "+ Add Model"
                                flat: true
                                Material.foreground: Material.accent
                                onClicked: {
                                    var list = config.aiModels
                                    list.push({name: "New Model", interface: "ollama", baseUrl: "http://localhost:11434", apiKey: "", isLocal: true})
                                    config.aiModels = list
                                    }
                                }

                            delegate: Rectangle {
                                width: ListView.view.width - 20
                                height: aiLayout.implicitHeight + 30
                                anchors.horizontalCenter: parent.horizontalCenter

                                // Card Styling
                                color: Material.theme === Material.Dark ? "#424242" : "#ffffff"
                                border.color: Material.theme === Material.Dark ? "#616161" : "#e0e0e0"
                                radius: 6

                                // Kleiner Schatten-Effekt (optional, wenn gewünscht)
                                layer.enabled: true

                                required property int index
                                required property var modelData

                                GridLayout {
                                    id: aiLayout
                                    anchors.fill: parent
                                    anchors.margins: 15
                                    columns: 2
                                    columnSpacing: 15
                                    rowSpacing: 10

                                    // --- Header Row: Name & Interface ---
                                    Label { text: "Name:"; Layout.alignment: Qt.AlignRight }
                                    TextField {
                                        text: modelData.name
                                        Layout.fillWidth: true
                                        font.bold: true
                                        onEditingFinished: { var l=config.aiModels; l[index].name=text; config.aiModels=l }
                                        }

                                    Label { text: "Interface:"; Layout.alignment: Qt.AlignRight }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: ["ollama", "gemini", "anthropic", "openai"]

                                        // Setzt den Index basierend auf dem String im Model
                                        currentIndex: indexOfValue(modelData.interface)

                                        onActivated: {
                                            var l=config.aiModels; l[index].interface=currentText; config.aiModels=l
                                            }
                                        }

                                    // --- Connection Details ---
                                    Label { text: "URL:"; Layout.alignment: Qt.AlignRight }
                                    TextField {
                                        text: modelData.baseUrl
                                        Layout.fillWidth: true
                                        placeholderText: "https://api.example.com"
                                        onEditingFinished: { var l=config.aiModels; l[index].baseUrl=text; config.aiModels=l }
                                        }

                                    Label { text: "API Key:"; Layout.alignment: Qt.AlignRight }
                                    TextField {
                                        text: modelData.apiKey
                                        Layout.fillWidth: true
                                        echoMode: TextInput.Password // Versteckt den Key
                                        placeholderText: "sk-..."
                                        onEditingFinished: { var l=config.aiModels; l[index].apiKey=text; config.aiModels=l }
                                        }

                                    // --- Switches & Actions ---
                                    Label { text: "Local Run:"; Layout.alignment: Qt.AlignRight }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Switch {
                                            checked: modelData.isLocal
                                            onToggled: { var l=config.aiModels; l[index].isLocal=checked; config.aiModels=l }
                                            }

                                        Item { Layout.fillWidth: true } // Spacer

                                        Button {
                                            text: "Delete"
                                            flat: true
                                            Material.foreground: Material.Red
                                            onClicked: {
                                                var list = config.aiModels
                                                list.splice(index, 1)
                                                config.aiModels = list
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
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