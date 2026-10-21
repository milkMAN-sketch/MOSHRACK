#include "MoshRackGUI.h"
#include "MoshRackEngine.h"
#include <imgui.h>

constexpr unsigned int C_AMBER = IM_COL32(255, 170, 0, 255); // Warm Amber
constexpr unsigned int C_CYAN = IM_COL32(0, 204, 204, 255); // Cyan
constexpr unsigned int C_GREEN = IM_COL32(102, 255, 0, 255); // Neon Green
constexpr unsigned int C_PINK = IM_COL32(255, 68, 170, 255); // Hot Pink
constexpr unsigned int C_PURPLE = IM_COL32(170, 68, 255, 255); // Purple
constexpr unsigned int C_RED = IM_COL32(255, 51, 51, 255); // Red
constexpr unsigned int C_TEXT = IM_COL32(154, 212, 200, 255); // Pale Sage/Periwinkle
constexpr unsigned int C_TEXT_DIM = IM_COL32(96, 112, 122, 255); // Slate Gray

void MoshRackGUI::RenderLFOSection() {
    BeginSectionFrame("1. SINE LFO GENERATOR", C_GREEN);

    // Directly binds to the engine map. Clicking it flips fx_enabled["LFO"] automatically.
    LEDToggle("LFO ON", &engine->fx_enabled["LFO"], C_GREEN, C_GREEN, 80, 30, "LFO_TOGGLE");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Enable or disable low-frequency modulation output");
    }

    ImGui::SameLine();


    // --- RATE KNOB ---
    CircularKnob("RATE", &engine->lfo_rate, 0.1f, 5.0f, 1.0f, C_GREEN, false, "Rate in Hz", "LFO_RATE_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("LFO Oscillation Speed (0.1 Hz - 5.0 Hz)");
    }

    ImGui::SameLine();

    // --- GAIN KNOB ---
    CircularKnob("GAIN", &engine->lfo_gain, 0.0f, 1.0f, 0.8f, C_GREEN, false, "LFO Amplitude", "LFO_GAIN_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Modulation depth / wave amplitude (0% to 100%)");
    }

    ImGui::SameLine();

    // --- BIAS KNOB ---
    CircularKnob("BIAS", &engine->lfo_bias, -0.5f, 0.5f, 0.0f, C_GREEN, true, "Center Bias", "LFO_BIAS_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("DC Offset: shifts waveform center point (-0.5 to +0.5)");
    }

    ImGui::SameLine();

    // --- TAP TEMPO BUTTON ---
    if (ImGui::Button("TAP", ImVec2(45, 30))) {
        engine->tap_tempo();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Tap repeatedly to match LFO speed to manual tempo");
    }

    ImGui::SameLine();

    // --- BEATSYNC TOGGLE BUTTON ---
    bool sync_state = engine->lfo_beat_sync_active.load();
    if (ImGui::Button(sync_state ? "SYNC..." : "SYNC", ImVec2(55, 30))) {
        if (!sync_state) {
            std::lock_guard<std::mutex> lock(engine->sync_mutex);
            engine->sync_beat_times.clear();
            engine->lfo_status_text = "Listening (0/16)...";
            engine->lfo_beat_sync_active.store(true);
        }
        else {
            engine->lfo_beat_sync_active.store(false);
            engine->lfo_status_text = "Idle";
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Auto-detect BPM from incoming audio peaks over 16 beats");
    }

    ImGui::SameLine();

    // --- DIGITAL BPM READOUT & STATUS ---
    float bpm = engine->lfo_rate * 60.0f;

    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::TextColored(ImVec4(0.3f, 0.3f, 0.3f, 1.0f), "%s", engine->lfo_status_text.c_str());
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(C_GREEN), "%5.1f BPM", bpm);
    ImGui::EndGroup();

    EndSectionFrame();
}

