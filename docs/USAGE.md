# Usage and customization

## Settings categories and motion

Settings are grouped into **Appearance** (color and flow), **Bars** (geometry and
segments), **Finish** (glow, outlines, shading, fade, peaks), **Motion**, **Audio**,
and **Behavior** (automatic playback and desktop placement). Pages scroll when
needed. **Transparency** applies to all color effects: 0% is
fully opaque, 100% invisible. It does not stop audio capture or motion analysis.

**Effect motion** is optional and independent of color. Choose **Zoom** or **Shake**.
With either enabled, **Bass cutoff** selects the split between motion and bar
heights (40–500 Hz, default 150 Hz). Frequencies below it drive motion; higher
frequencies fill the bars. The cutoff rounds upward to the next logarithmic
spectrum-band edge, and FFT resolution limits how sharp the separation can be. Turning motion off immediately restores the
full frequency range. At zero strength, bass is still excluded while motion is on.

- **Zoom depth**: 0–40% maximum enlargement (default 15%), about bottom-center.
  Outer bars may move beyond the clipped edges; vertical headroom keeps peak
  height within the selected maximum.
- **Shake strength**: 0–40 logical pixels of sideways movement (default 8 px).
  Vertical travel is 60% of that amount. The spectrum moves together in a bounded
  oscillation; the wallpaper itself and application windows do not move.

Bass attacks respond immediately and settle over 180 ms, independently of bar
fall time. Sustained bass can hold zoom or keep shake active. The visible desktop
rectangle clips both motion types, and wallpaper samples stay aligned to the
screen. All choices persist; old Bass zoom settings migrate to Effect motion / Zoom.

## Effects

Choose **Effect** in Appearance settings. Selection applies immediately and persists.
**Rainbow** keeps the original colored bars. **Wallpaper RGB shift** samples the
wallpaper at each bar pixel and rotates its hue. Greater differences between
neighboring bar heights increase their color separation. Shadows are lifted and
existing colors strengthened so dark wallpaper still produces visible bars.
The brightness curve preserves detail without clipping highlights; neutral
grayscale pixels stay grayscale.

Plasma's current desktop configuration is read every two seconds while this effect
is selected. Each output uses its own image and fill mode. Sampling uses the full
monitor image and accounts for panel offsets and the visible-desktop area's
position, rather than stretching the image into the bars. A shared texture per
monitor feeds GPU shaders; no desktop screenshots are taken and no wallpaper
preferences are changed.

This first version supports direct static image files with Plasma's standard
fill modes. Package wallpapers, animated/slideshow wallpaper plugins, and blurred
background filling fall back to rainbow on the affected output, with a short
hint under Effect. A changed image selection is picked up automatically; changes
to the contents of the same file may require reselecting the effect. Mixed-DPI
alignment and unusual panel layouts still need desktop validation.

## Bar styles

Base colors in Appearance:
- **Rainbow** and **Wallpaper RGB shift** retain the existing looks.
- **Solid color** uses the primary color picker.
- **Custom gradient** blends two selected colors, across the shared spectrum or
  from each bar's bottom to its top.
- **Intensity colors** blends low/high colors according to each bar's energy.

**Animated color flow** is a separate effect with moving rainbow colors, horizontal
or vertical direction, and signed speed
(-100–100%; magnitude 100 is one cycle per second). Negative reverses direction,
zero pauses. Its controls appear only when selected. Other effects keep their own
colors. Horizontal coordinates continue across shared desktop regions.
Existing enabled flow toggles migrate to this effect.

Finish settings are independent and can be combined with any base effect:
- **Outline only**, thickness 1–8 logical px, also outlines individual segments.
- **Neon glow**, radius 1–30 px and strength 0–100%. Halos can soften segment gaps
  and are clipped at the chosen desktop region, preserving application boundaries.
- **Fade toward tips**, 0–100% alpha reduction at the bar tops.
- **Surface shading**, Flat / Metallic / Glass, with 0–100% strength.
- **Peak caps**, with color, thickness 1–8 px, hold 0–2000 ms, and full-height fall
  100–4000 ms. Peaks follow smoothed bar levels and decay independently afterward.

All controls persist. Color pickers and mode-specific fields are disabled when
irrelevant; global transparency also applies to glow and peak caps. Whole-segment
stepping remains active for segmented bars, while caps are separate markers.

## Audio gain

**Audio → Audio gain** replaces Follow volume. The multiplier ranges from 0.1×
to 20.0×; 1.0× preserves the original input. It applies a fixed gain to the captured
samples before FFT, affecting both spectrum and bass motion without changing
speaker volume. There is no automatic normalization, compression, or sample clipping.
A 10× multiplier adds 20 dB; display heights still use the logarithmic dB mapping
and saturate at their normal maximum. Silence remains zero. Larger gains can
make existing low-level noise visible; they do not create new frequencies.

Changes apply live and persist. Old Follow volume preferences are removed and
migrate to 1.0×, avoiding the former automatic amplification of quiet content.

## Automatic activation

