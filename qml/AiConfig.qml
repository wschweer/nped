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
    Label {
        text: "AI Models (LLM)"
        font.pointSize: 18; font.bold: true
        color: Material.foreground
        }
    Rectangle {
        height: 1
        Layout.fillWidth: true
        color: Material.accent
        }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 15

        // Left Side: Model List
        Frame {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            padding: 0
            background: Rectangle {
                color: Material.theme === Material.Dark ? "#424242" : "#eeeeee"
                border.color: Material.theme === Material.Dark ? "#616161" : "#bdbdbd"
                radius: 4
                }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                ListView {
                    id: modelListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: nped.models

                    Component.onCompleted: {
                        if (count > 0) currentIndex = 0
                        }

                    delegate: ItemDelegate {
                        width: ListView.view.width
                        height: 40
                        text: modelData.name
                        highlighted: ListView.isCurrentItem
                        onClicked: ListView.view.currentIndex = index
                        font.weight: ListView.isCurrentItem ? Font.DemiBold : Font.Normal

                        Label {
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.isLocal ? "(Local)" : ""
                            font.pixelSize: 10
                            color: Material.hintTextColor
                            }
                        }
                    }

                Button {
                    Layout.fillWidth: true
                    text: "+ Add Model"
                    flat: true
                    Material.foreground: Material.accent
                    onClicked: {
                        var list = nped.models
                        list.push({name: "New Model", modelIdentifier: "", api: "ollama", baseUrl: "http://localhost:11434", apiKey: "", isLocal: false, supportsThinking: false, temperature: -1.0, topP: -1.0, maxTokens: -1})
                        nped.models = list
                        modelListView.currentIndex = list.length - 1
                        }
                    }
                }
            }

        // Right Side: Details of the selected model
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                anchors.fill: parent
                spacing: 15
                anchors.margins: 10

                property var currentModel: modelListView.currentIndex >= 0 && modelListView.currentIndex < nped.models.length ? nped.models[modelListView.currentIndex] : null

                visible: currentModel !== null

                Label {
                    text: "Edit Model: " + (parent.currentModel ? parent.currentModel.name : "")
                    font.bold: true
                    font.pointSize: 14
                    }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 15
                    rowSpacing: 10

                    // Name
                    Label { text: "Name:"; Layout.alignment: Qt.AlignRight }
                    TextField {
                        text: parent.parent.currentModel ? parent.parent.currentModel.name : ""
                        Layout.fillWidth: true
                        font.bold: true
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                var l = nped.models;
                                l[modelListView.currentIndex].name = text;
                                nped.models = l;
                            }
                        }
                    }

                    // API
                    Label { text: "Api:"; Layout.alignment: Qt.AlignRight }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["ollama", "gemini", "anthropic", "openai"]
                        currentIndex: {
                            if (parent.parent.currentModel) {
                                var idx = find(parent.parent.currentModel.api);
                                return idx >= 0 ? idx : 0;
                            }
                            return 0;
                        }
                        onActivated: {
                            if (modelListView.currentIndex >= 0) {
                                var l = nped.models;
                                l[modelListView.currentIndex].api = currentText;
                                nped.models = l;
                            }
                        }
                    }

                    // Model ID
                    Label { text: "Model ID:"; Layout.alignment: Qt.AlignRight }
                    TextField {
                        text: parent.parent.currentModel ? parent.parent.currentModel.modelIdentifier : ""
                        Layout.fillWidth: true
                        placeholderText: "e.g. gpt-4, llama3"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                var l = nped.models;
                                l[modelListView.currentIndex].modelIdentifier = text;
                                nped.models = l;
                            }
                        }
                    }

                    // URL
                    Label { text: "URL:"; Layout.alignment: Qt.AlignRight }
                    TextField {
                        text: parent.parent.currentModel ? parent.parent.currentModel.baseUrl : ""
                        Layout.fillWidth: true
                        placeholderText: "https://api.example.com"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                var l = nped.models;
                                l[modelListView.currentIndex].baseUrl = text;
                                nped.models = l;
                            }
                        }
                    }

                    // API Key
                    Label { text: "API Key:"; Layout.alignment: Qt.AlignRight }
                    TextField {
                        text: parent.parent.currentModel ? parent.parent.currentModel.apiKey : ""
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: "sk-..."
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                var l = nped.models;
                                l[modelListView.currentIndex].apiKey = text;
                                nped.models = l;
                            }
                        }
                    }

                    // Temperature
                    Label { text: "Temperature:"; Layout.alignment: Qt.AlignRight }
                    TextField {
                        text: parent.parent.currentModel ? parent.parent.currentModel.temperature.toString() : ""
                        Layout.fillWidth: true
                        placeholderText: "-1.0 for default"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                var l = nped.models;
                                l[modelListView.currentIndex].temperature = parseFloat(text);
                                nped.models = l;
                            }
                        }
                    }

                    // Top P
                    Label { text: "Top P:"; Layout.alignment: Qt.AlignRight }
                    TextField {
                        text: parent.parent.currentModel ? parent.parent.currentModel.topP.toString() : ""
                        Layout.fillWidth: true
                        placeholderText: "-1.0 for default"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                var l = nped.models;
                                l[modelListView.currentIndex].topP = parseFloat(text);
                                nped.models = l;
                            }
                        }
                    }

                    // Max Tokens
                    Label { text: "Max Tokens:"; Layout.alignment: Qt.AlignRight }
                    TextField {
                        text: parent.parent.currentModel ? parent.parent.currentModel.maxTokens.toString() : ""
                        Layout.fillWidth: true
                        placeholderText: "-1 for default"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                var l = nped.models;
                                l[modelListView.currentIndex].maxTokens = parseInt(text);
                                nped.models = l;
                            }
                        }
                    }

                    // Options
                    Label { text: "Options:"; Layout.alignment: Qt.AlignRight }
                    RowLayout {
                        Layout.fillWidth: true
                        CheckBox {
                            text: "Supports Thinking"
                            checked: parent.parent.parent.currentModel ? parent.parent.parent.currentModel.supportsThinking : false
                            onToggled: {
                                if (modelListView.currentIndex >= 0) {
                                    var l = nped.models;
                                    l[modelListView.currentIndex].supportsThinking = checked;
                                    nped.models = l;
                                }
                            }
                        }
                    }
                }
                Item { height: 10 }

                Button {
                    Layout.alignment: Qt.AlignRight
                    text: "Delete Model"
                    flat: true
                    Material.foreground: Material.Red
                    visible: parent.currentModel && !parent.currentModel.isLocal
                    onClicked: {
                        if (modelListView.currentIndex >= 0) {
                            var list = nped.models;
                            list.splice(modelListView.currentIndex, 1);
                            nped.models = list;
                            modelListView.currentIndex = Math.min(modelListView.currentIndex, list.length - 1);
                            }
                        }
                    }
                }
            }
        }
    }
