import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../theme"
import "../components"

/**
 * DownloadListView - Displays the list of all downloads
 */
Item {
    id: root
    
    // Empty state
    Item {
        anchors.fill: parent
        visible: !listView.count
        
        ColumnLayout {
            anchors.centerIn: parent
            spacing: Theme.spacingLg
            
            // Empty icon
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 80
                height: 80
                radius: 40
                color: Theme.glassBg
                border.width: 2
                border.color: Theme.glassBorder
                
                Text {
                    anchors.centerIn: parent
                    text: "↓"
                    color: Theme.textTertiary
                    font.pixelSize: 36
                }
            }
            
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("No downloads yet")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeXl
                font.weight: Font.Medium
            }
            
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Click 'Add Download' to get started")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeMd
            }
            
            // Quick add button
            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.spacingMd
                
                contentItem: Text {
                    text: qsTr("+ Add Your First Download")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeMd
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                }
                
                background: Rectangle {
                    implicitWidth: 200
                    implicitHeight: 44
                    radius: Theme.radiusMd
                    color: parent.hovered ? Theme.primaryLight : Theme.primary
                    
                    Behavior on color {
                        ColorAnimation { duration: Theme.durationFast }
                    }
                }
                
                onClicked: {
                    // Find the add download dialog in parent hierarchy
                    var p = root.parent
                    while (p) {
                        if (p.hasOwnProperty("addDownloadDialog")) {
                            p.addDownloadDialog.open()
                            break
                        }
                        p = p.parent
                    }
                }
            }
        }
    }
    
    // Download list
    ListView {
        id: listView
        anchors.fill: parent
        spacing: Theme.spacingSm
        clip: true
        
        // Use download list model from C++
        model: downloadListModel
        
        // Smooth scrolling
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            
            contentItem: Rectangle {
                implicitWidth: 6
                radius: 3
                color: Theme.glassBorder
                opacity: parent.active ? 1.0 : 0.5
                
                Behavior on opacity {
                    NumberAnimation { duration: Theme.durationFast }
                }
            }
            
            background: Rectangle {
                color: "transparent"
            }
        }
        
        // Delegate
        delegate: DownloadItemDelegate {
            width: listView.width - Theme.spacingSm
            
            // Map model roles
            downloadId: model.id || ""
            fileName: model.fileName || "Unknown"
            url: model.url || ""
            state: model.state || 0
            progress: model.progress || 0
            speed: model.speed || 0
            totalSize: Theme.formatBytes(model.totalSize || 0)
            downloadedSize: Theme.formatBytes(model.downloadedSize || 0)
            remainingTime: model.remainingTime || ""
            errorMessage: model.errorMessage || ""
            activeSegments: model.activeSegments || 0
            totalSegments: model.totalSegments || 0
            
            // Action handlers
            onPauseClicked: downloadManager?.pauseDownload(downloadId)
            onResumeClicked: downloadManager?.resumeDownload(downloadId)
            onCancelClicked: downloadManager?.cancelDownload(downloadId)
            onRetryClicked: downloadManager?.retryDownload(downloadId)
            onOpenFolderClicked: {
                // Open containing folder
                Qt.openUrlExternally("file://" + model.filePath.substring(0, model.filePath.lastIndexOf("/")))
            }
            onDeleteClicked: downloadManager?.removeDownload(downloadId, false)
        }
        
        // Add/remove animations
        add: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationNormal }
                NumberAnimation { property: "scale"; from: 0.9; to: 1; duration: Theme.durationNormal; easing.type: Theme.easingType }
            }
        }
        
        remove: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; to: 0; duration: Theme.durationFast }
                NumberAnimation { property: "scale"; to: 0.9; duration: Theme.durationFast }
            }
        }
        
        displaced: Transition {
            NumberAnimation { properties: "y"; duration: Theme.durationNormal; easing.type: Theme.easingType }
        }
        
        // Pull to refresh hint (visual only for now)
        header: Item {
            width: listView.width
            height: listView.count > 0 ? Theme.spacingSm : 0
        }
        
        footer: Item {
            width: listView.width
            height: Theme.spacingLg
        }
    }
    
    // Floating action button (mobile-friendly)
    Rectangle {
        id: fab
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingLg
        width: 56
        height: 56
        radius: 28
        visible: Qt.platform.os === "android" || Qt.platform.os === "ios"
        
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.primary }
            GradientStop { position: 1.0; color: Theme.primaryDark }
        }
        
        // Shadow
        layer.enabled: true
        layer.effect: Item {
            Rectangle {
                anchors.fill: parent
                anchors.margins: -4
                radius: parent.radius + 4
                color: Qt.rgba(0, 0, 0, 0.3)
                z: -1
            }
        }
        
        Text {
            anchors.centerIn: parent
            text: "+"
            color: "white"
            font.pixelSize: 28
            font.weight: Font.Light
        }
        
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            
            onClicked: {
                var p = root.parent
                while (p) {
                    if (p.hasOwnProperty("addDownloadDialog")) {
                        p.addDownloadDialog.open()
                        break
                    }
                    p = p.parent
                }
            }
            
            onPressed: {
                fab.scale = 0.95
            }
            
            onReleased: {
                fab.scale = 1.0
            }
        }
        
        Behavior on scale {
            NumberAnimation { duration: Theme.durationFast }
        }
    }
}
