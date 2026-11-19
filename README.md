<div align="center">
  <img src="assets/logo.png" alt="KDE Synthesizer spectrum logo" width="180" />
  <h1>KDE Synthesizer</h1>
  <p><strong>Your music. Your wallpaper. One living desktop.</strong></p>
  <p>A music visualizer for KDE Plasma on Wayland, built with Qt Quick and Go.</p>
  <p>
    <img src="https://img.shields.io/badge/Arch_Linux-x86__64-1793D1?logo=archlinux&logoColor=white" alt="Arch Linux x86_64" />
    <img src="https://img.shields.io/badge/Plasma-Wayland-1D99F3?logo=kde&logoColor=white" alt="KDE Plasma Wayland" />
    <img src="https://img.shields.io/badge/audio-PipeWire-A855F7" alt="PipeWire audio" />
    <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-34D399" alt="MIT license" /></a>
  </p>
  <p><a href="#install">Install</a> · <a href="#make-it-yours">Customize</a> · <a href="docs/USAGE.md">User guide</a> · <a href="#build-from-source">Build</a></p>
</div>

> [!WARNING]
> **Independent project — not an official KDE product.** This project is not affiliated with, endorsed by, or maintained by KDE. “KDE” in the name only indicates that it is intended for use with **KDE Plasma**.

---

KDE Synthesizer draws audio-reactive visuals **above your wallpaper and below your
windows**. The overlay lets clicks pass through, keeps your existing wallpaper,
and leaves your applications in front. Despite the name, it visualizes music;
it does not generate sound.

Early release; currently targets **Arch Linux / KDE Plasma 6 / Wayland / PipeWire**.

## Make it yours

| Visualizer | Its own controls |
| --- | --- |
| **Spectrum bars** | Thickness, height, fall time, whole segments and spacing |
| **Spectrum curve** | Filled or line-only, smoothing, height and line thickness |
| **Waveform** | Audio history, amplitude, soft boost, trace height and position |
| **Mirrored spectrum** | Bars or curve, lower-half scale, center and independent geometry |

- **Six color effects:** rainbow, wallpaper RGB shift, solid color, custom gradient,
  intensity colors, and animated color flow.
- **Wallpaper-aware color:** shift the colors underneath each pixel of the
  visualizer while preserving the image's detail.
- **Finishing touches:** transparency, glow and fading, plus bar outlines,
  metallic/glass shading and peak caps for spectrum bars.
- **Bass motion:** optional zoom or shake, adjustable strength and bass cutoff.
- **App-only audio:** select Spotify or another running executable to visualize
  only its streams. Clear the selection to use the desktop mix.
- **Playback following:** automatically show and hide with the selected app.
  Manually switching off always takes priority.
- **Multiple monitors:** one surface per display. Optional **Fit to visible
  desktop** shares one spectrum across the largest free rectangle on each monitor.

## Install

Download the package and `SHA256SUMS` from [Releases](https://github.com/dev-idkwhoami/kde-synth/releases).
Releases are published automatically when a version tag is pushed and its build passes.
For development builds, download the `kde-synth-arch-x86_64` artifact from a successful **Build Arch package and release**
run in the repository's [**Actions** tab](https://github.com/dev-idkwhoami/kde-synth/actions/workflows/arch-package.yml) and extract the ZIP. GitHub may require
sign-in for workflow artifacts. In the extracted directory:

```sh
sha256sum -c SHA256SUMS
sudo pacman -U ./kde-synth-*.pkg.tar.zst
```

Keep your Arch system fully updated before installing. The package installs the
application, its audio helper, a launcher and the logo. Pacman resolves dependencies;
there is no separate Qt installer or IDE to install.

Open **KDE Synthesizer** from the application launcher, or run:

```sh
kde-synth --settings
```

This provides a pacman package, not an AUR entry or a configured package
repository. `pacman -S kde-synth` is not available.

### Everyday controls

| Action | Result |
| --- | --- |
| Left-click the tray icon | Toggle visualization on or off |
| Right-click the tray icon | Open settings or quit |
| Close the settings window | Continue running in the tray |
| Choose an app in Behavior settings | Capture only that executable's audio |
| Clear the selected app | Return to desktop audio |

The tray icon animates while visualization is active. Settings save automatically
to `~/.config/kde-synth/settings.ini` (or your XDG config directory). Only one
instance runs at a time; use its tray menu to reopen settings.

No autostart entry is added. To start at login, add the installed application in
Plasma's **System Settings → Autostart**. To uninstall:

```sh
sudo pacman -R kde-synth
```

Your personal settings remain available if you reinstall.

## Build from source

On an up-to-date Arch system:

```sh
sudo pacman -Syu --needed base-devel cmake ninja go qt6-base qt6-declarative \
  qt6-shadertools layer-shell-qt pipewire pipewire-audio kwin desktop-file-utils
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./bin/kde-synth --settings
```

All executables build into **`./bin/`**; intermediate files stay in `build/`.
Keep the development audio helper next to the application. The installed package
instead places the helper in `/usr/lib/kde-synth/`.

Run the tests:

```sh
ctest --test-dir build --output-on-failure
GOCACHE="$PWD/build/go-cache" go test ./audio
```

Build a pacman package as your regular user, after installing the dependencies above:

```sh
./scripts/build-arch-package.sh
```

Packages and `SHA256SUMS` land in **`bin/packages/`**. The script creates a local
source archive from an explicit file list, verifies its checksum with `makepkg`,
builds and tests in isolation from your development build, and does not install
anything. See [packaging and CI](docs/PACKAGING.md).

## How it works

**PipeWire → Go audio analysis → Qt Quick → Plasma desktop.**

One Go helper captures audio and produces spectrum, bass and waveform data for all
monitors. The Qt application manages settings, tray controls and click-through
LayerShellQt surfaces. Analysis runs at a configurable 10–120 Hz; visual updates
run at approximately 60 Hz. Audio is processed locally and is not saved to disk.

Selecting an app changes capture as well as activation: missing or unverifiable
streams produce silence, never a fallback to other desktop audio. Multiple
instances and browser tabs sharing an executable are treated as one application.

### Current boundaries

- Plasma **Wayland** is the supported desktop; X11 is not supported.
- Wallpaper RGB shift supports direct static images and standard fill modes.
  Unsupported wallpaper plugins fall back to rainbow.
- Fit-to-desktop uses the **largest uncovered rectangle per monitor**, not every
  small gap. A temporary KWin script reads window geometry; it does not move windows.
- Mixed-DPI layouts, physical hotplug and unusual window effects need broader testing.
- Window positioning belongs to KWin. See the [optional remember-position rule](docs/USAGE.md#settings-window-placement).

More details: [settings, effects and audio routing](docs/USAGE.md).

## License

The project's code is available under the [MIT license](LICENSE).
Qt, KDE, PipeWire and the Go runtime retain their own licenses; see
[third-party notices](THIRD_PARTY.md). The generated project logo is included in
this repository; it is not an official KDE logo.
