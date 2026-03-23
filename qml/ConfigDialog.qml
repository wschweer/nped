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

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 15

                            // Left Side: File Types List
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
                                        id: fileTypeListView
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        model: config.fileTypes
                                        
                                        Component.onCompleted: {
                                            if (count > 0) currentIndex = 0
                                        }
                                        
                                        delegate: ItemDelegate {
                                            width: ListView.view.width
                                            height: 40
                                            text: modelData.extensions || "New Type"
                                            highlighted: ListView.isCurrentItem
                                            onClicked: ListView.view.currentIndex = index
                                            font.weight: ListView.isCurrentItem ? Font.DemiBold : Font.Normal
                                        }
                                    }

                                    Button {
                                        Layout.fillWidth: true
                                        text: "+ Add File Type"
                                        flat: true
                                        Material.foreground: Material.accent
                                        onClicked: {
                                            var list = config.fileTypes
                                            list.push({
                                                extensions: "*.new",
                                                languageId: "text",
                                                languageServer: "none",
                                                tabSize: 4,
                                                parse: false
                                            })
                                            config.fileTypes = list
                                            fileTypeListView.currentIndex = list.length - 1
                                        }
                                    }
                                }
                            }

                            // Right Side: Details
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: parent.width - 20
                                    spacing: 15

                                    property var currentFileType: fileTypeListView.currentIndex >= 0 && fileTypeListView.currentIndex < config.fileTypes.length ? config.fileTypes[fileTypeListView.currentIndex] : null
                                    
                                    visible: currentFileType !== null
                                    
                                    Label {
                                        text: "Edit File Type: " + (parent.currentFileType ? parent.currentFileType.extensions : "")
                                        font.bold: true
                                        font.pointSize: 14
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: 2
                                        columnSpacing: 15
                                        rowSpacing: 10

                                        // Extensions
                                        Label { text: "Extensions:"; Layout.alignment: Qt.AlignRight }
                                        TextField {
                                            text: parent.parent.currentFileType ? parent.parent.currentFileType.extensions : ""
                                            Layout.fillWidth: true
                                            placeholderText: "*.cpp;*.h"
                                            onEditingFinished: {
                                                if (fileTypeListView.currentIndex >= 0) {
                                                    var l = config.fileTypes;
                                                    l[fileTypeListView.currentIndex].extensions = text;
                                                    config.fileTypes = l;
                                                }
                                            }
                                        }

                                        // Language ID
                                        Label { text: "Language ID:"; Layout.alignment: Qt.AlignRight }
                                        TextField {
                                            text: parent.parent.currentFileType ? parent.parent.currentFileType.languageId : ""
                                            Layout.fillWidth: true
                                            placeholderText: "cpp, python..."
                                            onEditingFinished: {
                                                if (fileTypeListView.currentIndex >= 0) {
                                                    var l = config.fileTypes;
                                                    l[fileTypeListView.currentIndex].languageId = text;
                                                    config.fileTypes = l;
                                                }
                                            }
                                        }

                                        // Language Server
                                        Label { text: "LSP Server:"; Layout.alignment: Qt.AlignRight }
                                        TextField {
                                            text: parent.parent.currentFileType ? parent.parent.currentFileType.languageServer : ""
                                            Layout.fillWidth: true
                                            placeholderText: "clangd, pylsp..."
                                            onEditingFinished: {
                                                if (fileTypeListView.currentIndex >= 0) {
                                                    var l = config.fileTypes;
                                                    l[fileTypeListView.currentIndex].languageServer = text;
                                                    config.fileTypes = l;
                                                }
                                            }
                                        }

                                        // Tab Size
                                        Label { text: "Tab Size:"; Layout.alignment: Qt.AlignRight }
                                        SpinBox {
                                            from: 1; to: 16
                                            value: parent.parent.currentFileType ? parent.parent.currentFileType.tabSize : 4
                                            editable: true
                                            Layout.preferredWidth: 100
                                            onValueModified: {
                                                if (fileTypeListView.currentIndex >= 0) {
                                                    var l = config.fileTypes;
                                                    l[fileTypeListView.currentIndex].tabSize = value;
                                                    config.fileTypes = l;
                                                }
                                            }
                                        }

                                        // Parse (Enable LSP)
                                        Label { text: "Enable LSP:"; Layout.alignment: Qt.AlignRight }
                                        Switch {
                                            checked: parent.parent.currentFileType ? parent.parent.currentFileType.parse : false
                                            onToggled: {
                                                if (fileTypeListView.currentIndex >= 0) {
                                                    var l = config.fileTypes;
                                                    l[fileTypeListView.currentIndex].parse = checked;
                                                    config.fileTypes = l;
                                                }
                                            }
                                        }
                                    }

                                    Item { height: 10 }

                                    Button {
                                        Layout.alignment: Qt.AlignRight
                                        text: "Delete File Type"
                                        flat: true
                                        Material.foreground: Material.Red
                                        onClicked: {
                                            if (fileTypeListView.currentIndex >= 0) {
                                                var list = config.fileTypes;
                                                list.splice(fileTypeListView.currentIndex, 1);
                                                config.fileTypes = list;
                                                fileTypeListView.currentIndex = Math.min(fileTypeListView.currentIndex, list.length - 1);
                                            }
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

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 15

                            // Left Side: Server List
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
                                        id: serverListView
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        model: config.languageServersConfig
                                        
                                        Component.onCompleted: {
                                            if (count > 0) currentIndex = 0
                                        }
                                        
                                        delegate: ItemDelegate {
                                            width: ListView.view.width
                                            height: 40
                                            text: modelData.name || "New Server"
                                            highlighted: ListView.isCurrentItem
                                            onClicked: ListView.view.currentIndex = index
                                            font.weight: ListView.isCurrentItem ? Font.DemiBold : Font.Normal
                                        }
                                    }

                                    Button {
                                        Layout.fillWidth: true
                                        text: "+ Add Server"
                                        flat: true
                                        Material.foreground: Material.accent
                                        onClicked: {
                                            var list = config.languageServersConfig
                                            list.push({name: "New Server", command: "/usr/bin/...", args: ""})
                                            config.languageServersConfig = list
                                            serverListView.currentIndex = list.length - 1
                                        }
                                    }
                                }
                            }

                            // Right Side: Details
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: parent.width - 20
                                    spacing: 15

                                    property var currentServer: serverListView.currentIndex >= 0 && serverListView.currentIndex < config.languageServersConfig.length ? config.languageServersConfig[serverListView.currentIndex] : null
                                    
                                    visible: currentServer !== null
                                    
                                    Label {
                                        text: "Edit Server: " + (parent.currentServer ? parent.currentServer.name : "")
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
                                            text: parent.parent.currentServer ? parent.parent.currentServer.name : ""
                                            Layout.fillWidth: true
                                            placeholderText: "e.g. clangd"
                                            onEditingFinished: {
                                                if (serverListView.currentIndex >= 0) {
                                                    var l = config.languageServersConfig;
                                                    l[serverListView.currentIndex].name = text;
                                                    config.languageServersConfig = l;
                                                }
                                            }
                                        }

                                        // Command
                                        Label { text: "Command:"; Layout.alignment: Qt.AlignRight }
                                        TextField {
                                            text: parent.parent.currentServer ? parent.parent.currentServer.command : ""
                                            Layout.fillWidth: true
                                            placeholderText: "/path/to/executable"
                                            onEditingFinished: {
                                                if (serverListView.currentIndex >= 0) {
                                                    var l = config.languageServersConfig;
                                                    l[serverListView.currentIndex].command = text;
                                                    config.languageServersConfig = l;
                                                }
                                            }
                                        }

                                        // Arguments
                                        Label { text: "Arguments:"; Layout.alignment: Qt.AlignRight }
                                        TextField {
                                            text: parent.parent.currentServer ? parent.parent.currentServer.args : ""
                                            Layout.fillWidth: true
                                            placeholderText: "--stdio --log=verbose"
                                            onEditingFinished: {
                                                if (serverListView.currentIndex >= 0) {
                                                    var l = config.languageServersConfig;
                                                    l[serverListView.currentIndex].args = text;
                                                    config.languageServersConfig = l;
                                                }
                                            }
                                        }
                                    }

                                    Item { height: 10 }

                                    Button {
                                        Layout.alignment: Qt.AlignRight
                                        text: "Delete Server"
                                        flat: true
                                        Material.foreground: Material.Red
                                        onClicked: {
                                            if (serverListView.currentIndex >= 0) {
                                                var list = config.languageServersConfig;
                                                list.splice(serverListView.currentIndex, 1);
                                                config.languageServersConfig = list;
                                                serverListView.currentIndex = Math.min(serverListView.currentIndex, list.length - 1);
                                            }
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
                                        model: agent.models
                                        
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
                                            var list = agent.models
                                            list.push({name: "New Model", modelIdentifier: "", api: "ollama", baseUrl: "http://localhost:11434", apiKey: "", isLocal: false, filterToolMessages: true, filterThoughts: false, supportsThinking: false, temperature: -1.0, topP: -1.0, maxTokens: -1})
                                            agent.models = list
                                            modelListView.currentIndex = list.length - 1
                                        }
                                    }
                                }
                            }

                            // Right Side: Details of the selected model
                            ScrollView {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true
                                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                ColumnLayout {
                                    width: parent.width - 20
                                    spacing: 15

                                    property var currentModel: modelListView.currentIndex >= 0 && modelListView.currentIndex < agent.models.length ? agent.models[modelListView.currentIndex] : null
                                    
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
                                                    var l = agent.models;
                                                    l[modelListView.currentIndex].name = text;
                                                    agent.models = l;
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
                                                    var l = agent.models;
                                                    l[modelListView.currentIndex].api = currentText;
                                                    agent.models = l;
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
                                                    var l = agent.models;
                                                    l[modelListView.currentIndex].modelIdentifier = text;
                                                    agent.models = l;
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
                                                    var l = agent.models;
                                                    l[modelListView.currentIndex].baseUrl = text;
                                                    agent.models = l;
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
                                                    var l = agent.models;
                                                    l[modelListView.currentIndex].apiKey = text;
                                                    agent.models = l;
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
                                                    var l = agent.models;
                                                    l[modelListView.currentIndex].temperature = parseFloat(text);
                                                    agent.models = l;
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
                                                    var l = agent.models;
                                                    l[modelListView.currentIndex].topP = parseFloat(text);
                                                    agent.models = l;
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
                                                    var l = agent.models;
                                                    l[modelListView.currentIndex].maxTokens = parseInt(text);
                                                    agent.models = l;
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
                                                        var l = agent.models;
                                                        l[modelListView.currentIndex].supportsThinking = checked;
                                                        agent.models = l;
                                                    }
                                                }
                                            }
                                            CheckBox {
                                                text: "Filter Tools"
                                                checked: parent.parent.parent.currentModel ? parent.parent.parent.currentModel.filterToolMessages : false
                                                onToggled: {
                                                    if (modelListView.currentIndex >= 0) {
                                                        var l = agent.models;
                                                        l[modelListView.currentIndex].filterToolMessages = checked;
                                                        agent.models = l;
                                                    }
                                                }
                                            }
                                            CheckBox {
                                                text: "Filter Thoughts"
                                                checked: parent.parent.parent.currentModel ? parent.parent.parent.currentModel.filterThoughts : false
                                                onToggled: {
                                                    if (modelListView.currentIndex >= 0) {
                                                        var l = agent.models;
                                                        l[modelListView.currentIndex].filterThoughts = checked;
                                                        agent.models = l;
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
                                                var list = agent.models;
                                                list.splice(modelListView.currentIndex, 1);
                                                agent.models = list;
                                                modelListView.currentIndex = Math.min(modelListView.currentIndex, list.length - 1);
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
