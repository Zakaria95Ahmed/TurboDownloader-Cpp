import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "theme"
import "views"
import "components"

/**
 * Main.qml - OpenIDM Main Application Window
 * 
 * Features:
 * - Modern glassmorphism dark theme
 * - Header with global stats
 * - Download list view
 * - Add download dialog
 * - Settings access
 */
ApplicationWindow {
    id: window
    
    width: 900
    height: 700
    minimumWidth: 600
    minimumHeight: 400
    visible: true
    title: qsTr("OpenIDM - Download Manager")
    color: Theme.background
    
    // Background gradient
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.background }
            GradientStop { position: 0.5; color: Theme.backgroundAlt }
            GradientStop { position: 1.0; color: Theme.background }
        }
    }
    
    // Decorative elements
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        width: 400
        height: 400
        radius: 200
        color: Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.05)
        
        transform: Translate { x: 150; y: -150 }
    }
    
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 300
        height: 300
        radius: 150
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.03)
        
        transform: Translate { x: -100; y: 100 }
    }
    
    // Main content
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // ═══════════════════════════════════════════════════════════════════
        // Header
        // ═══════════════════════════════════════════════════════════════════
        Rectangle {
            Layout.fillWidth: true
            height: Theme.headerHeight
            color: Qt.rgba(0, 0, 0, 0.3)
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLg
                anchors.rightMargin: Theme.spacingLg
                spacing: Theme.spacingLg
                
                // Logo/Title
                RowLayout {
                    spacing: Theme.spacingMd
                    
                    // Logo icon
                    Rectangle {
                        width: 32
                        height: 32
                        radius: Theme.radiusMd
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: Theme.primary }
                            GradientStop { position: 1.0; color: Theme.primaryDark }
                        }
                        
                        Text {
                            anchors.centerIn: parent
                            text: "↓"
                            color: "white"
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }
                    }
                    
                    Text {
                        text: "OpenIDM"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSize2xl
                        font.weight: Font.Bold
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // Global stats
                RowLayout {
                    spacing: Theme.spacing2xl
                    
                    // Active downloads
                    ColumnLayout {
                        spacing: 2
                        
                        Text {
                            text: downloadManager ? downloadManager.activeDownloads : "0"
                            color: Theme.accent
                            font.pixelSize: Theme.fontSizeLg
                            font.weight: Font.Bold
                            font.family: Theme.fontFamilyMono
                        }
                        
                        Text {
                            text: qsTr("Active")
                            color: Theme.textTertiary
                            font.pixelSize: Theme.fontSizeXs
                        }
                    }
                    
                    // Speed
                    ColumnLayout {
                        spacing: 2
                        
                        Text {
                            text: downloadManager ? downloadManager.globalSpeedFormatted : "0 B/s"
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeLg
                            font.weight: Font.Bold
                            font.family: Theme.fontFamilyMono
                        }
                        
                        Text {
                            text: qsTr("Speed")
                            color: Theme.textTertiary
                            font.pixelSize: Theme.fontSizeXs
                        }
                    }
                    
                    // Total downloads
                    ColumnLayout {
                        spacing: 2
                        
                        Text {
                            text: downloadManager ? downloadManager.totalDownloads : "0"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeLg
                            font.weight: Font.Bold
                            font.family: Theme.fontFamilyMono
                        }
                        
                        Text {
                            text: qsTr("Total")
                            color: Theme.textTertiary
                            font.pixelSize: Theme.fontSizeXs
                        }
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // Header actions
                RowLayout {
                    spacing: Theme.spacingSm
                    
                    // Add download button
                    Button {
                        text: qsTr("+ Add Download")
                        
                        contentItem: RowLayout {
                            spacing: Theme.spacingSm
                            
                            Text {
                                text: "+"
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeLg
                                font.weight: Font.Bold
                            }
                            
                            Text {
                                text: qsTr("Add Download")
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeMd
                                font.weight: Font.Medium
                            }
                        }
                        
                        background: Rectangle {
                            radius: Theme.radiusMd
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: Theme.primary }
                                GradientStop { position: 1.0; color: Theme.primaryDark }
                            }
                            opacity: parent.hovered ? 0.9 : 1.0
                            
                            Behavior on opacity {
                                NumberAnimation { duration: Theme.durationFast }
                            }
                        }
                        
                        onClicked: addDownloadDialog.open()
                    }
                    
                    // Settings button
                    ActionButton {
                        iconSource: "settings"
                        tooltip: qsTr("Settings")
                        iconColor: Theme.textSecondary
                        onClicked: settingsPopup.open()
                    }
                }
            }
        }
        
        // ═══════════════════════════════════════════════════════════════════
        // Toolbar
        // ═══════════════════════════════════════════════════════════════════
        Rectangle {
            Layout.fillWidth: true
            height: Theme.toolbarHeight
            color: Qt.rgba(0, 0, 0, 0.15)
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingLg
                anchors.rightMargin: Theme.spacingLg
                spacing: Theme.spacingMd
                
                // Bulk actions
                RowLayout {
                    spacing: Theme.spacingSm
                    
                    Button {
                        flat: true
                        text: qsTr("Start All")
                        font.pixelSize: Theme.fontSizeSm
                        
                        contentItem: Text {
                            text: parent.text
                            color: parent.hovered ? Theme.primary : Theme.textSecondary
                            font: parent.font
                            
                            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                        }
                        
                        background: Item {}
                        
                        onClicked: downloadManager?.startAll()
                    }
                    
                    Rectangle { width: 1; height: 16; color: Theme.glassBorder }
                    
                    Button {
                        flat: true
                        text: qsTr("Pause All")
                        font.pixelSize: Theme.fontSizeSm
                        
                        contentItem: Text {
                            text: parent.text
                            color: parent.hovered ? Theme.warning : Theme.textSecondary
                            font: parent.font
                            
                            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                        }
                        
                        background: Item {}
                        
                        onClicked: downloadManager?.pauseAll()
                    }
                    
                    Rectangle { width: 1; height: 16; color: Theme.glassBorder }
                    
                    Button {
                        flat: true
                        text: qsTr("Clear Completed")
                        font.pixelSize: Theme.fontSizeSm
                        
                        contentItem: Text {
                            text: parent.text
                            color: parent.hovered ? Theme.success : Theme.textSecondary
                            font: parent.font
                            
                            Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                        }
                        
                        background: Item {}
                        
                        onClicked: downloadManager?.clearCompleted()
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // Search/Filter (placeholder)
                GlassPanel {
                    width: 200
                    height: 32
                    radius: Theme.radiusMd
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSm
                        spacing: Theme.spacingSm
                        
                        Text {
                            text: "🔍"
                            color: Theme.textTertiary
                            font.pixelSize: Theme.fontSizeSm
                        }
                        
                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeSm
                            clip: true
                            
                            Text {
                                anchors.fill: parent
                                text: qsTr("Search downloads...")
                                color: Theme.textTertiary
                                font.pixelSize: Theme.fontSizeSm
                                visible: !parent.text && !parent.activeFocus
                            }
                        }
                    }
                }
            }
        }
        
        // ═══════════════════════════════════════════════════════════════════
        // Download List
        // ═══════════════════════════════════════════════════════════════════
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            DownloadListView {
                id: downloadListView
                anchors.fill: parent
                anchors.margins: Theme.spacingLg
            }
        }
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // Add Download Dialog
    // ═══════════════════════════════════════════════════════════════════════
    AddDownloadDialog {
        id: addDownloadDialog
        anchors.centerIn: parent
        
        onAccepted: {
            if (downloadManager && urlText !== "") {
                downloadManager.addDownloadUrl(urlText, "", true)
            }
        }
    }
    
    // ═══════════════════════════════════════════════════════════════════════
    // Settings Popup (placeholder)
    // ═══════════════════════════════════════════════════════════════════════
    Popup {
        id: settingsPopup
        width: 400
        height: 300
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        anchors.centerIn: parent
        
        background: GlassPanel {
            glassOpacity: 0.15
            borderOpacity: 0.2
        }
        
        contentItem: ColumnLayout {
            spacing: Theme.spacingLg
            
            Text {
                text: qsTr("Settings")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSize2xl
                font.weight: Font.Bold
            }
            
            Text {
                text: qsTr("Settings panel coming soon...")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeMd
            }
            
            Item { Layout.fillHeight: true }
            
            Button {
                text: qsTr("Close")
                Layout.alignment: Qt.AlignRight
                onClicked: settingsPopup.close()
            }
        }
    }
}