void MoshRackGUI::RenderMoshSection() {
    BeginSectionFrame("2. MOSH EFFECT", C_PINK);

    // --- MOSH ENABLE TOGGLE ---
    bool is_enabled = engine->fx_enabled["MOSH"];

    // Pass the local 'is_enabled' variable to the toggle
    if (LEDToggle("MOSH ON", &is_enabled, C_PINK, C_PINK, 80, 30, "MOSH_TOGGLE")) {
        // If clicked, update the engine map with the NEW toggled state
        engine->fx_enabled["MOSH"] = is_enabled;
    }

    ImGui::SameLine();

    // --- MOTION / DECAY KNOB ---
    CircularKnob("Motion", &engine->current_fx_gains["MOSH_DECAY"], 0.0f, 1.0f, 0.0f, C_PINK, false, "Decay Rate", "MOSH_DECAY");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Motion threshold gate: filters out subtle movements so only fast/large motion triggers the smear");
    }

    ImGui::SameLine();

    // --- MOSH INTENSITY KNOB ---
    CircularKnob("MOSH!! ", &engine->current_fx_gains["MOSH"], 0.0f, 1.0f, 0.0f, C_PINK, false, "Mosh Intensity", "MOSH");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Datamosh displacement strength and artifact intensity");
    }

    ImGui::SameLine();

    // --- MOSH SIZE KNOB ---
    CircularKnob("SIZE", &engine->current_fx_gains["MOSH_SIZE"], 0.0f, 1.0f, 0.0f, C_PINK, false, "Pixel Block Size", "MOSH_SIZE");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Scales the resolution of the motion vectors (fine liquid to chunky blocks)");
    }

    ImGui::SameLine();

    // --- RAINBOW HSV MIX KNOB ---
    CircularKnob("RNBW!!", &engine->current_fx_gains["RAINBOW"], 0.0f, 1.0f, 0.0f, C_PURPLE, false, "Rainbow HSV Mix", "RAINBOW");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Smoothly fades in the chromatic rainbow hue-shifting effect");
    }

    EndSectionFrame();
}

void MoshRackGUI::RenderFlowSection() {
    BeginSectionFrame("3. OPTICAL FLOW", C_TEXT);

    bool flow_on = engine->fx_enabled["FLOW"];
    if (LEDToggle("FLOW", &flow_on, C_TEXT, C_TEXT_DIM, 80, 30, "FLOW_TOGGLE")) {
        engine->fx_enabled["FLOW"] = flow_on;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Enable or disable liquid distortion processing");
    }

    ImGui::SameLine();

    CircularKnob("FLOW!!", &engine->current_fx_gains["FLOW"], 0.0f, 1.0f, 0.0f, C_TEXT, false, "Distortion Amount", "FLOW_AMT_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Liquid surface distortion and warp intensity");
    }

    ImGui::SameLine();

    CircularKnob("STRTN", &engine->current_fx_gains["FLOW_SAT"], 0.0f, 1.0f, 0.0f, C_TEXT, false, "Gloss Intensity", "FLOW_SAT_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Saturation / Base color boost");
    }

    ImGui::SameLine();

    CircularKnob("WET", &engine->current_fx_gains["FLOW_WET"], 0.0f, 1.0f, 0.0f, C_TEXT, false, "Wet Gloss Layer", "FLOW_WET_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Adds reflective water gloss to bright luminance regions");
    }

    EndSectionFrame();
}

