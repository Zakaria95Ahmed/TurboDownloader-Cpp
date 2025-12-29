import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../theme"
import "../components"

/**
 * Settings view for configuring download options
 */
Item {
    id: root

    property var controller: DownloadController

    ScrollView {
        anchors.fill: parent
        anchors.margins: Theme.spacingMd

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingLg

            // General Settings
            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: settingsGeneral.height + Theme.spacingLg * 2

                ColumnLayout {
                    id: settingsGeneral
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLg
                    spacing: Theme.spacingMd

                    Text {
                        text: "General"
                        font: Theme.fontHeading
                        color: Theme.textPrimary
                    }

                    // Default save path
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingXs

                        Text {
                            text: "Default Save Location"
                            font: Theme.fontCaption
                            color: Theme.textSecondary
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.spacingSm

                            TextField {
                                id: savePathField
                                Layout.fillWidth: true
                                text: controller.defaultSavePath
                                selectByMouse: true

                                background: Rectangle {
                                    color: Theme.backgroundSecondary
                                    radius: Theme.radiusSm
                                    border.color: savePathField.activeFocus ? Theme.primary : Theme.glassBorder
                                    border.width: 1
                                }

                                color: Theme.textPrimary
                                font: Theme.fontBody

                                onEditingFinished: controller.defaultSavePath = text
                            }

                            Button {
                                text: "Browse"

                                background: Rectangle {
                                    color: parent.hovered ? Theme.glassHighlight : Theme.glassBackground
                                    radius: Theme.radiusSm
                                    border.color: Theme.glassBorder
                                    border.width: 1
                                }

                                contentItem: Text {
                                    text: parent.text
                                    font: Theme.fontCaption
                                    color: Theme.textPrimary
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                onClicked: folderDialog.open()
                            }
                        }
                    }

                    // Max concurrent downloads
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingXs

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: "Maximum Concurrent Downloads"
                                font: Theme.fontCaption
                                color: Theme.textSecondary
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: concurrentSlider.value
                                font: Theme.fontBody
                                color: Theme.primary
                            }
                        }

                        Slider {
                            id: concurrentSlider
                            Layout.fillWidth: true
                            from: 1
                            to: 10
                            stepSize: 1
                            value: controller.maxConcurrent

                            onValueChanged: controller.maxConcurrent = value
                        }
                    }
                }
            }

            // Performance Settings
            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: settingsPerf.height + Theme.spacingLg * 2

                ColumnLayout {
                    id: settingsPerf
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLg
                    spacing: Theme.spacingMd

                    Text {
                        text: "Performance"
                        font: Theme.fontHeading
                        color: Theme.textPrimary
                    }

                    // Max segments per download
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingXs

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: "Maximum Segments per Download"
                                font: Theme.fontCaption
                                color: Theme.textSecondary
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: segmentsSlider.value
                                font: Theme.fontBody
                                color: Theme.primary
                            }
                        }

                        Slider {
                            id: segmentsSlider
                            Layout.fillWidth: true
                            from: 1
                            to: 32
                            stepSize: 1
                            value: 8
                        }

                        Text {
                            text: "More segments can increase speed but use more resources"
                            font: Theme.fontSmall
                            color: Theme.textTertiary
                        }
                    }

                    // Speed limit (future feature)
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingXs

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                text: "Speed Limit"
                                font: Theme.fontCaption
                                color: Theme.textSecondary
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: speedLimitSlider.value === 0 ? "Unlimited" : (speedLimitSlider.value + " MB/s")
                                font: Theme.fontBody
                                color: speedLimitSlider.value === 0 ? Theme.success : Theme.primary
                            }
                        }

                        Slider {
                            id: speedLimitSlider
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            stepSize: 5
                            value: 0
                        }
                    }
                }
            }

            // About Section
            GlassPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: aboutSection.height + Theme.spacingLg * 2

                ColumnLayout {
                    id: aboutSection
                    anchors.fill: parent
                    anchors.margins: Theme.spacingLg
                    spacing: Theme.spacingMd

                    Text {
                        text: "About"
                        font: Theme.fontHeading
                        color: Theme.textPrimary
                    }

                    Column {
                        spacing: Theme.spacingXs

                        Text {
                            text: "OpenIDM"
                            font: Theme.fontBody
                            color: Theme.textPrimary
                        }

                        Text {
                            text: "Version 1.0.0"
                            font: Theme.fontCaption
                            color: Theme.textSecondary
                        }

                        Text {
                            text: "A modern, cross-platform download manager"
                            font: Theme.fontCaption
                            color: Theme.textTertiary
                        }
                    }

                    Text {
                        text: "Licensed under GPL-3.0"
                        font: Theme.fontSmall
                        color: Theme.textTertiary
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Select Default Save Location"
        currentFolder: "file:///" + savePathField.text

        onAccepted: {
            let path = selectedFolder.toString().replace("file:///", "")
            savePathField.text = path
            controller.defaultSavePath = path
        }
    }
}
