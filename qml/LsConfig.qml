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
        text: "Language Servers (LSP)"
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
                    model: nped.languageServersConfig

                    Component.onCompleted: {
                        if (count > 0)
                            currentIndex = 0;
                        }

                    delegate: ItemDelegate {
                        required property var modelData
                        required property var index
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
                        var list = nped.languageServersConfig;
                        list.push({
                            name: "New Server",
                            command: "/usr/bin/...",
                            args: ""
                            });
                        nped.languageServersConfig = list;
                        serverListView.currentIndex = list.length - 1;
                        }
                    }
                }
            }

        // Right Side: Details
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            id: server
            property var currentServer: serverListView.currentIndex >= 0 && serverListView.currentIndex < nped.languageServersConfig.length ? nped.languageServersConfig[serverListView.currentIndex] : null

            ColumnLayout {
                anchors.fill: parent
                //                Layout.fillWidth: true
                //                 Layout.fillHeight: true
                spacing: 15
                anchors.margins: 10

                visible: server.currentServer !== null

                Label {
                    text: "Edit Server: " + (server.currentServer ? server.currentServer.name : "")
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
                        text: server.currentServer ? server.currentServer.name : ""
                        Layout.fillWidth: true
                        placeholderText: "e.g. clangd"
                        onEditingFinished: {
                            if (serverListView.currentIndex >= 0) {
                                var l = nped.languageServersConfig;
                                l[serverListView.currentIndex].name = text;
                                nped.languageServersConfig = l;
                                }
                            }
                        }

                    // Command
                    Label {
                        text: "Command:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: server.currentServer ? server.currentServer.command : ""
                        Layout.fillWidth: true
                        placeholderText: "/path/to/executable"
                        onEditingFinished: {
                            if (serverListView.currentIndex >= 0) {
                                var l = nped.languageServersConfig;
                                l[serverListView.currentIndex].command = text;
                                nped.languageServersConfig = l;
                                }
                            }
                        }

                    // Arguments
                    Label {
                        text: "Arguments:"
                        Layout.alignment: Qt.AlignRight
                        }
                    TextField {
                        text: server.currentServer ? server.currentServer.args : ""
                        Layout.fillWidth: true
                        placeholderText: "--stdio --log=verbose"
                        onEditingFinished: {
                            if (serverListView.currentIndex >= 0) {
                                var l = nped.languageServersConfig;
                                l[serverListView.currentIndex].args = text;
                                nped.languageServersConfig = l;
                                }
                            }
                        }
                    }

                Item {
                    height: 10
                    }

                Button {
                    Layout.alignment: Qt.AlignRight
                    text: "Delete Server"
                    flat: true
                    Material.foreground: Material.Red
                    onClicked: {
                        if (serverListView.currentIndex >= 0) {
                            var list = nped.languageServersConfig;
                            list.splice(serverListView.currentIndex, 1);
                            nped.languageServersConfig = list;
                            serverListView.currentIndex = Math.min(serverListView.currentIndex, list.length - 1);
                            }
                        }
                    }
                }
            }
        }
    }