void MoshRackGUI::RenderKscopeSection() {
    BeginSectionFrame("4. KSCOPE", C_PURPLE);

    // --- KSCOPE ENABLE TOGGLE ---
    bool kscope_on = engine->fx_enabled["KSCOPE"];
    if (LEDToggle("KSCOPE", &kscope_on, C_PURPLE, C_PURPLE, 80, 30, "KSCOPE_TOGGLE")) {
        engine->fx_enabled["KSCOPE"] = kscope_on;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Enable or disable the fractal kaleidoscope mirror tunnel");
    }

    ImGui::SameLine();

    // --- K_ROT KNOB (Bipolar Rotation Speed & Direction) ---
    CircularKnob("K_ROT", &engine->current_fx_gains["KSCOPE_ROT"], -10.0f, 10.0f, 0.0f, C_PURPLE, true, "Rotation Speed", "KSCOPE_ROT");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Bipolar rotation speed and direction (-10.0 to +10.0)");
    }

    ImGui::SameLine();

    // --- SEGMENTS KNOB (NEW: Stepping from 0, 2, 4, 6...) ---
    CircularKnob("SEGMENTS", &engine->current_fx_gains["KSCOPE_SEGMENTS"], 0.0f, 12.0f, 6.0f, C_PURPLE, false, "Kaleidoscope Segments", "KSCOPE_SEGMENTS");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Number of mirror segments (0 = normal video, 2, 4, 6...)");
    }

    ImGui::SameLine();

    // --- SEG_OFFSET KNOB (Phase-Shift / Twisting Slices) ---
    CircularKnob("OFFSET", &engine->current_fx_gains["KSCOPE_OFFSET"], 0.0f, 1.0f, 0.0f, C_PURPLE, false, "Segment Phase Offset", "KSCOPE_OFFSET");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Phase-shift alternate mirror segments to twist geometry and warp shapes");
    }

    ImGui::SameLine();

    // --- TUNNEL / FRACTAL ZOOM KNOB (Recursive Feedback) ---
    CircularKnob("TUNNEL", &engine->current_fx_gains["KSCOPE_ZOOM"], 0.5f, 2.0f, 1.0f, C_PURPLE, false, "Fractal Tunnel Zoom", "KSCOPE_ZOOM");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Recursive feedback zoom for an infinite fractal mirror tunnel effect");
    }

    EndSectionFrame();
}

void MoshRackGUI::RenderChromaSection() {
    BeginSectionFrame("5. CHROMA", C_CYAN);

    // --- CHROMA ENABLE TOGGLE ---
    bool chroma_on = engine->fx_enabled["CHROMA"];
    if (LEDToggle("CHROMA", &chroma_on, C_CYAN, C_CYAN, 80, 30, "CHROMA_TOGGLE")) {
        engine->fx_enabled["CHROMA"] = chroma_on;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Enable or disable chromatic aberration and warp effects");
    }

    ImGui::SameLine();

    // --- KNOB 1: DISPERSION DEPTH ---
    CircularKnob("DEPTH", &engine->current_fx_gains["CHROMA"], -0.5f, 0.5f, 0.0f, C_CYAN, true, "RGB Dispersion", "CHROMA_DEPTH_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Standard RGB split. Mild in the first half, MASSIVE spread in the second half.");
    }

    ImGui::SameLine();

    // --- KNOB 2: WARP SPEED ---
    CircularKnob("WARP SPD", &engine->current_fx_gains["CHROMA_WARP"], 0.0f, 4.0f, 0.0f, C_CYAN, false, "Rainbow Scroll Speed", "CHROMA_WARP_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Scrolls the entire frame through a full rainbow gradient. 0 to STROBE speed.");
    }

    ImGui::SameLine();

    // --- KNOB 3: CRAZY ---
    CircularKnob("CRAZY", &engine->current_fx_gains["CHROMA_CRAZY"], -1.0f, 1.0f, 0.0f, C_CYAN, true, "Neon Tri-Color Split", "CHROMA_CRAZY_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Splits custom Cyan, Green, and Pink neon ghosts. Mild to MASSIVE spread.");
    }

    EndSectionFrame();
}

