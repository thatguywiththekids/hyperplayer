**Hyperplayer SDL**

Hyperplayer SDL is a cross-platform desktop Amiga-MOD player written in C11, built around a tracker-style interface inspired by the Amiga ProTracker 2.3.
It is based on Hyperplayer that was originally developed for Windows.
It loads and plays module files, shows live playback state, displays pattern data, exposes sample information, and renders multiple synchronized visualizers in the same UI. The app initializes a default hyperplayer.ini on first start, opens a configurable default folder, and is designed around a fixed 1920×1080 interface layout. If the file hyperplayer.ini is missing, it will create it on start with all the default settings.

**THANKS!**

Thanks to Hyperunknown who released the original Hyperplayer source!

**What it does**

Hyperplayer is focused on .MOD playback and browsing. The built-in file browser shows folders plus MOD files, lets you move through drives and directories, and loads a selected module directly into the player. Once a file is loaded, the browser is hidden so the visualizer panel takes over that area instead, but can be opened again by clicking on the "File Browser" text.
The File Browser is listing all "*.mod" files aswell as files starting with "mod." as per the Amiga standard.
During playback, Hyperplayer keeps separate OpenMPT instances for rendering audio and for UI/state tracking, so it can show song position, pattern/order/row data, sample usage, waveform previews, and visual meters while the song is playing. Audio is rendered at 44.1 kHz, by default using SDL3's audio stream API (with an optional ALSA backend option on Linux).

**Main features**

Tracker-style playback view
Hyperplayer renders a 4-channel pattern view with live row highlighting. The first row of each pattern can use a separate color, the current play row is highlighted in white during playback, and the pattern view is designed to stay visually stable around the play position instead of jumping around aggressively.

**Sample list and sample waveform preview**

The player shows all 31 sample slots with number, name, volume, and size. Clicking a sample locks the sample display to that slot. If no sample is selected, the sample waveform view automatically cycles through non-empty samples every 2 seconds.

**Live sample activity highlighting**

The sample list includes animated background highlights that react to currently active samples detected from playback state.

**Multiple synchronized visualizers**

Hyperplayer includes several real-time visual components:
 * Radial tunnel/ring visualizer with starfield and glow effects
 * Spectrum analyzer
 * VU meter
 * 4-channel quadrascope
 * Sample waveform view
 * Live pattern display
 * Configurable look and behavior

A large part of the visual behavior is controlled through hyperplayer.ini, including stereo separation, pattern colors, quadrascope color, VU meter colors and transparency, sample waveform color, spectrum analyzer settings, sample highlight fade behavior, and many radial visualizer parameters. If the INI is deleted, the app recreates it with defaults on startup.

**Controls**

**Space:** play / pause
**Ctrl + R:** restart playback from the current pattern/order position and play
**S:** stop
**Left Arrow:** previous pattern/order
**Right Arrow:** next pattern/order
**Up Arrow:** load and play previous MOD in the current folder
**Down Arrow:** load and play next MOD in the current folder
**F11 or Alt + Enter:** toggle fullscreen mode
**F10 or Alt + B:** toggle borderless window mode (only when windowed)
**Escape:** quit the application

**How to use it**

On first launch, the app creates hyperplayer.ini if it does not already exist.
The browser opens in the folder defined by DEFAULTDIR. If DEFAULTDIR=. then it starts in the same directory as the executable.
Browse to a folder containing .MOD files.
Click a module to load it.
Press Space or use the on-screen Play button to start playback.
While playing, use the pattern view, sample list, waveform display, spectrum analyzer, quadrascope, VU meter, and radial visualizer to inspect the module in real time.

**What it uses**

Hyperplayer is built with C11 and uses:
 * **SDL3:** For cross-platform window management, graphics rendering (via a custom drawing wrapper), and audio output (by default).
 * **ALSA (Linux only):** An optional audio backend, primarily for demonstrating a custom/alternative audio backend implementation.
 * **libopenmpt:** For module decoding, pattern access, metadata, timing, and playback state.
 * **SDL3_image:** For loading PNG image assets.
 * **SDL3_ttf:** For high-quality text rendering.
 * **Win32 Portability Shim:** A custom portability layer that shims basic Win32 API functions (e.g., INI parsing, ticks, file path utilities) to allow the original Windows-based utility code to run on POSIX systems.
 * **Double-buffered drawing:** To ensure smooth UI updates and prevent flickering.

**Configuration**

Hyperplayer reads settings from hyperplayer.ini. Current configuration sections include:
* **SYSTEM**: startup folder (`DEFAULTDIR`), borderless window (`BORDERLESS`), and fullscreen mode (`FULLSCREEN`).
* **AUDIO**
* **SAMPLELIST**
* **PATTERN**
* **QUADRASCOPE**
* **VUMETER**
* **SAMPLEVIEW**
* **SPECTRUMANALYZER**
* **VISUALIZER**

This makes it possible to change the startup folder, toggle borderless or fullscreen window states, modify the stereo image, adjust colors and visualizer layouts, and fine-tune the rendering parameters without recompiling.

**Building**

The project uses CMake for building across different platforms.

### Prerequisites
- CMake 3.25+
- SDL3, SDL3_image, SDL3_ttf
- libopenmpt 0.6.0+ (pkg-config)
- A C11 compatible compiler (GCC, Clang, MSVC)
- ALSA development libraries (optional, only needed for compiling with the ALSA backend on Linux)

### Build Commands
```bash
# Create build directory (using default SDL3 audio backend)
cmake -B build

# Or create build directory using ALSA audio backend (Linux only)
cmake -B build -DAUDIO_BACKEND=ALSA

# Build the project
cmake --build build
```

**Additional documentation**

ARCHITECTURE.md describes the software architecture.

**http://www.hyperunknown.net**

![Hyperplayer screenshot](screenshots/hyperplayer-screenshot.png)
