# SanMu Audio Internal Beta User Manual

## 1. Installation

Quit the host before installation. The macOS package lets you select VST3, Audio Unit (AU) and AAX. Select AAX for Pro Tools, AU for Logic Pro and usually VST3 for other hosts. Restart the host and rescan plug-ins after installation.

macOS paths:

- VST3: `/Library/Audio/Plug-Ins/VST3`
- AU: `/Library/Audio/Plug-Ins/Components`
- AAX: `/Library/Application Support/Avid/Audio/Plug-Ins`

Windows paths after a Windows installer is available:

- VST3: `C:\Program Files\Common Files\VST3`
- AAX: `C:\Program Files\Common Files\Avid\Audio\Plug-Ins`

## 2. Common controls

- Drag a knob vertically; use the wheel for fine adjustment.
- Double-click a knob, or Command/Ctrl-click it, to restore its default.
- Focus a control and use the arrow keys; Home/End reach the limits.
- `A/B` stores two sound states; the copy control copies between them.
- `BYP` bypasses effect processing.
- Theme, language, tooltip and interface-scale preferences are stored on the machine and reused by later instances.
- The host stores project parameters separately from interface preferences.

## 3. SanMu EQ6

- **Zero Latency** is the real-time, minimum-phase path.
- **Linear Phase** is the FIR path for mastering and parallel work; the host reports and compensates its added latency.
- Each band exposes frequency, gain, Q, filter type and dynamic parameters. Dragging a point on the curve should release the host for playback immediately.
- Input/output spectrum and peak hold help locate frequencies. Peak display must not prevent selecting a band.

## 4. SanMu Comp

LEV is for levelling, VCA for buses, FET for fast transients and Opto for smooth dynamics. The graph shows input, output and gain reduction. Freezing the graph only freezes its display; audio and meters continue. Clear peaks resets peak hold and overload notices.

## 5. SanMu Delay

Modern is a clear digital delay; Tape adds tape colour and modulation; BBD is darker and grainier; Diffused creates a wider echo tail. Independent, Ping-Pong and Cross are the routing modes. Sync, BPM and Free set time; Ducking reduces the wet signal while the input is active.

## 6. SanMu Reverb

Plate is dense and continuous, Room is close and defined, Hall is wide and open, and Chamber has a room-like character. Predelay sets the gap before the first reflections; Decay sets tail length; Size sets scale; damping, diffusion, modulation and output EQ shape the tail. Dry, Wet and Mix only change the sound-field view and do not alter the wet/dry balance.

The light theme shows water and the dark theme shows particles/dust. Turning off the sound-field display does not stop reverb processing or meters.

## 7. Beta expiry

The beta ends at 23:59 on October 31, 2026 (Asia/Shanghai). After expiry, effect processing stops and dry audio passes through while project parameters remain. Freeze or print important tracks before the deadline.

## 8. Feedback

Include the plug-in and version, OS version, host and version, plug-in format, mono/stereo state, reproduction steps and a screenshot. If a plug-in is missing, quit the host completely, confirm the install path and rescan.
