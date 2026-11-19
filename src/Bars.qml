import QtQuick

Item {
    id: surface
    clip: true
    readonly property var free: visualizer.fitDesktop ? desktopLayout.regions[outputName] : undefined
    readonly property var slice: visualizer.fitDesktop ? desktopLayout.slices[outputName] : undefined
    readonly property var wallpaper: wallpapers.outputs[outputName]
    readonly property var offset: desktopLayout.offsets[outputName]
    readonly property bool wallpaperReady: visualizer.effect === "wallpaper-rgb"
        && wallpaper !== undefined && offset !== undefined && wallpaperImage.status === Image.Ready
    Rectangle {
        id: wallpaperCanvas
        width: surface.wallpaper ? surface.wallpaper.width : 1
        height: surface.wallpaper ? surface.wallpaper.height : 1
        color: surface.wallpaper ? surface.wallpaper.color : "black"
        visible: false
        Image {
            id: wallpaperImage
            anchors.fill: parent
            source: surface.wallpaper ? surface.wallpaper.source : ""
            fillMode: surface.wallpaper ? surface.wallpaper.fill : Image.PreserveAspectCrop
            asynchronous: true
        }
    }
    ShaderEffectSource {
        id: wallpaperSource
        sourceItem: surface.wallpaper ? wallpaperCanvas : null
        hideSource: true
        visible: false
        live: true
    }
    Item {
    id: scene
    x: surface.free === undefined ? 0 : surface.free.x
    y: surface.free === undefined ? 0 : surface.free.y
    width: surface.free === undefined ? surface.width : surface.free.width
    height: surface.free === undefined ? surface.height : surface.free.height
    visible: width > 0 && height > 0
    clip: true
    opacity: 1 - visualizer.transparency / 100
    readonly property bool zooming: visualizer.motionEnabled && visualizer.motionType === "zoom"
    readonly property real zoom: zooming ? 1 + visualizer.bassLevel * visualizer.bassDepth / 100 : 1
    readonly property real zoomLimit: zooming ? 1 + visualizer.bassDepth / 100 : 1
    readonly property int count: Math.max(1, Math.floor((width + 4) / (visualizer.barWidth + 4)))
    readonly property real ceiling: Math.max(0, visualizer.maxHeight === 0 ? height - 6 : Math.min(height - 6, visualizer.maxHeight))
    readonly property real spectrumStart: surface.slice === undefined ? 0 : surface.slice.start
    readonly property real spectrumSpan: surface.slice === undefined ? 1 : surface.slice.end - surface.slice.start
    function position(index) { return spectrumStart + index / count * spectrumSpan }
    function amplitude(index, levels) {
        const start = Math.max(0, Math.min(levels.length - 1, Math.floor(position(index) * levels.length)))
        const end = Math.max(start + 1, Math.ceil(position(index + 1) * levels.length))
        let peak = 0
        for (let i = start; i < Math.min(end, levels.length); ++i)
            peak = Math.max(peak, levels[i])
        return peak
    }
    Repeater {
        model: visualizer.visualizerType === "bars" ? scene.count : 0
        Item {
            id: bar
            required property int index
            readonly property real energy: scene.amplitude(index, visualizer.levels)
            // Project about bottom-center. The outer scene clips to the free
            // desktop rectangle; sampling below stays in screen coordinates.
            x: scene.width / 2 + (index * (scene.width + 4) / scene.count - scene.width / 2) * scene.zoom + visualizer.shakeX
            y: scene.height - 6 - height + visualizer.shakeY
            width: Math.max(1, (scene.width + 4) / scene.count - 4) * scene.zoom
            readonly property real rawHeight: scene.ceiling / scene.zoomLimit * energy
            // Count only complete segments; no trailing gap above the last one.
            // Quantize before zoom so motion scales intact segments together.
            readonly property int wholeSegments: Math.max(0, Math.floor(
                (rawHeight + visualizer.segmentGap) / (visualizer.segmentHeight + visualizer.segmentGap)))
            height: (visualizer.segmented
                ? Math.max(0, wholeSegments * (visualizer.segmentHeight + visualizer.segmentGap) - visualizer.segmentGap)
                : rawHeight) * scene.zoom
            readonly property real peakEnergy: scene.amplitude(index, visualizer.peakLevels)
            ShaderEffect {
                readonly property real pad: visualizer.style.glow ? visualizer.style.glowRadius * scene.zoom : 0
                x: -pad; y: -pad
                width: bar.width + pad * 2; height: bar.height + pad * 2
                visible: bar.height > 0
                property real maskEnabled: 0
                property variant shapeTexture: wallpaperSource
                property real padding: pad
                property color colorA: visualizer.style.colorA
                property color colorB: visualizer.style.colorB
                property real colorMode: surface.wallpaperReady ? 1 : visualizer.effect === "solid" ? 2
                    : visualizer.effect === "gradient" ? 3 : visualizer.effect === "intensity" ? 4 : visualizer.effect === "color-flow" ? 5 : 0
                property real energy: bar.energy
                property vector2d spectrumPosition: Qt.vector2d(scene.position(bar.index),scene.spectrumSpan/scene.count)
                property real gradientDirection: visualizer.style.direction === "vertical" ? 1 : 0
                property real outlineWidth: visualizer.style.outline ? visualizer.style.outlineWidth * scene.zoom : 0
                property real glowRadius: pad
                property real glowStrength: visualizer.style.glowStrength / 100
                property real fadeStrength: visualizer.style.fade / 100
                property real shading: visualizer.style.shading === "metal" ? 1 : visualizer.style.shading === "glass" ? 2 : 0
                property real shadeStrength: visualizer.style.shadeStrength / 100
                property real flowPhase: visualizer.animationTime
                property real flowDirection: visualizer.style.flowDirection === "vertical" ? 1 : 0
                property real segmentHeight: visualizer.segmented ? visualizer.segmentHeight * scene.zoom : 0
                property real segmentGap: visualizer.segmentGap * scene.zoom
                property variant wallpaperTexture: wallpaperSource
                property vector2d origin: Qt.vector2d(scene.x + bar.x + (surface.offset ? surface.offset.x : 0),
                    scene.y + bar.y + (surface.offset ? surface.offset.y : 0))
                property vector2d extent: Qt.vector2d(bar.width,bar.height)
                property vector2d canvasSize: Qt.vector2d(wallpaperCanvas.width,wallpaperCanvas.height)
                property real hueShift: 0.10 + bar.energy * 0.55
                    + (bar.energy - scene.amplitude(bar.index-1, visualizer.levels)) * 0.45
                fragmentShader: "qrc:/src/wallpaper.frag.qsb"
            }
            Rectangle {
                visible: visualizer.style.peaks && bar.peakEnergy > 0.0001
                width: bar.width
                height: visualizer.style.peakHeight * scene.zoom
                y: bar.height - Math.min(scene.ceiling - height,
                    scene.ceiling / scene.zoomLimit * bar.peakEnergy * scene.zoom) - height
                color: visualizer.style.peakColor
                radius: Math.min(1,height/2)
            }
        }
    }
    Trace {
        visible: visualizer.visualizerType !== "bars"
        controller: visualizer
        wallpaperTexture: wallpaperSource
        wallpaperReady: surface.wallpaperReady
        readonly property var opts: visualizer.visualizerOptions
        readonly property real center: scene.height*(visualizer.visualizerType==="waveform"?opts.waveCenter:opts.mirrorCenter)/100
        x: visualizer.shakeX
        width: scene.width
        height: Math.max(1,visualizer.visualizerType==="curve"?Math.min(scene.height-12,opts.curveHeight):
            Math.min(visualizer.visualizerType==="waveform"?opts.waveHeight:opts.mirrorHeight,2*Math.min(center,scene.height-center)-12))
        y: (visualizer.visualizerType==="curve"?scene.height-6-height:center-height/2)+visualizer.shakeY
        origin: Qt.vector2d(scene.x+x+(surface.offset?surface.offset.x:0),scene.y+y+(surface.offset?surface.offset.y:0))
        canvasSize: Qt.vector2d(wallpaperCanvas.width,wallpaperCanvas.height)
        spectrumStart: scene.spectrumStart
        spectrumSpan: scene.spectrumSpan
        zoom: scene.zoom
        zoomLimit: scene.zoomLimit
    }

}
}
