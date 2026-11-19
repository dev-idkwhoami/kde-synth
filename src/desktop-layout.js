// Only geometry is read; no windows or Plasma preferences are modified.
const endpoint = __ENDPOINT__;
const ownerPid = __PID__;
const pluginName = "kde-synth-visible-desktop";
const timer = new QTimer();
timer.interval = 250;
let ticks = 0;
let last = "";
function rect(r) { return {x:r.x,y:r.y,width:r.width,height:r.height}; }
function report() {
    const windows = workspace.stackingOrder;
    const outputs = [];
    for (const output of workspace.screens) {
        const desktop = workspace.currentDesktopForScreen(output);
        const area = workspace.clientArea(KWin.MaximizeArea, output, desktop);
        const obstacles = [];
        for (const w of windows) {
            if (w.minimized || w.hidden || w.opacity === 0 || !(w.normalWindow || w.dialog || w.splash)) continue;
            // Our transparent output surfaces are normal windows to the script.
            // Exclude by process identity rather than compositor-decorated titles.
            if (Number(w.pid) === ownerPid) continue;
            if (w.desktops.length && !w.desktops.some(d => d.id === desktop.id)) continue;
            if (w.activities.length && w.activities.indexOf(workspace.currentActivity) === -1) continue;
            obstacles.push(rect(w.frameGeometry));
        }
        outputs.push({name:output.name,geometry:rect(output.geometry),area:rect(area),windows:obstacles});
    }
    const data = JSON.stringify(outputs);
    if (data !== last || ticks % 8 === 0) {
        last = data;
        callDBus(endpoint,"/org/kdesynth/Layout","org.kdesynth.Layout","Report",data);
    }
    if (++ticks % 8 === 0) {
        callDBus("org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","NameHasOwner",endpoint,function(alive){
            if (!alive) {
                timer.stop();
                callDBus("org.kde.KWin","/Scripting","org.kde.kwin.Scripting","unloadScript",pluginName);
            }
        });
    }
}
timer.timeout.connect(report);
report();
timer.start();
