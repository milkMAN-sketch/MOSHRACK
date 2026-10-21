#include "MoshRackGUI.h"
#include "MoshRackEngine.h"
#include <imgui.h>
#include <vector>
#include <cmath>
#include <algorithm>

// --- VIBRANT COLOR CONSTANTS (ABGR format) ---
constexpr unsigned int C_AMBER = 0xFF00BBE6; // Rich Amber
constexpr unsigned int C_CYAN = 0xFFE6E600; // Bright Cyan
constexpr unsigned int C_GREEN = 0xFF00FF55; // Electric Green
constexpr unsigned int C_PINK = 0xFFD833FF; // Hot Pink / Magenta
constexpr unsigned int C_PURPLE = 0xFFB333FF; // Deep Purple
constexpr unsigned int C_RED = 0xFF2222FF; // Neon Red
constexpr unsigned int C_BLUE = 0xFFFF5500; // Vivid Blue
constexpr unsigned int C_TEXT_DIM = 0xFFB0A595; // Clearer light-dim text

void MoshRackGUI::RenderRightPanel() {
    float targetHeight = ImGui::GetContentRegionAvail().y -20.0f ;

    BeginSectionFrame("", C_CYAN);

    ImGui::TextDisabled("PATCH BAY");

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ;

    ImGui::BeginChild("PatchBayContainer", ImVec2(0, targetHeight - 27.0f), true, flags);
    RenderPatchBay();
    ImGui::EndChild();

    EndSectionFrame();
}