Enable **Follow application playback** and choose a running application. Refresh
updates the process list; typing searches it. The executable path is saved,
not its temporary PID, and the saved choice remains available after it exits.
For PipeWire streams whose process cannot be resolved, matching falls back to
the executable basename supplied by the stream/client metadata.

The enabled switch is manual permission. Automatic mode only displays bars when
that permission is on and the selected application is playing. Pause/stop hides
the bars and stops capture. Manual off stays off across playback changes and
restarts; turning permission back on resumes automatic waiting/detection.

Detection polls every 750 ms. MPRIS playback state is preferred when provided
by the selected executable (including Spotify); otherwise running PipeWire
playback streams are used. Applications that keep silent streams running may
appear active. Browser tabs and multiple instances sharing an executable are
one identity. Audio isolation applies whenever an application is selected, even
with automatic activation off. Clear returns to desktop audio; automatic playback
following is bypassed until another application is selected.

Capture verifies the executable through /proc and taps its PipeWire output ports,
without changing its normal playback links. Missing or unverifiable applications
produce silence, never desktop audio. New streams are discovered every 500 ms;
mono is duplicated to stereo and matching left/right channels are captured.
Capture does not use the basename fallback used for activation detection.

## Multi-monitor behavior

Each output has its own surface and uses Qt logical coordinates/scaling. KWin
handles panel reservations; bars sit six logical pixels above the available
bottom edge. Auto-hidden panels may not reserve space. Monitor connection and
removal create/destroy surfaces, preventing migration of duplicate surfaces.

**Fit to visible desktop** is optional. It finds the largest uncovered rectangle
per output, then concatenates those regions into one shared spectrum, ordered
left to right in global desktop coordinates (top to bottom when x is equal).
Each region receives a fraction proportional to its available logical width:
600 / 1200 / 600 pixels shows the first 25%, middle 50%, and final 25%.
Covered regions and physical gaps consume no spectrum. A fully covered output
draws nothing. Minimum useful space is 80×48 logical pixels. Height adapts to each
area independently; color progression follows the shared spectrum. Mapping updates
as windows move. Turning Fit off restores one full spectrum per monitor.

The temporary KWin script reads output work areas and visible window frame
rectangles every 250 ms, accounting for minimized windows, virtual desktops,
and activities. It ignores our own process to prevent the transparent surfaces
from counting as obstacles. It does not move windows or change desktop settings.
Transparency, shadows and unusual desktop effects are approximated by window
rectangles; this remains experimental. If KWin integration fails or becomes
stale, normal full-width placement is restored. There is no persistent plugin.

There is one analyzer for the application, not one per monitor. Wallpaper sampling
stays local to each monitor; zoom and shake keep their per-region placement.

## Settings window placement

The settings window is titled **KDE Synthesizer**. On Plasma Wayland, KWin owns
top-level window positioning. Run `scripts/remember-settings-position.sh` once
to add a narrowly scoped **Remember position** rule for this window. It leaves
other window rules intact and backs up the original `kwinrulesrc` on first use.
The rule can be removed in System Settings → Window Management → Window Rules.

Closing and reopening the application was verified to restore the settings
window from the left display to its last position on the middle display.
KWin 6.8.2 did not write the remembered coordinates to disk in this check:
[upstream bug 526933](https://bugs.kde.org/show_bug.cgi?id=526933) describes loss
of remembered positions after logout or KWin restart. The rule therefore
currently guarantees only the verified same-session behavior.

## Visualizer types

The Visualizer dropdown selects **Spectrum bars**, **Spectrum curve**,
**Waveform**, or **Mirrored spectrum**. Each retains independent settings and
shows only its own customization tab:
- Bars retain thickness, height, segments, and fall time.
- Curve has height, line thickness, neighboring-band smoothing, filled/line
  rendering, and fall time.
- Waveform has trace height, amplitude (10–400%), line thickness, audio history
  (5–200 ms), and vertical center. Soft boost (default) lifts quiet waves with a fixed square-root display response;
  Linear preserves proportional amplitude. Neither changes bass detection.
  It uses 512 time-domain points from
  the louder stereo channel, preserving opposite-phase stereo without cancellation.
  History updates live; longer windows retain each bucket’s minimum and maximum
  in chronological order so fast oscillations are not averaged away.
- Mirrored spectrum offers bars or a curve, total height, vertical center,
  lower-half scale (100% is symmetrical), and fall time. Its bars have their own
  thickness, gap, and whole-segment controls; its curve has smoothing, line
  thickness, and fill.

Color effects, transparency, glow, vertical fade, motion, and audio routing are
shared. Bar-only outline, material shading, and peak caps are hidden for other
types. Bass motion removes bass bands from spectrum types; waveform displays
the full audio signal. Fit-to-visible-desktop distributes one spectrum or one
waveform history across the available regions rather than repeating it.

Waveform redraws follow the renderer’s approximately 60 Hz timer; higher audio
analysis rates still update the latest samples without redundant Canvas paints.
Normal interactive launches use no `--seconds` timeout. The option is only for
bounded validation runs.
