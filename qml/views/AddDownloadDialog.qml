import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../theme"
import "../components"

/**
 * AddDownloadDialog - Dialog for adding new downloads
 */
Popup {
    id: root
    
    width: Math.min(500, parent.width - Theme.spacing2xl * 2)
    height: contentColumn.implicitHeight + Theme.spacing2xl * 2
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    
    // Expose URL text for external access
    property alias urlText: urlInput.text
    
    // Signals
    signal accepted()
    
    // Dim background
    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.7)
        
        Behavior on opacity {
            NumberAnimation { duration: Theme.durationNormal }
        }
    }
    
    // Dialog background
    background: GlassPanel {
        glassOpacity: 0.2
        borderOpacity: 0.3
        radius: Theme.radiusXl
    }
    
    // Enter animation
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationNormal }
            NumberAnimation { property: "scale"; from: 0.95; to: 1; duration: Theme.durationNormal; easing.type: Theme.easingType }
        }
    }
    
    // Exit animation
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; to: 0; duration: Theme.durationFast }
            NumberAnimation { property: "scale"; to: 0.95; duration: Theme.durationFast }
        }
    }
    
    contentItem: ColumnLayout {
        id: contentColumn
        spacing: Theme.spacingLg
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            
            Text {
                text: qsTr("Add Download")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSize2xl
                font.weight: Font.Bold
            }
            
            Item { Layout.fillWidth: true }
            
            ActionButton {
                iconSource: "close"
                iconColor: Theme.textTertiary
                hoverColor: Theme.error
                onClicked: root.close()
            }
        }
        
        // URL input
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            
            Text {
                text: qsTr("Download URL")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
                font.weight: Font.Medium
            }
            
            GlassPanel {
                Layout.fillWidth: true
                height: 48
                radius: Theme.radiusMd
                glassOpacity: 0.1
                
                TextInput {
                    id: urlInput
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMd
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeMd
                    font.family: Theme.fontFamilyMono
                    verticalAlignment: Text.AlignVCenter
                    selectByMouse: true
                    clip: true
                    
                    // Placeholder
                    Text {
                        anchors.fill: parent
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("https://example.com/file.zip")
                        color: Theme.textTertiary
                        font: parent.font
                        verticalAlignment: Text.AlignVCenter
                        visible: !parent.text && !parent.activeFocus
                    }
                    
                    // Paste from clipboard on focus
                    onActiveFocusChanged: {
                        if (activeFocus && !text) {
                            // Could implement auto-paste from clipboard here
                        }
                    }
                    
                    Keys.onReturnPressed: {
                        if (text.trim()) {
                            root.accepted()
                            root.close()
                        }
                    }
                }
            }
            
            // URL validation hint
            Text {
                visible: urlInput.text && !isValidUrl(urlInput.text)
                text: qsTr("Please enter a valid URL")
                color: Theme.error
                font.pixelSize: Theme.fontSizeXs
            }
        }
        
        // Destination (optional, for future)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            visible: false  // Hidden for now, uses default directory
            
            Text {
                text: qsTr("Save to")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
                font.weight: Font.Medium
            }
            
            GlassPanel {
                Layout.fillWidth: true
                height: 40
                radius: Theme.radiusMd
                glassOpacity: 0.1
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSm
                    
                    Text {
                        Layout.fillWidth: true
                        text: downloadManager ? downloadManager.defaultDownloadDirectory : "~/Downloads"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSm
                        elide: Text.ElideMiddle
                    }
                    
                    ActionButton {
                        iconSource: "folder"
                        iconColor: Theme.textTertiary
                        tooltip: qsTr("Browse")
                    }
                }
            }
        }
        
        // Supported sites info
        GlassPanel {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingSm
            height: supportedSitesColumn.implicitHeight + Theme.spacingMd * 2
            radius: Theme.radiusMd
            glassOpacity: 0.05
            
            ColumnLayout {
                id: supportedSitesColumn
                anchors.fill: parent
                anchors.margins: Theme.spacingMd
                spacing: Theme.spacingXs
                
                Text {
                    text: qsTr("💡 Supported:")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: Font.Medium
                }
                
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Direct links, YouTube*, and 1000+ streaming sites via yt-dlp")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                    wrapMode: Text.WordWrap
                }
                
                Text {
                    text: qsTr("* Requires yt-dlp to be installed")
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                    font.italic: true
                }
            }
        }
        
        // Buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.spacingSm
            spacing: Theme.spacingMd
            
            Item { Layout.fillWidth: true }
            
            // Cancel button
            Button {
                text: qsTr("Cancel")
                
                contentItem: Text {
                    text: parent.text
                    color: parent.hovered ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: Theme.fontSizeMd
                    horizontalAlignment: Text.AlignHCenter
                    
                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                }
                
                background: Rectangle {
                    implicitWidth: 100
                    implicitHeight: 40
                    radius: Theme.radiusMd
                    color: parent.hovered ? Theme.glassBgHover : "transparent"
                    border.width: 1
                    border.color: Theme.glassBorder
                    
                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                }
                
                onClicked: root.close()
            }
            
            // Add button
            Button {
                enabled: urlInput.text.trim() && isValidUrl(urlInput.text)
                text: qsTr("Add Download")
                
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? Theme.textPrimary : Theme.textTertiary
                    font.pixelSize: Theme.fontSizeMd
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                }
                
                background: Rectangle {
                    implicitWidth: 140
                    implicitHeight: 40
                    radius: Theme.radiusMd
                    
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { 
                            position: 0.0
                            color: parent.parent.enabled 
                                   ? (parent.parent.hovered ? Theme.primaryLight : Theme.primary)
                                   : Theme.surfaceHover
                        }
                        GradientStop { 
                            position: 1.0
                            color: parent.parent.enabled
                                   ? (parent.parent.hovered ? Theme.primary : Theme.primaryDark)
                                   : Theme.surface
                        }
                    }
                    
                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                }
                
                onClicked: {
                    root.accepted()
                    root.close()
                }
            }
        }
    }
    
    // Helper function
    function isValidUrl(url) {
        if (!url) return false
        url = url.trim()
        return url.startsWith("http://") || 
               url.startsWith("https://") || 
               url.startsWith("ftp://") ||
               url.startsWith("magnet:")
    }
    
    // Clear on open
    onOpened: {
        urlInput.text = ""
        urlInput.forceActiveFocus()
    }
}
