import QtQuick 2.15
import QtQuick.Controls 2.15
import "../theme"

/**
 * ActionButton - Icon button for download actions
 */
AbstractButton {
    id: root
    
    implicitWidth: 32
    implicitHeight: 32
    
    // Public properties
    property string iconSource: ""
    property color iconColor: Theme.textSecondary
    property color hoverColor: Theme.primary
    property color backgroundColor: "transparent"
    property color hoverBackgroundColor: Theme.glassBgHover
    property string tooltip: ""
    property int iconSize: 18
    
    // States
    hoverEnabled: true
    
    background: Rectangle {
        radius: Theme.radiusMd
        color: root.hovered ? root.hoverBackgroundColor : root.backgroundColor
        
        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }
    }
    
    contentItem: Item {
        // Icon using Canvas (for built-in icons) or Image
        Canvas {
            id: iconCanvas
            anchors.centerIn: parent
            width: root.iconSize
            height: root.iconSize
            visible: root.iconSource !== ""
            
            property color drawColor: root.hovered ? root.hoverColor : root.iconColor
            
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = drawColor
                ctx.fillStyle = drawColor
                ctx.lineWidth = 2
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                
                var w = width
                var h = height
                
                switch (root.iconSource) {
                    case "pause":
                        drawPause(ctx, w, h)
                        break
                    case "resume":
                    case "play":
                        drawPlay(ctx, w, h)
                        break
                    case "cancel":
                    case "close":
                        drawClose(ctx, w, h)
                        break
                    case "add":
                    case "plus":
                        drawPlus(ctx, w, h)
                        break
                    case "settings":
                        drawSettings(ctx, w, h)
                        break
                    case "folder":
                        drawFolder(ctx, w, h)
                        break
                    case "retry":
                        drawRetry(ctx, w, h)
                        break
                    case "delete":
                    case "trash":
                        drawTrash(ctx, w, h)
                        break
                }
            }
            
            function drawPause(ctx, w, h) {
                var barWidth = w * 0.2
                var gap = w * 0.2
                ctx.fillRect(w/2 - gap/2 - barWidth, h * 0.2, barWidth, h * 0.6)
                ctx.fillRect(w/2 + gap/2, h * 0.2, barWidth, h * 0.6)
            }
            
            function drawPlay(ctx, w, h) {
                ctx.beginPath()
                ctx.moveTo(w * 0.25, h * 0.15)
                ctx.lineTo(w * 0.85, h * 0.5)
                ctx.lineTo(w * 0.25, h * 0.85)
                ctx.closePath()
                ctx.fill()
            }
            
            function drawClose(ctx, w, h) {
                var margin = w * 0.2
                ctx.beginPath()
                ctx.moveTo(margin, margin)
                ctx.lineTo(w - margin, h - margin)
                ctx.moveTo(w - margin, margin)
                ctx.lineTo(margin, h - margin)
                ctx.stroke()
            }
            
            function drawPlus(ctx, w, h) {
                var margin = w * 0.2
                ctx.beginPath()
                ctx.moveTo(w/2, margin)
                ctx.lineTo(w/2, h - margin)
                ctx.moveTo(margin, h/2)
                ctx.lineTo(w - margin, h/2)
                ctx.stroke()
            }
            
            function drawSettings(ctx, w, h) {
                // Simple gear icon
                ctx.beginPath()
                ctx.arc(w/2, h/2, w * 0.25, 0, 2 * Math.PI)
                ctx.stroke()
                
                for (var i = 0; i < 8; i++) {
                    var angle = (i / 8) * 2 * Math.PI
                    var x1 = w/2 + Math.cos(angle) * w * 0.25
                    var y1 = h/2 + Math.sin(angle) * h * 0.25
                    var x2 = w/2 + Math.cos(angle) * w * 0.4
                    var y2 = h/2 + Math.sin(angle) * h * 0.4
                    ctx.beginPath()
                    ctx.moveTo(x1, y1)
                    ctx.lineTo(x2, y2)
                    ctx.stroke()
                }
            }
            
            function drawFolder(ctx, w, h) {
                ctx.beginPath()
                ctx.moveTo(w * 0.1, h * 0.3)
                ctx.lineTo(w * 0.1, h * 0.8)
                ctx.lineTo(w * 0.9, h * 0.8)
                ctx.lineTo(w * 0.9, h * 0.35)
                ctx.lineTo(w * 0.5, h * 0.35)
                ctx.lineTo(w * 0.4, h * 0.25)
                ctx.lineTo(w * 0.1, h * 0.25)
                ctx.closePath()
                ctx.stroke()
            }
            
            function drawRetry(ctx, w, h) {
                ctx.beginPath()
                ctx.arc(w/2, h/2, w * 0.3, -0.5, 2 * Math.PI - 0.5)
                ctx.stroke()
                
                // Arrow
                ctx.beginPath()
                ctx.moveTo(w * 0.7, h * 0.2)
                ctx.lineTo(w * 0.8, h * 0.35)
                ctx.lineTo(w * 0.6, h * 0.35)
                ctx.closePath()
                ctx.fill()
            }
            
            function drawTrash(ctx, w, h) {
                // Lid
                ctx.beginPath()
                ctx.moveTo(w * 0.15, h * 0.25)
                ctx.lineTo(w * 0.85, h * 0.25)
                ctx.stroke()
                
                ctx.beginPath()
                ctx.moveTo(w * 0.35, h * 0.25)
                ctx.lineTo(w * 0.35, h * 0.15)
                ctx.lineTo(w * 0.65, h * 0.15)
                ctx.lineTo(w * 0.65, h * 0.25)
                ctx.stroke()
                
                // Can
                ctx.beginPath()
                ctx.moveTo(w * 0.2, h * 0.25)
                ctx.lineTo(w * 0.25, h * 0.85)
                ctx.lineTo(w * 0.75, h * 0.85)
                ctx.lineTo(w * 0.8, h * 0.25)
                ctx.stroke()
            }
            
            onDrawColorChanged: requestPaint()
            
            Behavior on drawColor {
                ColorAnimation { duration: Theme.durationFast }
            }
        }
    }
    
    // Tooltip
    ToolTip {
        visible: root.hovered && root.tooltip !== ""
        text: root.tooltip
        delay: 500
        
        background: Rectangle {
            color: Theme.surface
            border.color: Theme.glassBorder
            radius: Theme.radiusSm
        }
        
        contentItem: Text {
            text: root.tooltip
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeSm
        }
    }
}
