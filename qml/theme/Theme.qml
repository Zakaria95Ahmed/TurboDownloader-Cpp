pragma Singleton
import QtQuick 2.15

/**
 * OpenIDM Theme - Glassmorphism Design System
 * 
 * This singleton provides centralized design tokens for the entire application.
 * Features a modern dark theme with glassmorphism effects.
 */
QtObject {
    id: theme
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Color Palette
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Primary Colors
    readonly property color primary: "#6366F1"           // Indigo
    readonly property color primaryLight: "#818CF8"
    readonly property color primaryDark: "#4F46E5"
    readonly property color primaryHover: "#7C3AED"
    
    // Accent Colors
    readonly property color accent: "#22D3EE"            // Cyan
    readonly property color accentLight: "#67E8F9"
    readonly property color success: "#10B981"           // Emerald
    readonly property color successLight: "#34D399"
    readonly property color warning: "#F59E0B"           // Amber
    readonly property color warningLight: "#FBBF24"
    readonly property color error: "#EF4444"             // Red
    readonly property color errorLight: "#F87171"
    
    // Background Colors (Dark Theme)
    readonly property color background: "#0F0F23"        // Deep dark blue
    readonly property color backgroundAlt: "#1A1A2E"    // Slightly lighter
    readonly property color surface: "#16213E"           // Card/panel surface
    readonly property color surfaceHover: "#1E2A4A"
    readonly property color surfaceActive: "#243B55"
    
    // Glass Effect Colors
    readonly property color glassBg: Qt.rgba(255, 255, 255, 0.05)
    readonly property color glassBgHover: Qt.rgba(255, 255, 255, 0.08)
    readonly property color glassBorder: Qt.rgba(255, 255, 255, 0.1)
    readonly property color glassBorderHover: Qt.rgba(255, 255, 255, 0.2)
    
    // Text Colors
    readonly property color textPrimary: "#F8FAFC"       // Almost white
    readonly property color textSecondary: "#94A3B8"     // Muted
    readonly property color textTertiary: "#64748B"      // Even more muted
    readonly property color textInverse: "#0F172A"       // For light backgrounds
    
    // State Colors
    readonly property color downloading: "#22D3EE"       // Cyan - active
    readonly property color paused: "#F59E0B"            // Amber
    readonly property color completed: "#10B981"         // Green
    readonly property color failed: "#EF4444"            // Red
    readonly property color queued: "#64748B"            // Gray
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Typography
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Font Family
    readonly property string fontFamily: "Inter, -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif"
    readonly property string fontFamilyMono: "'JetBrains Mono', 'Fira Code', Consolas, monospace"
    
    // Font Sizes
    readonly property int fontSizeXs: 10
    readonly property int fontSizeSm: 12
    readonly property int fontSizeMd: 14
    readonly property int fontSizeLg: 16
    readonly property int fontSizeXl: 18
    readonly property int fontSize2xl: 20
    readonly property int fontSize3xl: 24
    readonly property int fontSize4xl: 30
    
    // Font Weights
    readonly property int fontWeightNormal: 400
    readonly property int fontWeightMedium: 500
    readonly property int fontWeightSemibold: 600
    readonly property int fontWeightBold: 700
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Spacing
    // ═══════════════════════════════════════════════════════════════════════════
    
    readonly property int spacing0: 0
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 20
    readonly property int spacing2xl: 24
    readonly property int spacing3xl: 32
    readonly property int spacing4xl: 40
    readonly property int spacing5xl: 48
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Border Radius
    // ═══════════════════════════════════════════════════════════════════════════
    
    readonly property int radiusNone: 0
    readonly property int radiusSm: 4
    readonly property int radiusMd: 8
    readonly property int radiusLg: 12
    readonly property int radiusXl: 16
    readonly property int radius2xl: 20
    readonly property int radiusFull: 9999
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Shadows
    // ═══════════════════════════════════════════════════════════════════════════
    
    readonly property string shadowSm: "0 1px 2px 0 rgba(0, 0, 0, 0.3)"
    readonly property string shadowMd: "0 4px 6px -1px rgba(0, 0, 0, 0.4)"
    readonly property string shadowLg: "0 10px 15px -3px rgba(0, 0, 0, 0.5)"
    readonly property string shadowXl: "0 20px 25px -5px rgba(0, 0, 0, 0.6)"
    readonly property string shadowGlow: "0 0 20px rgba(99, 102, 241, 0.3)"
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Animation
    // ═══════════════════════════════════════════════════════════════════════════
    
    readonly property int durationFast: 150
    readonly property int durationNormal: 250
    readonly property int durationSlow: 350
    readonly property int durationSlowest: 500
    
    readonly property int easingType: Easing.OutCubic
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Layout
    // ═══════════════════════════════════════════════════════════════════════════
    
    readonly property int sidebarWidth: 240
    readonly property int sidebarCollapsedWidth: 64
    readonly property int headerHeight: 56
    readonly property int toolbarHeight: 48
    readonly property int downloadItemHeight: 72
    readonly property int downloadItemHeightExpanded: 140
    
    // Breakpoints
    readonly property int breakpointSm: 640
    readonly property int breakpointMd: 768
    readonly property int breakpointLg: 1024
    readonly property int breakpointXl: 1280
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Progress Bar
    // ═══════════════════════════════════════════════════════════════════════════
    
    readonly property int progressHeight: 6
    readonly property int progressHeightLarge: 8
    readonly property color progressBg: Qt.rgba(255, 255, 255, 0.1)
    readonly property color progressFill: primary
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Helper Functions
    // ═══════════════════════════════════════════════════════════════════════════
    
    function stateColor(state) {
        switch (state) {
            case 2: return downloading  // Downloading
            case 3: return paused       // Paused
            case 6: return completed    // Completed
            case 7: return failed       // Failed
            default: return queued      // Queued and others
        }
    }
    
    function stateText(state) {
        switch (state) {
            case 0: return qsTr("Queued")
            case 1: return qsTr("Probing")
            case 2: return qsTr("Downloading")
            case 3: return qsTr("Paused")
            case 4: return qsTr("Merging")
            case 5: return qsTr("Verifying")
            case 6: return qsTr("Completed")
            case 7: return qsTr("Failed")
            default: return qsTr("Unknown")
        }
    }
    
    function formatBytes(bytes) {
        if (bytes < 0) return "Unknown"
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KB"
        if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + " MB"
        return (bytes / (1024 * 1024 * 1024)).toFixed(2) + " GB"
    }
    
    function formatSpeed(bytesPerSecond) {
        return formatBytes(bytesPerSecond) + "/s"
    }
    
    function formatDuration(ms) {
        if (ms < 0) return "--:--"
        var seconds = Math.floor(ms / 1000)
        var minutes = Math.floor(seconds / 60)
        var hours = Math.floor(minutes / 60)
        
        seconds = seconds % 60
        minutes = minutes % 60
        
        if (hours > 0) {
            return hours + "h " + minutes + "m " + seconds + "s"
        } else if (minutes > 0) {
            return minutes + "m " + seconds + "s"
        } else {
            return seconds + "s"
        }
    }
}
