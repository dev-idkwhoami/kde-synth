# Third-party components

KDE Synthesizer uses system-provided libraries and tools. Installing this package
does not install the Qt online installer or Qt Creator, and it does not bundle
private copies of Qt or KDE libraries.

| Component | Usage | Upstream licensing information |
| --- | --- | --- |
| Qt 6 Core, Gui, Widgets, DBus, Qml and Quick | Dynamically linked UI and rendering | [Qt licenses](https://doc.qt.io/qt-6/licensing.html); these modules offer LGPL/GPL options |
| LayerShellQt | Dynamically linked Wayland surface integration | [KDE source and license files](https://invent.kde.org/plasma/layer-shell-qt) |
| Qt Shader Tools | Build-time shader compilation | [Module license](https://doc.qt.io/qt-6/qtshadertools-index.html#licenses-and-attributions) |
| PipeWire | System executables for capture and routing | [PipeWire source](https://gitlab.freedesktop.org/pipewire/pipewire) |
| Go standard library/runtime | Audio helper | [Go license](https://go.dev/LICENSE) |

Dependencies retain their own licenses. Their upstream notices and corresponding
sources are available through the linked projects and Arch's package sources.
Users may replace the shared system libraries with compatible modified versions;
this application does not impose restrictions on debugging those modifications.
No dependency license is replaced by the license of this project's own code.

The Go helper includes the Go runtime; its license is reproduced in
`docs/GO-LICENSE.txt` and installed alongside the application's documentation.
