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
                    model: nped.fileTypes

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
                        var list = nped.fileTypes
                        list.push({
                            extensions: "*.new",
                            languageId: "text",
                            languageServer: "none",
                            tabSize: 4,
                            parse: false
                        })
                        nped.fileTypes = list
                        fileTypeListView.currentIndex = list.length - 1
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

            ColumnLayout {
                anchors.fill: parent
//                Layout.fillWidth: true
//                Layout.fillHeight: true
                spacing: 15
                anchors.margins: 10

                property var currentFileType: fileTypeListView.currentIndex >= 0 && fileTypeListView.currentIndex < nped.fileTypes.length ? nped.fileTypes[fileTypeListView.currentIndex] : null

                visible: currentFileType !== null

                Label {
                    text: "Edit File Type: " + (parent.currentFileType ? parent.currentFileType.extensions : "")
                    font.bold: true
                    font.pointSize: 1
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
                                var l = nped.fileTypes;
                                l[fileTypeListView.currentIndex].extensions = text;
                                nped.fileTypes = l;
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
                                var l = nped.fileTypes;
                                l[fileTypeListView.currentIndex].languageId = text;
                                nped.fileTypes = l;
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
                                var l = nped.fileTypes;
                                l[fileTypeListView.currentIndex].languageServer = text;
                                nped.fileTypes = l;
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
                                var l = nped.fileTypes;
                                l[fileTypeListView.currentIndex].tabSize = value;
                                nped.fileTypes = l;
                            }
                        }
                    }

                    // Parse (Enable LSP)
                    Label { text: "Enable LSP:"; Layout.alignment: Qt.AlignRight }
                    Switch {
                        checked: parent.parent.currentFileType ? parent.parent.currentFileType.parse : false
                        onToggled: {
                            if (fileTypeListView.currentIndex >= 0) {
                                var l = nped.fileTypes;
                                l[fileTypeListView.currentIndex].parse = checked;
                                nped.fileTypes = l;
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
                            var list = nped.fileTypes;
                            list.splice(fileTypeListView.currentIndex, 1);
                            nped.fileTypes = list;
                            fileTypeListView.currentIndex = Math.min(fileTypeListView.currentIndex, list.length - 1);
                        }
                    }
                }
            }
        }
    }
}