void MoshRackGUI::RenderPatchBay() {
    jack_positions.clear();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 50.0f) return;

    if (engine) {
        ImGui::TextDisabled("--- AUDIO INPUT & MONITOR ---");
        ImGui::Spacing();

        ImGui::Text("Input Gain");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 10.0f);

        ImGui::SliderFloat("##audio_gain", &engine->audio_input_gain, 0.0f, 10.0f, "%.2fx");

        if (ImGui::IsItemHovered()) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                engine->audio_input_gain = (std::clamp)(engine->audio_input_gain + (wheel * 0.2f), 0.0f, 10.0f);
            }
        }

        ImGui::Spacing();
        RenderVUMeter();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    struct JackDef {
        std::string id;
        std::string label;
        unsigned int color;
        bool isSource;
    };

    static const std::vector<JackDef> sourceJacks = {
        { "SRC_BASS",     "BASS",     C_PINK,   true },
        { "SRC_MID",      "MID",      C_CYAN,   true },
        { "SRC_HI",       "HI",       C_AMBER,  true },
        { "SRC_LFO",      "LFO",      C_GREEN,  true },
        { "SRC_PITCH",    "PITCH",    C_PURPLE, true },
        { "SRC_VELOCITY", "VEL",      C_RED,    true }
    };

    static const std::vector<JackDef> targetJacks = {
        // --- MOSH (Pink) ---
        { "TGT_MOSH_GAIN",   "MOSH GAIN", 0xFFE04EE6, false },
        { "TGT_MOSH_DECAY",  "MOSH DECY", 0xFFC02CD9, false },
        { "TGT_MOSH_SIZE",   "MOSH SIZE", 0xFFE04EE6, false },
        { "TGT_MOSH_RNBW",   "MOSH RNBW", 0xFFC02CD9, false },

        // --- CHROMA (Cyan) ---
        { "TGT_CHROMA_AMT",  "CHRO DPTH", 0xFFD9D933, false },
        { "TGT_CHROMA_WARP", "CHRO WARP", 0xFFB5B51A, false },
        { "TGT_CHROMA_CRZY", "CHRO CRZY", 0xFFD9D933, false },

        // --- FRACTAL KSCOPE (Purple) ---
        { "TGT_KSCOPE_ROT",  "KSCO ROT",  0xFFBA55D3, false },
        { "TGT_KSCOPE_SEG",  "KSCO SEG",  0xFFCA77E3, false },
        { "TGT_KSCOPE_OFF",  "KSCO OFF",  0xFFBA55D3, false },
        { "TGT_KSCOPE_ZOM",  "KSCO ZOOM", 0xFFCA77E3, false },

        // --- OPTICAL FLOW (Grey) ---
        { "TGT_FLOW_AMT",    "FLOW AMT",  0xFFDCDCDC, false },
        { "TGT_FLOW_HUE",    "FLOW HUE",  0xFFB8B8B8, false },
        { "TGT_FLOW_SAT",    "FLOW SAT",  0xFFDCDCDC, false },

        // --- MESH (Green) ---
        { "TGT_MESH_SCL",    "MESH SCL",  0xFF00FF99, false },
        { "TGT_MESH_X",      "MESH X",    0xFF00CC7A, false },
        { "TGT_MESH_Y",      "MESH Y",    0xFF00995C, false },
        { "TGT_MESH_Z",      "MESH Z",    0xFF00CC7A, false },



        // --- ZOOM & ROTATION (Purple) ---
        { "TGT_ZOOM",        "ZOOM",      0xFFD666FF, false },
        { "TGT_ROTATE",      "ROTATE",    0xFFA333E6, false },
        { "TGT_ROT_X",      "ROT X",     0xFFBA55D3, false },
        { "TGT_ROT_Y",      "ROT Y",     0xFFCA77E3, false },
        { "TGT_POS_X",      "PAN X",     0xFFDCDCDC, false },
        { "TGT_POS_Y",      "PAN Y",     0xFFB8B8B8, false },

        // --- RGB GAINS (Red, Green, Cyan) ---
        { "TGT_RGB_R",       "RGB RED",   0xFF3333FF, false },
        { "TGT_RGB_G",       "RGB GRN",   0xFF00FF99, false },
        { "TGT_RGB_B",       "RGB BLU",   0xFFD9D933, false }
    };

    const float jackRadius = 16.5f;
    const float marginX = 8.0f;
    const float marginY = 10.0f;

    float usableWidth = (std::max)(avail.x - (marginX * 2.0f), 10.0f);

    int srcCols = (std::max)(1, static_cast<int>(usableWidth / 115.0f));
    float srcStepX = usableWidth / static_cast<float>(srcCols);

    int tgtCols = (std::max)(1, static_cast<int>(usableWidth / 115.0f));
    float tgtStepX = usableWidth / static_cast<float>(tgtCols);

    int srcRows = static_cast<int>((sourceJacks.size() + srcCols - 1) / srcCols);
    int tgtRows = static_cast<int>((targetJacks.size() + tgtCols - 1) / tgtCols);

    float rowHeight = (jackRadius * 2.0f) + 24.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    std::string hovered_jack_id = "";
    bool hovered_jack_is_source = false;

    auto RenderJackSocket = [&](const JackDef& jack, ImVec2 centerPos) {
        ImGui::PushID(jack.id.c_str());
        ImGui::SetCursorScreenPos(ImVec2(centerPos.x - jackRadius, centerPos.y - jackRadius));
        ImGui::InvisibleButton("##jack_btn", ImVec2(jackRadius * 2.0f, jackRadius * 2.0f));

        jack_positions[jack.id] = centerPos;

        bool isHovered = ImGui::IsItemHovered();
        if (isHovered) {
            hovered_jack_id = jack.id;
            hovered_jack_is_source = jack.isSource;
        }

        // Check if this destination jack currently has a cable patched into it
        bool isConnectedDest = !jack.isSource && (active_cables.find(jack.id) != active_cables.end());

        // Ensure sensitivity defaults to 1.0f instead of 0.0f on first lookup
        if (engine->module_sensitivities.find(jack.id) == engine->module_sensitivities.end()) {
            engine->module_sensitivities[jack.id] = 1.0f;
        }
        float& current_sens = engine->module_sensitivities[jack.id];

        // --- MOUSE WHEEL SENSITIVITY SCALING ---
        if (isHovered && isConnectedDest && ImGui::GetIO().MouseWheel != 0.0f) {
            current_sens += ImGui::GetIO().MouseWheel * 0.1f;
            current_sens = std::clamp(current_sens, 0.0f, 5.0f); // 0x to 5x range
        }

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !is_dragging_cable) {
            if (jack.isSource) {
                is_dragging_cable = true;
                drag_start_id = jack.id;
                drag_start_is_source = true;
            }
            else {
                if (isConnectedDest) {
                    is_dragging_cable = true;
                    drag_start_id = active_cables[jack.id];
                    drag_start_is_source = true;
                    active_cables.erase(jack.id);
                }
            }
        }

        if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !jack.isSource) {
            active_cables.erase(jack.id);
        }

        bool isHeld = isHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        unsigned int ringCol = isHeld ? 0xFFFFFFFF : (isHovered ? 0xFFDCF0DC : 0xFF5F5F5A);

        drawList->AddCircleFilled(centerPos, jackRadius, ringCol);
        drawList->AddCircleFilled(centerPos, jackRadius - 2.0f, 0xFF1C1919);
        drawList->AddCircleFilled(centerPos, jackRadius - 5.0f, jack.color);
        drawList->AddCircleFilled(centerPos, 4.5f, 0xFF050505);

        // --- GHOST ARC RENDER (Sensitivity Ring) ---
        // Displays while hovering a patched jack, or if the sensitivity has been altered from 1.0
        if (isConnectedDest && (isHovered || std::abs(current_sens - 1.0f) > 0.01f)) {
            float arc_radius = jackRadius + 4.0f;
            float sens_fraction = current_sens / 5.0f; // Normalized to max 5.0 range
            float start_angle = 3.14159265f * 0.75f;
            float end_angle = start_angle + (sens_fraction * (3.14159265f * 1.5f));

            drawList->PathArcTo(centerPos, arc_radius, start_angle, end_angle, 16);
            drawList->PathStroke(0xFFC864C8, false, 2.0f); // Magenta modulation ghost arc
        }

        ImVec2 textSize = ImGui::CalcTextSize(jack.label.c_str());
        ImVec2 textPos = ImVec2(centerPos.x - (textSize.x * 0.5f), centerPos.y + jackRadius + 2.0f);
        drawList->AddText(textPos, C_TEXT_DIM, jack.label.c_str());
        ImGui::PopID();
        };

    ImGui::TextDisabled("--- MODULATION SOURCES (OUTPUTS) ---");
    float currentYOffset = marginY + 25.0f;

    for (size_t i = 0; i < sourceJacks.size(); ++i) {
        int col = static_cast<int>(i % srcCols);
        int row = static_cast<int>(i / srcCols);
        float relX = marginX + (col * srcStepX) + (srcStepX * 0.5f);
        float relY = currentYOffset + (row * rowHeight);
        RenderJackSocket(sourceJacks[i], ImVec2(origin.x + relX, origin.y + relY));
    }

    currentYOffset += (srcRows * rowHeight) + 25.0f;

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + currentYOffset));
    ImGui::Separator();
    ImGui::TextDisabled("--- VFX PARAMETER INPUTS (DESTINATIONS) ---");

    currentYOffset += 40.0f;

    for (size_t i = 0; i < targetJacks.size(); ++i) {
        int col = static_cast<int>(i % tgtCols);
        int row = static_cast<int>(i / tgtCols);
        float relX = marginX + (col * tgtStepX) + (tgtStepX * 0.5f);
        float relY = currentYOffset + (row * rowHeight);
        RenderJackSocket(targetJacks[i], ImVec2(origin.x + relX, origin.y + relY));
    }

    currentYOffset += (tgtRows * rowHeight) + 15.0f;

    if (is_dragging_cable && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (!hovered_jack_id.empty() && !hovered_jack_is_source && drag_start_is_source) {
            active_cables[hovered_jack_id] = drag_start_id;
        }
        is_dragging_cable = false;
    }

    auto GetSourceIntensity = [&](const std::string& srcId) -> float {
        if (!engine) return 0.2f;
        float raw = 0.2f;
        if (srcId == "SRC_BASS")        raw = engine->audio_level_bass;
        else if (srcId == "SRC_MID")      raw = engine->audio_level_mid;
        else if (srcId == "SRC_HI")       raw = engine->audio_level_high;
        else if (srcId == "SRC_LFO")      raw = engine->audio_level_lfo;
        else if (srcId == "SRC_PITCH")    raw = engine->audio_pitch_val;
        else if (srcId == "SRC_VELOCITY") raw = engine->audio_velocity;
        return (std::min)(raw * 1.3f, 1.5f);
        };

    auto GetRandomCableColor = [&](const std::string& src, const std::string& tgt) -> unsigned int {
        unsigned int seed = 0;
        for (char c : src + tgt) seed = seed * 33 + c;
        int r = 120 + (seed % 135);
        int g = 120 + ((seed >> 5) % 135);
        int b = 120 + ((seed >> 10) % 135);
        return 0xFF000000 | (b << 16) | (g << 8) | r;
        };

    float time = (float)ImGui::GetTime();

    for (const auto& pair : active_cables) {
        const std::string& targetId = pair.first;
        const std::string& sourceId = pair.second;

        if (jack_positions.count(sourceId) && jack_positions.count(targetId)) {
            ImVec2 p1 = jack_positions[sourceId];
            ImVec2 p4 = jack_positions[targetId];

            float intensity = GetSourceIntensity(sourceId);
            float wireWidth = 3.5f + (intensity * 5.0f);

            float dx = p4.x - p1.x;
            float dy = p4.y - p1.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 1.0f) dist = 1.0f;

            ImVec2 p2, p3;

            // Determine if the cable is predominantly vertical
            bool isVertical = std::abs(dy) > std::abs(dx) * 1.2f;

            if (isVertical) {
                // --- VERTICAL CABLES: Sideways 2D Arch with Randomized Side ---
                float nx = -dy / dist;
                float ny = dx / dist;

                float wobble = std::sin(time * 8.0f + p1.x * 0.03f) * (intensity * 10.0f);
                float arch = 25.0f + (dist * 0.18f) + wobble;

                // Stable pseudo-random side selection (-1.0 or 1.0) based on connection ID
                std::size_t hashVal = std::hash<std::string>{}(sourceId + "_" + targetId);
                float sideSign = (hashVal % 2 == 0) ? 1.0f : -1.0f;
                arch *= sideSign;

                p2 = ImVec2(p1.x + dx * 0.33f + nx * arch, p1.y + dy * 0.33f + ny * arch);
                p3 = ImVec2(p1.x + dx * 0.66f + nx * arch, p1.y + dy * 0.66f + ny * arch);
            }
            else {
                // --- HORIZONTAL / DIAGONAL CABLES: Previous Gravity Sag Physics ---
                float verticalBias = (std::abs(dy) > std::abs(dx)) ? (dy * 0.15f) : 0.0f;
                float wobble1 = std::sin(time * 8.0f + p1.x * 0.03f) * (intensity * 14.0f);
                float wobble2 = std::cos(time * 13.0f + p1.y * 0.04f) * (intensity * 6.0f);
                float sag = 50.0f + (dist * 0.2f) + wobble1 + wobble2;

                p2 = ImVec2(p1.x + verticalBias, p1.y + sag);
                p3 = ImVec2(p4.x - verticalBias, p4.y + sag);
            }

            // --- 3D TUBING RENDERING PASSES (Shared for all cables) ---

            // 1. Soft Drop Shadow (offset down-right)
            ImVec2 shadowOffset(2.0f, 3.5f);
            drawList->AddBezierCubic(
                ImVec2(p1.x + shadowOffset.x, p1.y + shadowOffset.y),
                ImVec2(p2.x + shadowOffset.x, p2.y + shadowOffset.y),
                ImVec2(p3.x + shadowOffset.x, p3.y + shadowOffset.y),
                ImVec2(p4.x + shadowOffset.x, p4.y + shadowOffset.y),
                IM_COL32(0, 0, 0, 170), wireWidth + 4.0f
            );

            // 2. Main Cable Body (Rich colored core)
            unsigned int wireColor = GetRandomCableColor(sourceId, targetId);
            drawList->AddBezierCubic(p1, p2, p3, p4, wireColor, wireWidth);

            // 3. Glossy Specular Highlight (offset up-left for 3D tubing sheen)
            ImVec2 highOffset(-1.0f, -1.0f);
            drawList->AddBezierCubic(
                ImVec2(p1.x + highOffset.x, p1.y + highOffset.y),
                ImVec2(p2.x + highOffset.x, p2.y + highOffset.y),
                ImVec2(p3.x + highOffset.x, p3.y + highOffset.y),
                ImVec2(p4.x + highOffset.x, p4.y + highOffset.y),
                IM_COL32(255, 255, 255, 110), wireWidth * 0.32f
            );
        }
    }

    if (is_dragging_cable) {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            is_dragging_cable = false;
        }
        else if (jack_positions.count(drag_start_id)) {
            ImVec2 p1 = jack_positions[drag_start_id];
            ImVec2 p4 = ImGui::GetMousePos();

            float dragIntensity = GetSourceIntensity(drag_start_id);
            float wireWidth = 3.5f + (dragIntensity * 4.0f);

            float sag = 50.0f + (std::abs(p4.x - p1.x) * 0.25f);
            ImVec2 p2 = ImVec2(p1.x, p1.y + sag);
            ImVec2 p3 = ImVec2(p4.x, p4.y + sag);

            drawList->AddBezierCubic(p1, p2, p3, p4, 0x88DDDDDD, wireWidth);
        }

    }
    // Sync the frontend active cables directly to the engine's backend patch matrix
    if (engine) {
        engine->patch_matrix = active_cables;
    }

    // --- MINI MONITOR (Auto-scaling & CRT Shutdown) ---
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + currentYOffset));
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("MINI MONITOR (Right-Click: CRT Power)");

    float remainingHeight = ImGui::GetContentRegionAvail().y - 10.0f;
    if (remainingHeight < 40.0f) remainingHeight = 40.0f;

    float monitorW = avail.x;
    float monitorH = monitorW * 0.5625f;

    if (monitorH > remainingHeight) {
        monitorH = remainingHeight;
        monitorW = monitorH * (16.0f / 9.0f);
    }

    float xOffset = (avail.x - monitorW) * 0.5f;
    if (xOffset > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset);
    }

    static bool s_monitor_on = true;
    static float s_anim_progress = 0.0f; // 0.0 (ON) -> 1.0 (OFF)
    static bool s_is_animating = false;

    ImVec2 miniSize(monitorW, monitorH);

    float dt = ImGui::GetIO().DeltaTime;
    if (s_is_animating) {
        float speed = 4.5f; // Fast, punchy CRT collapse
        if (!s_monitor_on) {
            s_anim_progress += dt * speed;
            if (s_anim_progress >= 1.0f) {
                s_anim_progress = 1.0f;
                s_is_animating = false;
            }
        }
        else {
            s_anim_progress -= dt * speed;
            if (s_anim_progress <= 0.0f) {
                s_anim_progress = 0.0f;
                s_is_animating = false;
            }
        }
    }

    // Zero out padding so the video and vector canvas fill the child window exactly
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("MiniMonitorChild", miniSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 rectMin = ImGui::GetWindowPos();
    ImVec2 rectMax = ImVec2(rectMin.x + monitorW, rectMin.y + monitorH);
    ImVec2 center = ImVec2(rectMin.x + monitorW * 0.5f, rectMin.y + monitorH * 0.5f);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. UNIFIED CLICK INTERACTION (CRT Power Toggle)
    if (ImGui::IsWindowHovered()) {
        // RIGHT-CLICK: Toggles CRT Power Shutdown / Startup Animation
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            s_monitor_on = !s_monitor_on;
            s_is_animating = true;
        }
    }

    // 2. LAYER 1: VIDEO FEED (Bottom)
    if (s_anim_progress < 1.0f) {
        RenderOutputFeed(ImGui::GetContentRegionAvail());
    }

    // 3. LAYER 2: CRT SHUTDOWN & STARTUP ANIMATION OVERLAY
    const ImU32 colBlack = IM_COL32(0, 0, 0, 255);

    if (s_anim_progress >= 1.0f) {
        // Fully OFF: Pure black screen
        dl->AddRectFilled(rectMin, rectMax, colBlack);
    }
    else if (s_anim_progress > 0.0f) {
        if (s_anim_progress <= 0.65f) {
            // PHASE 1: Vertical squeeze into a central horizontal beam line
            float t = s_anim_progress / 0.65f; // 0.0 to 1.0
            float visibleH = monitorH * (1.0f - t);
            float halfH = visibleH * 0.5f;

            float topY = center.y - halfH;
            float bottomY = center.y + halfH;

            // Black out regions outside the collapsing video slit
            if (topY > rectMin.y) {
                dl->AddRectFilled(rectMin, ImVec2(rectMax.x, topY), colBlack);
            }
            if (bottomY < rectMax.y) {
                dl->AddRectFilled(ImVec2(rectMin.x, bottomY), rectMax, colBlack);
            }

            // Collapsing edge phosphor lines (cyan glow tracking slit boundaries)
            float edgeAlpha = (1.0f - t) * 0.4f;
            ImU32 edgeColor = IM_COL32(180, 240, 255, (int)(edgeAlpha * 255.0f));
            dl->AddLine(ImVec2(rectMin.x, topY), ImVec2(rectMax.x, topY), edgeColor, 1.0f);
            dl->AddLine(ImVec2(rectMin.x, bottomY), ImVec2(rectMax.x, bottomY), edgeColor, 1.0f);

            // Central horizontal beam (intensifies non-linearly)
            float beamIntensity = t * t;
            int glowAlpha = (int)(beamIntensity * 180.0f);
            int coreAlpha = (int)(beamIntensity * 255.0f);

            if (glowAlpha > 0) {
                float glowHalfH = 1.0f + (t * 2.0f);
                ImU32 beamGlow = IM_COL32(160, 230, 255, glowAlpha);
                ImU32 beamCore = IM_COL32(255, 255, 255, coreAlpha);

                dl->AddRectFilled(ImVec2(rectMin.x, center.y - glowHalfH), ImVec2(rectMax.x, center.y + glowHalfH), beamGlow);
                dl->AddRectFilled(ImVec2(rectMin.x, center.y - 0.75f), ImVec2(rectMax.x, center.y + 0.75f), beamCore);
            }
        }
        else if (s_anim_progress <= 0.90f) {
            // PHASE 2: Horizontal collapse from full-width center line down to a dot
            float t = (s_anim_progress - 0.65f) / 0.25f; // 0.0 to 1.0
            float visibleW = monitorW * (1.0f - t);
            float halfW = (std::max)(visibleW * 0.5f, 2.0f);

            float leftX = center.x - halfW;
            float rightX = center.x + halfW;

            dl->AddRectFilled(rectMin, rectMax, colBlack);

            ImU32 glowCol = IM_COL32(120, 220, 255, 220);
            ImU32 coreCol = IM_COL32(255, 255, 255, 255);

            dl->AddRectFilled(ImVec2(leftX, center.y - 2.5f), ImVec2(rightX, center.y + 2.5f), glowCol);
            dl->AddRectFilled(ImVec2(leftX, center.y - 1.0f), ImVec2(rightX, center.y + 1.0f), coreCol);
        }
        else {
            // PHASE 3: Center phosphor dot decay down to black
            float t = (s_anim_progress - 0.90f) / 0.10f; // 0.0 to 1.0
            float alpha = 1.0f - t;
            float radius = 3.5f * (1.0f - (t * 0.5f));

            dl->AddRectFilled(rectMin, rectMax, colBlack);

            ImU32 dotGlow = IM_COL32(140, 220, 255, (int)(alpha * 150.0f));
            ImU32 dotCore = IM_COL32(255, 255, 255, (int)(alpha * 255.0f));

            dl->AddCircleFilled(center, radius + 2.0f, dotGlow);
            dl->AddCircleFilled(center, radius, dotCore);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}