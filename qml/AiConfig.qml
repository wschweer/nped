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

ColumnLayout {
    Label {
        text: "AI Models (LLM)"
        font.pointSize: 18
        font.bold: true
        color: Material.foreground
        }
    Rectangle {
        implicitHeight: 1
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
                    model: nped.modelsModel

                    Component.onCompleted: {
                        if (count > 0)
                            currentIndex = 0;
                        }

                    delegate: ItemDelegate {
                        id: delegate
                        required property var modelData
                        required property var index
                        width: ListView.view.width
                        height: 40
                        text: modelData
                        highlighted: ListView.isCurrentItem
                        onClicked: ListView.view.currentIndex = index
                        font.weight: ListView.isCurrentItem ? Font.DemiBold : Font.Normal

                        Label {
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: delegate.modelData.isLocal ? "(Local)" : ""
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
                        nped.addModel();
                        modelListView.currentIndex = nped.modelsModel.length - 1;
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
                id: cl
                anchors.fill: parent
                spacing: 15
                anchors.margins: 10

                property var model: modelListView.currentIndex >= 0 && modelListView.currentIndex < nped.modelsModel.length ? nped.model(modelListView.currentIndex) : null
                visible: model !== null

                Label {
                    text: "Edit Model: " + (cl.model ? cl.model.name : "")
                    font.bold: true
                    font.pointSize: 14
                    }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 15
                    rowSpacing: 10

                    // Name
                    Label {
                        text: "Name:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: cl.model ? cl.model.name : ""
                        Layout.fillWidth: true
                        font.bold: true
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.name = text;
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // API
                    Label {
                        text: "Api:"
                        Layout.alignment: Qt.AlignRight
                        }
                    ComboBox {
                        Layout.fillWidth: true
                        model: ["ollama", "gemini", "anthropic", "openai"]
                        currentIndex: {
                            if (cl.model) {
                                var idx = find(cl.model.api);
                                return idx >= 0 ? idx : 0;
                                }
                            return 0;
                            }
                        onActivated: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.api = currentText;
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // Model ID
                    Label {
                        text: "Model ID:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: cl.model ? cl.model.modelIdentifier : ""
                        Layout.fillWidth: true
                        placeholderText: "e.g. gpt-4, llama3"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.modelIdentifier = text;
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // URL
                    Label {
                        text: "URL:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: cl.model ? cl.model.baseUrl : ""
                        Layout.fillWidth: true
                        placeholderText: "https://api.example.com"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.baseUrl = text;
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // API Key
                    Label {
                        text: "API Key:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: cl.model ? cl.model.apiKey : ""
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: "sk-..."
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.apiKey = text;
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // Temperature
                    Label {
                        text: "Temperature:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: cl.model ? cl.model.temperature.toString() : ""
                        Layout.fillWidth: true
                        placeholderText: "-1.0 for default"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.temperature = parseFloat(text);
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // Top P
                    Label {
                        text: "Top P:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: cl.model ? cl.model.topP.toString() : ""
                        Layout.fillWidth: true
                        placeholderText: "-1.0 for default"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.topP = parseFloat(text);
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // Top K
                    Label {
                        text: "Top K:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: cl.model ? cl.model.topK.toString() : ""
                        Layout.fillWidth: true
                        placeholderText: "-1.0 for default"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.topK = parseFloat(text);
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // Max Tokens
                    Label {
                        text: "Max Tokens:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: {
                            cl.model ? cl.model.maxTokens.toString() : "";
                            }
                        Layout.fillWidth: true
                        placeholderText: "-1 for default"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.maxTokens = parseInt(text);
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // num_ctx
                    Label {
                        text: "num_ctx:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: cl.model ? cl.model.num_ctx.toString() : ""
                        Layout.fillWidth: true
                        placeholderText: "-1 for default"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.num_ctx = parseInt(text);
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // Max Tokens
                    Label {
                        text: "num_predict:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: cl.model ? cl.model.num_predict.toString() : ""
                        Layout.fillWidth: true
                        placeholderText: "-1 for default"
                        onEditingFinished: {
                            if (modelListView.currentIndex >= 0) {
                                cl.model.num_predict = parseInt(text);
                                nped.setModel(modelListView.currentIndex, cl.model);
                                }
                            }
                        }

                    // Options
                    Label {
                        text: "Options:"
                        Layout.alignment: Qt.AlignRight
                        }
                    RowLayout {
                        Layout.fillWidth: true
                        CheckBox {
                            text: "Supports Thinking"
                            checked: cl.model ? cl.model.supportsThinking : false
                            onToggled: {
                                if (modelListView.currentIndex >= 0) {
                                    cl.model.supportsThinking = checked;
                                    nped.setModel(modelListView.currentIndex, cl.model);
                                    }
                                }
                            }
                        CheckBox {
                            text: "Stream"
                            checked: cl.model ? cl.model.stream : false
                            onToggled: {
                                if (modelListView.currentIndex >= 0) {
                                    cl.model.stream = checked;
                                    nped.setModel(modelListView.currentIndex, cl.model);
                                    }
                                }
                            }
                        }
                    }
                Item {
                    implicitHeight: 10
                    }

                Button {
                    Layout.alignment: Qt.AlignRight
                    text: "- Delete Model"
                    flat: true
                    Material.foreground: Material.Red
                    visible: cl.model && !cl.model.ollama
                    onClicked: {
                        if (modelListView.currentIndex >= 0) {
                            nped.removeModel(cl.model);
                            modelListView.currentIndex = Math.min(modelListView.currentIndex, list.length - 1);
                            }
                        }
                    }
                }
            }
        }
    }
