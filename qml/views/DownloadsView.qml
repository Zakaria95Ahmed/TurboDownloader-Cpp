import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
import "../components"

/**
 * Main downloads view with list and controls
 */
Item {
    id: root

    // Reference to controller for actions
    property var controller: DownloadController

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingMd

        // Header with stats and actions
        GlassPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 60

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingMd
                spacing: Theme.spacingLg

                // Stats
                Row {
                    spacing: Theme.spacingLg

                    // Active downloads
                    Column {
                        spacing: 2

                        Text {
                            text: controller.activeCount
                            font: Theme.fontHeading
                            color: Theme.primary
                        }

                        Text {
                            text: "Active"
                            font: Theme.fontSmall
                            color: Theme.textTertiary
                        }
                    }

                    // Queued
                    Column {
                        spacing: 2

                        Text {
                            text: controller.queuedCount
                            font: Theme.fontHeading
                            color: Theme.warning
                        }

                        Text {
                            text: "Queued"
                            font: Theme.fontSmall
                            color: Theme.textTertiary
                        }
                    }

                    // Total
                    Column {
                        spacing: 2

                        Text {
                            text: controller.totalCount
                            font: Theme.fontHeading
                            color: Theme.textPrimary
                        }

                        Text {
                            text: "Total"
                            font: Theme.fontSmall
                            color: Theme.textTertiary
                        }
                    }
                }

                // Separator
                Rectangle {
                    width: 1
                    Layout.fillHeight: true
                    Layout.topMargin: Theme.spacingSm
                    Layout.bottomMargin: Theme.spacingSm
                    color: Theme.glassBorder
                }

                // Total speed
                Column {
                    spacing: 2

                    Text {
                        text: controller.formattedTotalSpeed
                        font: Theme.fontHeading
                        color: Theme.success
                    }

                    Text {
                        text: "Total Speed"
                        font: Theme.fontSmall
                        color: Theme.textTertiary
                    }
                }

                Item { Layout.fillWidth: true }

                // Action buttons
                Row {
                    spacing: Theme.spacingSm

                    Button {
                        text: "\u25b6 Start All"
                        enabled: controller.queuedCount > 0

                        background: Rectangle {
                            color: parent.enabled ? (parent.hovered ? Theme.primaryHover : Theme.primary)
                                                  : Theme.backgroundTertiary
                            radius: Theme.radiusSm
                        }

                        contentItem: Text {
                            text: parent.text
                            font: Theme.fontCaption
                            color: parent.enabled ? Theme.textPrimary : Theme.textDisabled
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: controller.startAll()
                    }

                    Button {
                        text: "\u23f8 Pause All"
                        enabled: controller.activeCount > 0

                        background: Rectangle {
                            color: parent.hovered ? Theme.glassHighlight : Theme.glassBackground
                            radius: Theme.radiusSm
                            border.color: Theme.glassBorder
                            border.width: 1
                        }

                        contentItem: Text {
                            text: parent.text
                            font: Theme.fontCaption
                            color: parent.enabled ? Theme.textPrimary : Theme.textDisabled
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: controller.pauseAll()
                    }

                    Button {
                        text: "\u{1F5D1} Clear Done"
                        visible: controller.totalCount > controller.activeCount + controller.queuedCount

                        background: Rectangle {
                            color: parent.hovered ? Theme.glassHighlight : "transparent"
                            radius: Theme.radiusSm
                        }

                        contentItem: Text {
                            text: parent.text
                            font: Theme.fontCaption
                            color: Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: controller.clearCompleted()
                    }
                }
            }
        }

        // Downloads list
        GlassPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: downloadsList

                anchors.fill: parent
                anchors.margins: Theme.spacingSm
                clip: true
                spacing: Theme.spacingSm

                model: controller.model

                delegate: DownloadItemDelegate {
                    onPauseClicked: (id) => controller.pauseDownload(id)
                    onResumeClicked: (id) => controller.resumeDownload(id)
                    onCancelClicked: (id) => controller.cancelDownload(id, false)
                    onRetryClicked: (id) => controller.retryDownload(id)
                    onRemoveClicked: (id) => controller.removeDownload(id, false)
                    onOpenFileClicked: (id) => controller.openFile(id)
                    onOpenFolderClicked: (id) => controller.openFolder(id)
                }

                // Empty state
                Text {
                    anchors.centerIn: parent
                    visible: downloadsList.count === 0
                    text: "No downloads yet\n\nClick + to add a download"
                    font: Theme.fontBody
                    color: Theme.textTertiary
                    horizontalAlignment: Text.AlignHCenter
                }

                // Scroll indicator
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }
        }
    }
}
