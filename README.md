MOSHRACK


// HYBRID VIDEO SYNTHESIS & psuedo-DATAMOSHING ENGINE
1. Getting Your Video Started (Row 1)
This top row tells MOSHRACK where to pull its video from.

The Source Dropdown: Click this to see your connected webcams and capture cards. To use a pre-recorded video clip, select [ Load Video File... ] at the bottom.

The Action Button: Once you pick a source, click this to fire it up.

Load Video: Ready to start your first feed.

Swap (Bright Cyan): Click to seamlessly transition to a newly selected camera or file.

Loaded: Your current feed is live and active.

2. Connecting Audio & MIDI (Rows 2 & 3)
Sync up your external gear so your effects react to sound and physical controllers.

Poll HW Button: Plugged in a MIDI controller or mic after opening the app? Click this to refresh the system and find your new gear.

Select MIDI Source: Pick your physical MIDI controller to map it to visual effects.

Select Audio Source: Pick your microphone or desktop audio loopback. This feeds sound into the Patch Bay so visuals react to the beat.

3. Performance Controls & LCD (Row 4)
Live, easily accessible master controls for jamming.

FREEZE: Instantly pauses the incoming video feed, holding the current frame. Click again to unfreeze.

SPLASH Toggle: Turns your screensaver or custom splash image on and off.

Pro Tip: Right-click the SPLASH button to load your own custom image or clear it to use the built-in MOSHRACK screensaver.

FADE Knob: Smoothly crossfades between your live video feed and your Splash screen.

WET/DRY Knob: Master effect mix. Turn it down to bypass effects and see clean video.

LCD Display: The green screen on the right shows system status, FPS stability, and audio/MIDI links.

VFX Chain Order Panel: At the very bottom, a horizontal bar shows the exact order your video flows through the effect modules. Click and drag any effect block left or right to change how they stack.

The VFX Modules Rack
Plaintext

The Golden Rule: Every knob can be clicked and dragged, but you can also hover your mouse over a knob and use your scroll wheel to turn it. Hold Shift while scrolling for ultra-fine tuning.

1. Sine LFO Generator
Your auto-pilot. It generates a rhythmic, invisible waving signal routed in the Patch Bay to automatically twist other knobs to the beat.

LFO ON: Toggles the auto-pilot wave.

RATE: Oscillation speed (0.1 Hz crawl to 5.0 Hz wobble).

GAIN: Wave depth. Low for subtle wiggles, 100% for massive swings.

BIAS: Shifts the wave's center point. (Left for negative, right for positive, center for zero offset).

TAP & SYNC: Click TAP repeatedly to set BPM manually, or SYNC to let the engine detect tempo from incoming audio over 16 beats.

2. Mosh Effect
Breaks the video compression algorithm for gooey, melting transitions.

MOSH ON: Activates the layer.

Motion: Gatekeeper. Turn up to ignore subtle movements; only big, fast motions trigger smearing.

MOSH!!: Master intensity. Crank to melt the screen.

SIZE: Pixel block size. Low for liquid melt, high for 8-bit digital destruction.

RNBW!!: Injects a psychedelic, chromatic color-shifting rainbow into smeared pixels.

3. Optical Flow
Analyzes pixel movement to turn video into a distorted, liquid surface.

FLOW!!: Main distortion warp. Turn up to make moving objects drag through water.

STRTN: Boosts base color and gloss intensity.

WET: Adds a reflective, watery shine to the brightest parts of the video.

4. KScope (Kaleidoscope)
Folds video into rotating, symmetrical geometry and infinite fractal tunnels.

K_ROT: Rotation speed. Turn left for counter-clockwise, right for clockwise. Dead center stops.

SEGMENTS: Choose mirror slices (even numbers: 2, 4, 6, 8...).

OFFSET: Phase-shifts mirrors, twisting shapes into bizarre patterns.

