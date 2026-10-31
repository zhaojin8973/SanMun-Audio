# SanMu Audio Internal Beta Product Overview

## Beta scope

This beta includes four audio plug-ins. Every beta build expires at 23:59 on October 31, 2026 (Asia/Shanghai). After expiry, the plug-in keeps its project parameters but stops effect processing and passes dry audio through.

| Product | Version | Role | macOS formats | Windows planned formats |
|---|---:|---|---|---|
| SanMu EQ6 | 0.1.5 | Dynamic parametric EQ with up to 24 band slots and linear-phase processing | VST3 / AU / AAX | VST3 / AAX |
| SanMu Comp | 0.1.17 | Four compressor modes: LEV, VCA, FET and Opto | VST3 / AU / AAX | VST3 / AAX |
| SanMu Delay | 0.1.5 | Multi-engine, multi-routing delay | VST3 / AU / AAX | VST3 / AAX |
| SanMu Reverb | 1.0.4 | Plate, Room, Hall and Chamber reverb | VST3 / AU / AAX | VST3 / AAX |

## Product notes

### SanMu EQ6

SanMu EQ6 is a parametric equalizer for mixing and mastering. It provides up to 24 band slots, Zero Latency and Linear Phase paths, live spectrum, peak hold, band audition and Mid/Side operation. Linear Phase is intended for mastering and phase-sensitive material; Zero Latency is intended for live mixing and tracking.

### SanMu Comp

SanMu Comp provides LEV, VCA, FET and Opto modes. LEV is intended for levelling, VCA for bus glue, FET for fast transient capture and Opto for smoother optical-style dynamics. The interface shows input, output, gain reduction and listening measurements.

### SanMu Delay

SanMu Delay provides Modern, Tape, BBD and Diffused engines with Independent, Ping-Pong and Cross routing. Delay time supports Sync, BPM and Free modes, with ducking, feedback, modulation and an Echo time editor.

### SanMu Reverb

SanMu Reverb provides Plate, Room, Hall and Chamber spaces. Controls cover Predelay, Decay, Size, Attack, early/late diffusion, low/high damping, modulation and output EQ. Dry, Wet and Mix views are available; the light theme uses a water surface and the dark theme uses particles/dust for the sound-field layer.

## Platform scope

### macOS

- Intel: macOS 10.15 Catalina or later.
- Apple silicon: macOS 11.0 or later; a current supported macOS is recommended.
- Use AAX in Pro Tools, AU in Logic Pro and VST3 in other compatible hosts.
- The internal packages are Apple Developer ID signed Universal 2 installers.

### Windows

- Target: Windows 10 22H2 and Windows 11 x64.
- VST3 is intended for hosts such as Cubase, REAPER and Studio One.
- AAX is intended for Pro Tools on Windows; Audio Unit is not available on Windows.
- The Windows build, signing, host scan and hardware regression are not complete, so the website keeps Windows downloads closed.
