import QtQuick

Item {
    id: trace
    required property var controller
    required property var wallpaperTexture
    required property bool wallpaperReady
    required property vector2d origin
    required property vector2d canvasSize
    property real spectrumStart: 0
    property real spectrumSpan: 1
    property real zoom: 1
    property real zoomLimit: 1
    readonly property var options: controller.visualizerOptions
    readonly property bool wave: controller.visualizerType === "waveform"
    readonly property bool mirrored: controller.visualizerType === "mirrored"
    readonly property var samples: wave ? controller.waveform : controller.levels
    readonly property real strength: {
        let peak = 0
        for (let i=0;i<samples.length;++i) peak=Math.max(peak,Math.abs(samples[i]))
        return peak
    }
    function valueAt(position) {
        if (!samples.length) return 0
        const at=Math.max(0,Math.min(samples.length-1,position*(samples.length-1)))
        const lo=Math.floor(at), hi=Math.min(samples.length-1,lo+1)
        if(wave) return samples[lo]*(hi-at)+samples[hi]*(at-lo)+(hi===lo?samples[lo]:0)
        const radius=mirrored?options.mirrorSmooth:options.curveSmooth
        let sum=0, count=0
        for(let j=-radius;j<=radius;++j){
            const a=Math.max(0,Math.min(samples.length-1,lo+j))
            const b=Math.min(samples.length-1,a+1)
            sum+=samples[a]*(1-(at-lo))+samples[b]*(at-lo);++count
        }
        return sum/count
    }
    Canvas {
        id: mask
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx=getContext("2d")
            ctx.clearRect(0,0,width,height)
            if(trace.strength<0.00001 || width<=0 || height<=0) return
            ctx.fillStyle="white";ctx.strokeStyle="white";ctx.lineJoin="round";ctx.lineCap="round"
            const o=trace.options
            const line=trace.wave?o.waveLine:trace.mirrored?o.mirrorLine:o.curveLine
            const base=trace.wave||trace.mirrored?height/2:height-line/2
            const ratio=trace.mirrored?o.mirrorScale/100:1
            const extent=(trace.wave||trace.mirrored?(height-line)/2/Math.max(1,ratio):height-line)/trace.zoomLimit*trace.zoom
            function px(t){return width/2+(t*width-width/2)*trace.zoom}
            function level(t){return trace.valueAt(trace.spectrumStart+t*trace.spectrumSpan)}
            function segmentBar(x,y,w,h,down) {
                if(!o.mirrorSegments){ctx.fillRect(x,y,w,h);return}
                const sh=o.mirrorSegmentHeight*trace.zoom, gap=o.mirrorSegmentGap*trace.zoom
                const count=Math.floor((h+gap)/(sh+gap))
                for(let s=0;s<count;++s)ctx.fillRect(x,down?y+s*(sh+gap):y+h-sh-s*(sh+gap),w,sh)
            }
            if(trace.mirrored&&o.mirrorStyle==="bars"){
                const count=Math.max(1,Math.floor((width+o.mirrorGap)/(o.mirrorWidth+o.mirrorGap)))
                const pitch=(width+o.mirrorGap)/count
                for(let i=0;i<count;++i){
                    let energy=0
                    const lo=Math.floor((trace.spectrumStart+i/count*trace.spectrumSpan)*trace.samples.length)
                    const hi=Math.ceil((trace.spectrumStart+(i+1)/count*trace.spectrumSpan)*trace.samples.length)
                    for(let j=lo;j<hi&&j<trace.samples.length;++j)energy=Math.max(energy,trace.samples[j])
                    const h=energy*extent, x=px(i/count), w=Math.max(1,pitch-o.mirrorGap)*trace.zoom
                    segmentBar(x,base-h,w,h,false);segmentBar(x,base,w,h*ratio,true)
                }
                return
            }
            const points=trace.wave?Math.max(2,Math.ceil(512*trace.spectrumSpan)):Math.max(64,Math.ceil(256*trace.spectrumSpan))
            ctx.lineWidth=line*trace.zoom
            function path(lower){
                ctx.beginPath()
                for(let i=0;i<=points;++i){
                    const t=i/points
                    const amplitude=trace.wave?level(t)*o.waveAmplitude/100:level(t)
                    const y=base+(lower?1:-1)*amplitude*extent*(lower?ratio:1)
                    if(i===0)ctx.moveTo(px(t),y);else ctx.lineTo(px(t),y)
                }
                const filled=!trace.wave&&(trace.mirrored?o.mirrorFill:o.curveFill)
                if(filled){ctx.lineTo(px(1),base);ctx.lineTo(px(0),base);ctx.closePath();ctx.fill();ctx.stroke()}
                else ctx.stroke()
            }
            path(false)
            if(trace.mirrored)path(true)
        }
    }
    Connections {
        target: trace.controller
        enabled: trace.visible
        function onLevelsChanged(){mask.requestPaint()}
        function onSettingsChanged(){mask.requestPaint()}
    }
    onSpectrumStartChanged: mask.requestPaint()
    onSpectrumSpanChanged: mask.requestPaint()
    onZoomChanged: mask.requestPaint()
    ShaderEffectSource { id: maskSource; sourceItem: mask; hideSource: true; visible: false; live: true }
    ShaderEffect {
        anchors.fill: parent
        property real maskEnabled: 1
        property variant shapeTexture: maskSource
        property real padding: 0
        property color colorA: trace.controller.style.colorA
        property color colorB: trace.controller.style.colorB
        property real colorMode: trace.wallpaperReady ? 1 : trace.controller.effect === "solid" ? 2
            : trace.controller.effect === "gradient" ? 3 : trace.controller.effect === "intensity" ? 4 : trace.controller.effect === "color-flow" ? 5 : 0
        property real energy: trace.strength
        property vector2d spectrumPosition: Qt.vector2d(trace.spectrumStart,trace.spectrumSpan)
        property real gradientDirection: trace.controller.style.direction === "vertical" ? 1 : 0
        property real outlineWidth: 0
        property real glowRadius: trace.controller.style.glow ? trace.controller.style.glowRadius : 0
        property real glowStrength: trace.controller.style.glowStrength/100
        property real fadeStrength: trace.controller.style.fade/100
        property real shading: 0
        property real shadeStrength: 0
        property real flowPhase: trace.controller.animationTime
        property real flowDirection: trace.controller.style.flowDirection === "vertical" ? 1 : 0
        property real segmentHeight: 0
        property real segmentGap: 0
        property variant wallpaperTexture: trace.wallpaperTexture
        property vector2d origin: trace.origin
        property vector2d extent: Qt.vector2d(trace.width,trace.height)
        property vector2d canvasSize: trace.canvasSize
        property real hueShift: .10+trace.strength*.55
        fragmentShader: "qrc:/src/wallpaper.frag.qsb"
    }
}