void MoshRackGUI::RenderMeshSection() {
    BeginSectionFrame("6.  MESH", C_PURPLE);

    // --- ROW 1: ENABLE TOGGLE & KNOBS ---
    bool mesh_on = engine->fx_enabled["MESH"];
    if (LEDToggle("MESH", &mesh_on, C_PURPLE, C_PURPLE, 80, 30, "MESH_TOGGLE")) {
        engine->fx_enabled["MESH"] = mesh_on;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Enable or disable real-time scan processor mesh deformation");
    }

    ImGui::SameLine();

    // --- RESOLUTION / SCALE KNOB ---
    CircularKnob("SCALE", &engine->current_fx_gains["MESH_SCALE"], 2.0f, 100.0f, 25.0f, C_PURPLE, false, "Mesh Grid Resolution", "MESH_SCALE_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Adjusts the vertex density and polygon resolution of the mesh");
    }

    ImGui::SameLine();

    // --- X OSCILLATOR KNOB (Bipolar, centered at 0) ---
    CircularKnob("X OSC", &engine->current_fx_gains["MESH_H"], -5.0f, 5.0f, 0.0f, C_PURPLE, true, "X Chaos Amplitude", "MESH_H_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Modulates chaotic horizontal oscillator displacement (bipolar)");
    }
    ImGui::SameLine();

    // --- Y OSCILLATOR KNOB (Bipolar, centered at 0) ---
    CircularKnob("Y OSC", &engine->current_fx_gains["MESH_V"], -5.0f, 5.0f, 0.0f, C_PURPLE, true, "Y Chaos Amplitude", "MESH_V_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Modulates chaotic vertical oscillator displacement (bipolar)");
    }
    ImGui::SameLine();

    // --- Z OSCILLATOR KNOB ---
    CircularKnob("Z OSC", &engine->current_fx_gains["MESH_Z"], 0.0f, 5.0f, 1.0f, C_PURPLE, false, "Z Chaos Amplitude", "MESH_Z_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Modulates chaotic depth and z-axis noise displacement");
    }

    ImGui::Spacing();

    // --- ROW 2: TOPOLOGY TOGGLES & SUB-SWITCHES ---
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), "Topology:");
    ImGui::SameLine();

    bool is_tri = engine->mesh_triangles;
    bool is_horiz = engine->mesh_horiz;
    bool is_vert = engine->mesh_vert;

    if (is_tri) ImGui::PushStyleColor(ImGuiCol_Button, C_PURPLE);
    if (ImGui::Button("Triangles")) {
        engine->mesh_triangles = true;
        engine->mesh_horiz = false;
        engine->mesh_vert = false;
        engine->lastGridSize = -1;
    }
    if (is_tri) ImGui::PopStyleColor();

    ImGui::SameLine();

    if (is_horiz) ImGui::PushStyleColor(ImGuiCol_Button, C_PURPLE);
    if (ImGui::Button("Horiz Lines")) {
        engine->mesh_horiz = !engine->mesh_horiz;
        engine->mesh_triangles = false;
        engine->lastGridSize = -1;
    }
    if (is_horiz) ImGui::PopStyleColor();

    ImGui::SameLine();

    if (is_vert) ImGui::PushStyleColor(ImGuiCol_Button, C_PURPLE);
    if (ImGui::Button("Vert Lines")) {
        engine->mesh_vert = !engine->mesh_vert;
        engine->mesh_triangles = false;
        engine->lastGridSize = -1;
    }
    if (is_vert) ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 20.0f);

    bool bw_on = engine->fx_enabled["MESH_BW"];
    if (ImGui::Checkbox("B&W", &bw_on)) { engine->fx_enabled["MESH_BW"] = bw_on; }
    ImGui::SameLine();

    bool inv_on = engine->fx_enabled["MESH_INVERT"];
    if (ImGui::Checkbox("Invert", &inv_on)) { engine->fx_enabled["MESH_INVERT"] = inv_on; }
    ImGui::SameLine();

    bool luma_on = engine->fx_enabled["MESH_LUMA"];
    if (ImGui::Checkbox("Luma Key", &luma_on)) {
        engine->fx_enabled["MESH_LUMA"] = luma_on;
    }

    if (luma_on) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::SliderFloat("##LumaVal", &engine->current_fx_gains["MESH_LUMA_VAL"], 0.0f, 1.0f, "Thr: %.2f");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("Adjust luminance threshold");
        }
    }

    EndSectionFrame();
}

