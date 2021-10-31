/*
    SPDX-FileCopyrightText: 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12
import org.kde.kirigami 2.12 as Kirigami
import org.kde.kwin 3.0 as KWinComponents
import org.kde.plasma.components 3.0 as PC3
import org.kde.plasma.core 2.0 as PlasmaCore

Item {
    id: bar

    readonly property real desktopHeight: PlasmaCore.Units.gridUnit * 4
    readonly property real desktopWidth: desktopHeight * targetScreen.geometry.width / targetScreen.geometry.height
    readonly property real columnHeight: desktopHeight + PlasmaCore.Units.gridUnit

    property QtObject clientModel
    property alias desktopModel: desktopRepeater.model
    property QtObject selectedDesktop: null

    implicitHeight: columnHeight + 2 * PlasmaCore.Units.smallSpacing

    Flickable {
        anchors.fill: parent
        leftMargin: Math.max((width - contentWidth) / 2, 0)
        topMargin: Math.max((height - contentHeight) / 2, 0)
        contentWidth: contentItem.childrenRect.width
        contentHeight: contentItem.childrenRect.height
        clip: true
        flickableDirection: Flickable.HorizontalFlick

        Row {
            spacing: PlasmaCore.Units.largeSpacing

            Repeater {
                id: desktopRepeater

                Column {
                    id: delegate
                    width: bar.desktopWidth
                    height: bar.columnHeight
                    spacing: PlasmaCore.Units.smallSpacing

                    required property QtObject desktop
                    required property int index

                    function remove() {
                        bar.desktopModel.remove(delegate.index);
                    }

                    Item {
                        width: bar.desktopWidth
                        height: bar.desktopHeight

                        Rectangle {
                            id: mask
                            anchors.fill: parent
                            radius: 3
                            visible: false
                        }

                        DesktopView {
                            id: thumbnail

                            property bool scaled: state == "scaled"

                            width: targetScreen.geometry.width
                            height: targetScreen.geometry.height
                            visible: scaled
                            clientModel: bar.clientModel
                            desktop: delegate.desktop
                            scale: bar.desktopHeight / targetScreen.geometry.height
                            transformOrigin: Item.TopLeft

                            // Disable the item layer while being scaled.
                            layer.enabled: !scaled
                            layer.textureSize: Qt.size(bar.desktopWidth, bar.desktopHeight)

                            states: State {
                                name: "scaled"
                                ParentChange {
                                    target: thumbnail
                                    parent: container
                                    x: 0
                                    y: 0
                                    scale: 1
                                }
                            }

                            transitions: Transition {
                                SequentialAnimation {
                                    ParentAnimation {
                                        NumberAnimation {
                                            properties: "x,y,scale"
                                            duration: effect.animationDuration
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                    ScriptAction {
                                        script: {
                                            KWinComponents.Workspace.currentVirtualDesktop = delegate.desktop;
                                            effect.quickDeactivate();
                                        }
                                    }
                                }
                            }
                        }

                        OpacityMask {
                            anchors.fill: parent
                            cached: true
                            source: thumbnail
                            maskSource: mask
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 3
                            color: "transparent"
                            border.width: 2
                            border.color: PlasmaCore.ColorScope.highlightColor
                            opacity: dropArea.containsDrag ? 0.5 : 1.0
                            visible: !thumbnail.scaled && (dropArea.containsDrag || bar.selectedDesktop == delegate.desktop)
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                            onClicked: {
                                mouse.accepted = true;
                                switch (mouse.button) {
                                case Qt.LeftButton:
                                    thumbnail.state = "scaled";
                                    break;
                                case Qt.MiddleButton:
                                    delegate.remove();
                                    break;
                                }
                            }
                        }

                        Loader {
                            LayoutMirroring.enabled: Qt.application.layoutDirection == Qt.RightToLeft
                            active: hoverHandler.hovered && desktopRepeater.count > 1
                            anchors.right: parent.right
                            anchors.top: parent.top
                            sourceComponent: PC3.Button {
                                icon.name: "delete"
                                onClicked: delegate.remove()
                                PC3.ToolTip {
                                    text: i18n("Delete virtual desktop")
                                }

                            }
                        }

                        DropArea {
                            id: dropArea
                            anchors.fill: parent

                            onEntered: {
                                drag.accepted = true;
                            }
                            onDropped: {
                                drag.source.desktop = delegate.desktop.x11DesktopNumber;
                            }
                        }
                    }

                    Item {
                        id: label
                        width: bar.desktopWidth
                        height: PlasmaCore.Units.gridUnit
                        state: "normal"

                        PC3.Label {
                            PlasmaCore.ColorScope.colorGroup: PlasmaCore.Theme.ComplementaryColorGroup
                            anchors.fill: parent
                            elide: Text.ElideRight
                            text: delegate.desktop.name
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            visible: label.state == "normal"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onPressed: {
                                mouse.accepted = true;
                                label.startEditing();
                            }
                        }

                        TextInput {
                            id: editor
                            anchors.fill: parent
                            visible: label.state == "editing"
                            color: PlasmaCore.ColorScope.textColor
                            text: delegate.desktop.name
                            onEditingFinished: {
                                delegate.desktop.name = text;
                                label.stopEditing();
                            }
                            Keys.onEscapePressed: label.stopEditing();
                        }

                        states: [
                            State {
                                name: "normal"
                            },
                            State {
                                name: "editing"
                            }
                        ]

                        function startEditing() {
                            state = "editing";
                            editor.forceActiveFocus();
                        }
                        function stopEditing() {
                            state = "normal";
                        }
                    }

                    HoverHandler {
                        id: hoverHandler
                    }
                }
            }

            PC3.Button {
                width: bar.desktopWidth
                height: bar.desktopHeight
                icon.name: "list-add"
                opacity: hovered ? 1 : 0.75
                onClicked: desktopModel.create(desktopModel.rowCount())

                ToolTip.text: i18n("Add Desktop")
                ToolTip.visible: hovered
                ToolTip.delay: Kirigami.Units.toolTipDelay

                DropArea {
                    anchors.fill: parent
                    onEntered: {
                        drag.accepted = desktopModel.rowCount() < 20
                    }
                    onDropped: {
                        desktopModel.create(desktopModel.rowCount());
                        drag.source.desktop = desktopModel.rowCount() + 1;
                    }
                }
            }
        }
    }
}
