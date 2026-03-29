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
import Nped.Config

ColumnLayout {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true

    required property string type
    property var textStyles: type == "Dark" ? nped.textStylesDark : nped.textStylesLight

    Label {
        text: root.type + " Text Styles"
        font.pointSize: 18
        font.bold: true
        color: Material.foreground
        }
    Rectangle {
        // height: 1;
        Layout.fillWidth: true
        color: Material.accent
        }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 15

        // Left Side: TextStyles List
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
                    id: listView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.textStyles

                    Component.onCompleted: {
                        if (count > 0)
                            currentIndex = 0;
                        }

onCurrentIndexChanged: console.log("current index: "+currentIndex);
onModelChanged: console.log("Model changed");

                    delegate: ItemDelegate {
                        id: delegate
                        required property var modelData
                        required property var index
                        width: ListView.view.width
                        height: 40
                        text: modelData.name
                        highlighted: ListView.isCurrentItem
                        onClicked: ListView.view.currentIndex = index
                        font.weight: ListView.isCurrentItem ? Font.DemiBold : Font.Normal
                        }
                    }
                }
            }

        // Right Side: Details
        ScrollView {
            id: textItem
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            property var textStyle: listView.currentItem ? listView.currentItem.modelData : null

            ColumnLayout {
                anchors.fill: parent
                spacing: 15
                anchors.margins: 10

                visible: textItem.textStyle !== null

                ColorConfigItem {
                    name: "Text Color:"
                    color: textItem.textStyle ? textItem.textStyle.fg : "#000000"
                    onColorChangedByUser: newColor => {
                        textItem.textStyle.fg = newColor;
                        nped.setTextStyle(textItem.textStyle, root.type == "Dark", listView.currentIndex);
                        }
                    }
                ColorConfigItem {
                    name: "Background Color:"
                    color: textItem.textStyle ? textItem.textStyle.bg : "#000000"
                    onColorChangedByUser: newColor => {
                        textItem.textStyle.bg = newColor;
                        nped.setTextStyle(textItem.textStyle, root.type == "Dark", listView.currentIndex);
                        }
                    }
                CheckBox {
                    text: "italic"
                    Binding on checked {
                        value: textItem.textStyle ? textItem.textStyle.italic : false
                        }
                    onToggled: {
                        textItem.textStyle.italic = checked;
                        nped.setTextStyle(textItem.textStyle, root.type == "Dark", listView.currentIndex);
                        }
                    }
                CheckBox {
                    text: "bold"
                    Binding on checked {
                        value: textItem.textStyle ? textItem.textStyle.bold : false
                        }
                    onToggled: {
                        textItem.textStyle.bold = checked;
                        nped.setTextStyle(textItem.textStyle, root.type == "Dark", listView.currentIndex);
                        }
                    }
                }
            }
        }
    }