TUNNEL: Creates a recursive feedback loop for an infinite fractal mirror hole.

5. Chroma
Splits colors, mimicking broken lenses or VHS tracking errors.

DEPTH: Pushes RGB channels apart. Subtle 3D-glasses look at low levels, color blasting at high levels.

WARP SPD: Grabs the color spectrum and scrolls it. Max out for a strobe effect.

CRAZY: Neon tri-color split (Cyan, Green, Pink) for glowing ghosting effects.

6. Rutt/Etra
Turns flat video into a 3D wireframe grid reacting to the image underneath (inspired by the classic 1970s analog synthesizer).

SCALE: Grid size. Low for detailed nets, high for chunky polygons.

X, Y, & Z OSC: Chaos injectors. X/Y shake the mesh; Z pushes it violently toward/away from the camera.

Topology Buttons: Switch wireframe style between Triangles, Horizontal Lines, or Vertical Lines.

B&W / Invert / Luma Key: Strips color, flips colors, or cuts out the background based on brightness (using the Thr slider).

7. Zoom & 3D Space
ZOOM: Scales video up or down.

ROT X & ROT Y: Tilts the video forward/backward or pans it left/right.

8. RGB Gain & Key
Master color correction and greenscreen extraction.

R, G, B: Bipolar color correctors. Boost/cut individually.

LUMA / CHROMA: Mutually exclusive keys. LUMA makes dark/bright parts transparent. CHROMA keys out a specific color (greenscreen).

THRESH: Controls the aggressiveness of the active cutout.

The Patch Bay
1. Audio Input & Monitor
Input Gain: Volume knob for incoming sound. Scroll up to boost signal up to 10x.

VU Meter: Shows real-time volume of Bass, Mid, and High frequencies. Keep them bouncing dynamically—if they are stuck right, turn the gain down.

2. Modulation Sources (Top Jacks)
The "senders."

BASS, MID, HI: Sends signals based on specific frequency volumes (e.g., BASS pulses on kick drums).

LFO: Sends the rhythmic wave from the Sine LFO Generator.

PITCH & VEL: From a MIDI keyboard, PITCH sends high/low note signals, and VEL (Velocity) sends how hard keys are hit.

3. VFX Parameter Inputs (Bottom Jacks)
The "receivers." Hard-wired directly to specific VFX rack knobs. A signal here acts like an invisible hand turning the knob to the beat.

4. How to Patch
Connect: Click and hold a Source jack (top), drag the glowing cable to a Destination jack (bottom), and release.

Disconnect: Right-click the destination jack, or left-click and drag the cable away to rip it out.

5. The Sensitivity Ring
Adjust how strongly audio twists a patched knob.

The Ghost Arc: Hover over a patched destination jack and scroll your mouse wheel. A magenta ring appears. Scroll up to multiply strength (up to 5x), scroll down to shrink it.

6. Mini Monitor
Dedicated preview screen.

Auto-Scaling: Fits your window perfectly without stretching.

CRT Power Switch: Right-click the monitor for a retro CRT TV shutdown animation. Right-click again to turn it back on.

How to Build from Source
If you want to compile MOSHRACK yourself instead of grabbing the latest beta release, here is the standard workflow:

Prerequisites:

Visual Studio 2022 (with "Desktop development with C++" workload installed)

OpenCV 4.1.2 (Ensure opencv_world4120.lib is properly linked)

Required Libraries: GLFW, ImGui, and RtMidi.

Build Steps:

Clone the repository:

Bash
git clone https://github.com/your-username/moshrack.git
cd moshrack
Open the Project:
Double-click the .sln file to open the project in Visual Studio 2022.

Configure the Build:
At the top of the Visual Studio window, set your build configuration to Release and your platform to x64.

Compile the Engine:
Press Ctrl + Shift + B (or go to Build > Build Solution).

The final MOSHRACK.exe application and required DLLs will output directly into the x64/Release folder.