void MoshRackGUI::RenderZoomSection() {
    if (!engine) {
        return;
    }

    BeginSectionFrame("7. ZOOM & 3D SPACE", C_PURPLE);

    bool zoom_on = engine->fx_enabled["ZOOM"];
    if (LEDToggle("ZOOM", &zoom_on, C_PURPLE, C_PURPLE, 80, 28, "ZOOM_TOGGLE")) {
        engine->fx_enabled["ZOOM"] = zoom_on;
        engine->fx_enabled["TRANSFORM_3D"] = zoom_on;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Enable or disable 3D transform and zoom");
    }

    ImGui::SameLine();

    // Disabled blocks removed so knobs can be staged while bypassed
    CircularKnob("ZOOM##knob", &engine->current_fx_gains["ZOOM"], -1.0f, 1.0f, 0.0f, C_PURPLE, true, "Zoom Scale", "ZOOM_KNOB");
    ImGui::SameLine();

    CircularKnob("ROT X", &engine->current_fx_gains["ROT_X"], -90.0f, 90.0f, 0.0f, C_PURPLE, true, "X-Axis Tilt (±90°)", "ROT_X_KNOB");
    ImGui::SameLine();

    CircularKnob("ROT Y", &engine->current_fx_gains["ROT_Y"], -90.0f, 90.0f, 0.0f, C_PURPLE, true, "Y-Axis Yaw (±90°)", "ROT_Y_KNOB");
    ImGui::SameLine();

    EndSectionFrame();
}

void MoshRackGUI::RenderRGBSection() {
    BeginSectionFrame("8. RGB GAIN & KEY", C_RED);

    bool rgb_on = engine->fx_enabled["RGB"];
    if (LEDToggle("RGB", &rgb_on, C_RED, C_RED, 50, 28, "RGB_TOGGLE")) {
        engine->fx_enabled["RGB"] = rgb_on;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Enable or disable RGB bipolar color balancing");
    }

    ImGui::SameLine();

    CircularKnob("R", &engine->rgb_gains[0], -1.0f, 1.0f, 0.0f, C_RED, true, "Red Bipolar Gain", "RGB_RED_KNOB");
    ImGui::SameLine();

    CircularKnob("G", &engine->rgb_gains[1], -1.0f, 1.0f, 0.0f, C_GREEN, true, "Green Bipolar Gain", "RGB_GREEN_KNOB");
    ImGui::SameLine();

    CircularKnob("B", &engine->rgb_gains[2], -1.0f, 1.0f, 0.0f, C_CYAN, true, "Blue Bipolar Gain", "RGB_BLUE_KNOB");
    ImGui::SameLine();

    bool luma_on = engine->fx_enabled["LUMA_KEY"];
    if (LEDToggle("LUMA", &luma_on, C_RED, C_RED, 55, 28, "LUMA_KEY_TOGGLE")) {
        engine->fx_enabled["LUMA_KEY"] = luma_on;
        if (luma_on) engine->fx_enabled["CHROMA_KEY"] = false;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Enable Luma (brightness) keying");
    }

    ImGui::SameLine();

    bool chroma_on = engine->fx_enabled["CHROMA_KEY"];
    if (LEDToggle("CHROMA", &chroma_on, C_RED, C_RED, 65, 28, "CHROMA_KEY_TOGGLE")) {
        engine->fx_enabled["CHROMA_KEY"] = chroma_on;
        if (chroma_on) engine->fx_enabled["LUMA_KEY"] = false;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Enable Chroma (color/greenscreen) keying");
    }

    ImGui::SameLine();

    CircularKnob("THRESH", &engine->key_amount, 0.0f, 1.0f, 0.0f, C_RED, false, "Key Threshold", "KEY_THRESH_KNOB");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::SetTooltip("Adjusts the cutoff threshold for the active key");
    }

    EndSectionFrame();
}












