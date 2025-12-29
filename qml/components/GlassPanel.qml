import QtQuick 2.15
import QtQuick.Effects
import "../theme"

/**
 * GlassPanel - Glassmorphism container component
 * 
 * A reusable panel with frosted glass effect for the modern UI.
 */
Item {
    id: root
    
    // Public properties
    property real glassOpacity: 0.05
    property real borderOpacity: 0.1
    property int radius: Theme.radiusLg
    property bool hoverable: false
    property bool hovered: false
    property alias contentItem: contentContainer
    
    default property alias content: contentContainer.data
    
    // Background with glass effect
    Rectangle {
        id: background
        anchors.fill: parent
        radius: root.radius
        color: Qt.rgba(255, 255, 255, root.hovered && root.hoverable ? 0.08 : root.glassOpacity)
        
        border.width: 1
        border.color: Qt.rgba(255, 255, 255, root.hovered && root.hoverable ? 0.2 : root.borderOpacity)
        
        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }
        
        Behavior on border.color {
            ColorAnimation { duration: Theme.durationFast }
        }
    }
    
    // Inner glow effect (subtle)
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: root.radius - 1
        color: "transparent"
        
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(255, 255, 255, 0.03) }
            GradientStop { position: 0.5; color: "transparent" }
        }
    }
    
    // Content container
    Item {
        id: contentContainer
        anchors.fill: parent
    }
    
    // Hover detection
    MouseArea {
        anchors.fill: parent
        hoverEnabled: root.hoverable
        propagateComposedEvents: true
        
        onEntered: root.hovered = true
        onExited: root.hovered = false
        
        onPressed: function(mouse) { mouse.accepted = false }
        onReleased: function(mouse) { mouse.accepted = false }
        onClicked: function(mouse) { mouse.accepted = false }
    }
}
