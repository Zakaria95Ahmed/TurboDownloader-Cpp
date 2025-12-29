import QtQuick 2.15
import "../theme"

/**
 * SpeedIndicator - Displays download speed with animated icon
 */
Item {
    id: root
    
    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    
    // Public properties
    property real speed: 0  // bytes per second
    property bool active: speed > 0
    property color textColor: Theme.textSecondary
    
    Row {
        id: row
        spacing: Theme.spacingXs
        
        // Animated download arrow
        Item {
            width: 16
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            
            // Arrow shape
            Canvas {
                id: arrowCanvas
                anchors.fill: parent
                
                property real animOffset: 0
                property color arrowColor: root.active ? Theme.accent : Theme.textTertiary
                
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    
                    ctx.strokeStyle = arrowColor
                    ctx.lineWidth = 2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    
                    var w = width
                    var h = height
                    var offset = animOffset * 3
                    
                    // Draw arrow pointing down
                    ctx.beginPath()
                    ctx.moveTo(w/2, 2 + offset)
                    ctx.lineTo(w/2, h - 6 + offset)
                    ctx.stroke()
                    
                    // Arrow head
                    ctx.beginPath()
                    ctx.moveTo(w/2 - 4, h - 10 + offset)
                    ctx.lineTo(w/2, h - 6 + offset)
                    ctx.lineTo(w/2 + 4, h - 10 + offset)
                    ctx.stroke()
                    
                    // Base line
                    ctx.beginPath()
                    ctx.moveTo(4, h - 2)
                    ctx.lineTo(w - 4, h - 2)
                    ctx.stroke()
                }
                
                Behavior on arrowColor {
                    ColorAnimation { duration: Theme.durationFast }
                }
                
                // Animation for active state
                SequentialAnimation {
                    running: root.active
                    loops: Animation.Infinite
                    
                    NumberAnimation {
                        target: arrowCanvas
                        property: "animOffset"
                        from: 0
                        to: 1
                        duration: 500
                        easing.type: Easing.InOutQuad
                    }
                    
                    NumberAnimation {
                        target: arrowCanvas
                        property: "animOffset"
                        from: 1
                        to: 0
                        duration: 500
                        easing.type: Easing.InOutQuad
                    }
                }
                
                onAnimOffsetChanged: requestPaint()
                onArrowColorChanged: requestPaint()
            }
        }
        
        // Speed text
        Text {
            id: speedText
            anchors.verticalCenter: parent.verticalCenter
            
            text: Theme.formatSpeed(root.speed)
            color: root.active ? Theme.textPrimary : root.textColor
            font.pixelSize: Theme.fontSizeSm
            font.weight: Font.Medium
            font.family: Theme.fontFamilyMono
            
            Behavior on color {
                ColorAnimation { duration: Theme.durationFast }
            }
        }
    }
}
