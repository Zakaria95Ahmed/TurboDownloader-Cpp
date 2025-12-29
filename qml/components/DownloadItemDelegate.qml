import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../theme"

/**
 * DownloadItemDelegate - Displays a single download item in the list
 * 
 * Shows file name, progress, speed, ETA, and action buttons.
 * Features glassmorphism styling and smooth animations.
 */
GlassPanel {
    id: root
    
    // Required properties from model
    required property int index
    required property string downloadId
    required property string fileName
    required property string url
    required property int state  // DownloadState enum
    required property double progress  // 0-100
    required property double speed  // bytes/sec
    required property string totalSize
    required property string downloadedSize
    required property string remainingTime
    required property string errorMessage
    required property int activeSegments
    required property int totalSegments
    
    // Properties
    property bool expanded: false
    
    // Signals
    signal pauseClicked()
    signal resumeClicked()
    signal cancelClicked()
    signal retryClicked()
    signal openFolderClicked()
    signal deleteClicked()
    
    // Dimensions
    implicitWidth: ListView.view ? ListView.view.width : 400
    implicitHeight: expanded ? Theme.downloadItemHeightExpanded : Theme.downloadItemHeight
    hoverable: true
    radius: Theme.radiusMd
    
    Behavior on implicitHeight {
        NumberAnimation { 
            duration: Theme.durationNormal
            easing.type: Theme.easingType
        }
    }
    
    // Content
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingSm
        
        // Main row: icon, info, actions
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd
            
            // State indicator
            Rectangle {
                width: 4
                height: 40
                radius: 2
                color: Theme.stateColor(root.state)
                
                // Pulse animation for active downloads
                SequentialAnimation {
                    running: root.state === 2  // Downloading
                    loops: Animation.Infinite
                    
                    NumberAnimation {
                        target: parent
                        property: "opacity"
                        to: 0.5
                        duration: 800
                    }
                    NumberAnimation {
                        target: parent
                        property: "opacity"
                        to: 1.0
                        duration: 800
                    }
                }
            }
            
            // File info
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXs
                
                // File name
                Text {
                    Layout.fillWidth: true
                    text: root.fileName
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeMd
                    font.weight: Font.Medium
                    elide: Text.ElideMiddle
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.expanded = !root.expanded
                    }
                }
                
                // Status row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacingMd
                    
                    // Size info
                    Text {
                        text: root.downloadedSize + " / " + root.totalSize
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSm
                        font.family: Theme.fontFamilyMono
                    }
                    
                    // State badge
                    Rectangle {
                        visible: root.state !== 2  // Not downloading
                        height: 18
                        width: stateText.width + Theme.spacingMd
                        radius: 9
                        color: Qt.rgba(Theme.stateColor(root.state).r,
                                      Theme.stateColor(root.state).g,
                                      Theme.stateColor(root.state).b, 0.2)
                        
                        Text {
                            id: stateText
                            anchors.centerIn: parent
                            text: Theme.stateText(root.state)
                            color: Theme.stateColor(root.state)
                            font.pixelSize: Theme.fontSizeXs
                            font.weight: Font.Medium
                        }
                    }
                    
                    // Speed (when downloading)
                    SpeedIndicator {
                        visible: root.state === 2
                        speed: root.speed
                        active: true
                    }
                    
                    // Segments info (when downloading)
                    Text {
                        visible: root.state === 2 && root.totalSegments > 1
                        text: "(" + root.activeSegments + "/" + root.totalSegments + " segments)"
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeXs
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    // ETA (when downloading)
                    Text {
                        visible: root.state === 2 && root.remainingTime !== ""
                        text: root.remainingTime + " left"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSm
                    }
                }
            }
            
            // Action buttons
            RowLayout {
                spacing: Theme.spacingXs
                
                // Pause/Resume button
                ActionButton {
                    visible: root.state === 2 || root.state === 3  // Downloading or Paused
                    iconSource: root.state === 2 ? "pause" : "play"
                    tooltip: root.state === 2 ? qsTr("Pause") : qsTr("Resume")
                    onClicked: root.state === 2 ? root.pauseClicked() : root.resumeClicked()
                }
                
                // Retry button (for failed)
                ActionButton {
                    visible: root.state === 7  // Failed
                    iconSource: "retry"
                    iconColor: Theme.warning
                    hoverColor: Theme.warningLight
                    tooltip: qsTr("Retry")
                    onClicked: root.retryClicked()
                }
                
                // Open folder button (for completed)
                ActionButton {
                    visible: root.state === 6  // Completed
                    iconSource: "folder"
                    iconColor: Theme.success
                    hoverColor: Theme.successLight
                    tooltip: qsTr("Open Folder")
                    onClicked: root.openFolderClicked()
                }
                
                // Cancel/Delete button
                ActionButton {
                    iconSource: root.state === 6 || root.state === 7 ? "trash" : "close"
                    iconColor: Theme.textTertiary
                    hoverColor: Theme.error
                    tooltip: root.state === 6 || root.state === 7 ? qsTr("Remove") : qsTr("Cancel")
                    onClicked: {
                        if (root.state === 6 || root.state === 7) {
                            root.deleteClicked()
                        } else {
                            root.cancelClicked()
                        }
                    }
                }
            }
        }
        
        // Progress bar
        ProgressBar {
            Layout.fillWidth: true
            value: root.progress
            fillColor: Theme.stateColor(root.state)
            showStripes: root.state === 2
            visible: root.state !== 6 && root.state !== 7  // Not completed or failed
        }
        
        // Expanded details
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs
            visible: root.expanded
            opacity: root.expanded ? 1 : 0
            
            Behavior on opacity {
                NumberAnimation { duration: Theme.durationNormal }
            }
            
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.glassBorder
            }
            
            // URL
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                
                Text {
                    text: qsTr("URL:")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeSm
                }
                
                Text {
                    Layout.fillWidth: true
                    text: root.url
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSm
                    font.family: Theme.fontFamilyMono
                    elide: Text.ElideMiddle
                }
            }
            
            // Error message (if failed)
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm
                visible: root.state === 7 && root.errorMessage !== ""
                
                Text {
                    text: qsTr("Error:")
                    color: Theme.error
                    font.pixelSize: Theme.fontSizeSm
                }
                
                Text {
                    Layout.fillWidth: true
                    text: root.errorMessage
                    color: Theme.errorLight
                    font.pixelSize: Theme.fontSizeSm
                    wrapMode: Text.WordWrap
                }
            }
            
            // Progress percentage
            Text {
                text: qsTr("Progress: %1%").arg(root.progress.toFixed(1))
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
            }
        }
    }
}
