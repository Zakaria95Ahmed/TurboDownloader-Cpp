import QtQuick 2.15
import "../theme"

/**
 * ProgressBar - Custom progress bar with animations
 */
Item {
    id: root
    
    height: Theme.progressHeight
    
    // Public properties
    property real value: 0  // 0-100
    property color fillColor: Theme.progressFill
    property color backgroundColor: Theme.progressBg
    property bool animated: true
    property bool showStripes: true
    property bool indeterminate: false
    
    // Background track
    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: root.backgroundColor
        clip: true
        
        // Fill bar
        Rectangle {
            id: fill
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            
            width: root.indeterminate ? parent.width * 0.3 : (parent.width * root.value / 100)
            radius: parent.radius
            color: root.fillColor
            
            Behavior on width {
                enabled: root.animated && !root.indeterminate
                NumberAnimation { 
                    duration: Theme.durationNormal
                    easing.type: Theme.easingType
                }
            }
            
            // Animated stripes for active downloads
            Item {
                anchors.fill: parent
                visible: root.showStripes && root.value > 0 && root.value < 100
                clip: true
                
                Row {
                    id: stripes
                    spacing: 0
                    
                    property real stripeWidth: 20
                    
                    x: -stripeWidth + (stripesAnimation.running ? stripesAnimation.offset : 0)
                    
                    Repeater {
                        model: Math.ceil(fill.width / stripes.stripeWidth) + 2
                        
                        Rectangle {
                            width: stripes.stripeWidth
                            height: fill.height * 2
                            rotation: -45
                            y: -fill.height / 2
                            
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "transparent" }
                                GradientStop { position: 0.5; color: Qt.rgba(255, 255, 255, 0.15) }
                                GradientStop { position: 1.0; color: "transparent" }
                            }
                        }
                    }
                }
                
                NumberAnimation {
                    id: stripesAnimation
                    running: root.showStripes && root.value > 0 && root.value < 100
                    loops: Animation.Infinite
                    duration: 1000
                    
                    property real offset: 0
                    from: 0
                    to: stripes.stripeWidth
                    
                    onRunningChanged: offset = 0
                    
                    ScriptAction {
                        script: stripesAnimation.offset = stripesAnimation.to
                    }
                }
            }
            
            // Glow effect
            Rectangle {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: parent.height
                visible: root.value > 0 && root.value < 100
                
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "transparent" }
                    GradientStop { position: 1.0; color: Qt.rgba(root.fillColor.r, root.fillColor.g, root.fillColor.b, 0.5) }
                }
            }
        }
        
        // Indeterminate animation
        SequentialAnimation {
            running: root.indeterminate
            loops: Animation.Infinite
            
            NumberAnimation {
                target: fill
                property: "x"
                from: -fill.width
                to: track.width
                duration: 1500
                easing.type: Easing.InOutQuad
            }
        }
    }
}
