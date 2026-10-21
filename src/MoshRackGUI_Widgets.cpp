#include "MoshRackGUI.h"
#include "MoshRackEngine.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// --- COLOR CONSTANTS ---
constexpr unsigned int C_AMBER = 0xFF00CC55;
constexpr unsigned int C_CYAN = 0xFFCCCC00;
constexpr unsigned int C_GREEN = 0xFF00FF66;
constexpr unsigned int C_PINK = 0xFFEE4488;
constexpr unsigned int C_PURPLE = 0xFFAA44FF;
constexpr unsigned int C_RED = 0xFFFF3333;
constexpr unsigned int C_TEXT = 0xFFC8D49A;
constexpr unsigned int C_TEXT_DIM = 0xFF7A7060;
constexpr unsigned int C_BEVEL_SH = 0xFF0E0E14;
constexpr unsigned int C_BEVEL_HI = 0xFF4A3A3A;
constexpr unsigned int C_GROOVE = 0xFF0D0D10;
constexpr unsigned int C_PANEL = 0xFF181412;
bool MoshRackGUI::CircularKnob(const char* label, float* value, float min_val, float max_val,
    float default_val, unsigned int color, bool bipolar,
    const char* tooltip, const char* param_name) {

    // --- BAKE THE LOCK DIRECTLY INTO THE WIDGET ---
    std::lock_guard<std::mutex> lock(engine->engine_mutex);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float size = 50.0f;

    ImGui::InvisibleButton(label, ImVec2(size, size + 28));
    bool is_active = ImGui::IsItemActive();
    bool is_hovered = ImGui::IsItemHovered();

    if (midi_mapping_mode) {
        if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            engine->is_learning_midi = true;
            std::string cleanName = (param_name != nullptr && param_name[0] != '\0') ? param_name : label;
            size_t hashPos = cleanName.find("##");
            if (hashPos != std::string::npos) cleanName = cleanName.substr(0, hashPos);
            engine->param_to_learn = cleanName;
        }
    }
    else {
        if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            // Check for Shift key during mouse drag as well if you want fine-tuning there too!
            float drag_scale = ImGui::GetIO().KeyShift ? 0.25f : 1.0f;
            float delta = -ImGui::GetIO().MouseDelta.y;
            float range = max_val - min_val;
            *value = std::clamp(*value + delta * (range / 200.0f) * drag_scale, min_val, max_val);
        }
        if (is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            *value = default_val;
        }
    }

    // --- MOUSE WHEEL WITH SHIFT FINE-TUNING ---
    if (is_hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            float range = max_val - min_val;

            // Scale down step aggressively if Shift is held down
            bool isFineTuning = ImGui::GetIO().KeyShift;
            float stepMultiplier = isFineTuning ? 0.0005f : 0.005f;

            float step = range * stepMultiplier;
            *value += wheel * step;
            *value = std::clamp(*value, min_val, max_val);
        }
    }

    float norm = (*value - min_val) / (max_val - min_val);
    float angle = 225.0f - norm * 270.0f;
    float rad = angle * M_PI / 180.0f;

    float cx = p.x + size * 0.5f;
    float cy = p.y + size * 0.5f;
    float r = size * 0.5f - 4.0f;
    float ri = r - 6.0f;

    draw->AddCircle(ImVec2(cx, cy), r + 2, C_BEVEL_SH, 32, 3.0f);
    draw->AddCircleFilled(ImVec2(cx, cy), r, C_BEVEL_SH);
    draw->AddCircleFilled(ImVec2(cx, cy), ri, 0xFF1C1C24);

    float px = cx + cos(rad) * (ri - 3.0f);
    float py = cy - sin(rad) * (ri - 3.0f);
    draw->AddLine(ImVec2(cx, cy), ImVec2(px, py), color, 2.0f);
    draw->AddCircleFilled(ImVec2(px, py), 2.0f, color);

    char val_buf[16];
    if (bipolar) snprintf(val_buf, sizeof(val_buf), "%+.2f", *value);
    else snprintf(val_buf, sizeof(val_buf), "%.2f", *value);

    draw->AddText(ImVec2(cx - 14, p.y + size + 2), color, val_buf);
    draw->AddText(ImVec2(cx - 14, p.y + size + 14), C_TEXT_DIM, label);

    return is_active;
}

bool MoshRackGUI::LEDToggle(const char* label, bool* state,
    unsigned int on_color, unsigned int off_color,
    float width, float height,
    const char* param_name) {
    
    // --- BAKE THE LOCK IN HERE TOO ---
    std::lock_guard<std::mutex> lock(engine->engine_mutex);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    bool clicked = ImGui::InvisibleButton(label, ImVec2(width, height));
    
    if (clicked) {
        if (midi_mapping_mode) {
            engine->is_learning_midi = true;
            std::string cleanName = (param_name != nullptr && param_name[0] != '\0') ? param_name : label;
            size_t hashPos = cleanName.find("##");
            if (hashPos != std::string::npos) cleanName = cleanName.substr(0, hashPos);
            engine->param_to_learn = cleanName;
        }
        else {
            *state = !(*state);
        }
    }
    
 


    // --- BEVEL & CONTAINER BACKGROUND ---
    unsigned int bg_col = *state ? 0xFF1C1C28 : 0xFF0E0E14;
    draw->AddRectFilled(p, ImVec2(p.x + width, p.y + height), C_BEVEL_SH);
    draw->AddRectFilled(ImVec2(p.x + 1, p.y + 1), ImVec2(p.x + width - 1, p.y + height - 1), C_BEVEL_HI);
    draw->AddRectFilled(ImVec2(p.x + 2, p.y + 2), ImVec2(p.x + width - 2, p.y + height - 2), bg_col);

    // --- REALISTIC GREEN LED WITH WEAK HAZE ---
    ImVec2 ledCenter = ImVec2(p.x + 12.0f, p.y + height * 0.5f);

    if (*state) {
        // Layer 1: Outer soft haze/glow (Radius 8.0px, ~15% alpha)
        draw->AddCircleFilled(ledCenter, 8.0f, IM_COL32(50, 255, 110, 38));

        // Layer 2: Inner concentrated glow (Radius 5.5px, ~35% alpha)
        draw->AddCircleFilled(ledCenter, 5.5f, IM_COL32(60, 255, 120, 90));

        // Layer 3: Main illuminated LED lens (Radius 3.5px, Solid Green)
        draw->AddCircleFilled(ledCenter, 3.5f, IM_COL32(60, 255, 110, 255));

        // Layer 4: Intense incandescent core spot (Radius 1.2px, Near White)
        draw->AddCircleFilled(ledCenter, 1.2f, IM_COL32(235, 255, 240, 240));
    }
    else {
        // Unlit State: Dark desaturated green glass bulb with subtle rim
        draw->AddCircleFilled(ledCenter, 3.5f, IM_COL32(14, 42, 22, 255));
        draw->AddCircle(ledCenter, 3.5f, IM_COL32(8, 24, 12, 180), 0, 1.0f);
    }

    // --- LABEL TEXT ---
    const char* text_end = ImGui::FindRenderedTextEnd(label);
    draw->AddText(ImVec2(p.x + 24.0f, p.y + height * 0.5f - 6.0f), *state ? C_TEXT : C_TEXT_DIM, label, text_end);

    return clicked;
}

void MoshRackGUI::BeginSectionFrame(const char* title, unsigned int accent_color) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;

    bool hasTitle = (title && title[0] != '\0' && title[0] != ' ');

    // Only draw the accent line if there is a valid title
    if (hasTitle) {
        draw->AddRectFilled(p, ImVec2(p.x + w, p.y + 3), accent_color);
        ImGui::Dummy(ImVec2(0, 3));
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, C_PANEL);

    const char* child_id = hasTitle ? title : "UnnamedSectionChild";
    ImGui::BeginChild(child_id, ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    // Only draw the text if a valid title exists
    if (hasTitle) {
        ImGui::TextColored(ImVec4(
            ((accent_color >> 0) & 0xff) / 255.0f,
            ((accent_color >> 8) & 0xff) / 255.0f,
            ((accent_color >> 16) & 0xff) / 255.0f, 1.0f), "%s", title);
    }
}

void MoshRackGUI::EndSectionFrame() {
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 4));
}

void MoshRackGUI::RenderVUMeter() {
    BeginSectionFrame("AUDIO LEVELS", C_CYAN);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;
    float h = 36.0f;

    draw->AddRectFilled(p, ImVec2(p.x + w, p.y + h), C_GROOVE);

    static const char* bands[] = { "BASS", "MID", "HIGH", "LFO", "PITCH", "VEL" };
    static const unsigned int colors[] = { C_PINK, C_CYAN, C_AMBER, C_GREEN, C_PURPLE, C_RED };
    float bw = w / 6.0f;

    for (int i = 0; i < 6; i++) {
        float x0 = p.x + i * bw + 2;
        float level = 0.0f;

        if (engine) {
            if (i == 0) level = engine->audio_level_bass.load(std::memory_order_relaxed);
            else if (i == 1) level = engine->audio_level_mid.load(std::memory_order_relaxed);
            else if (i == 2) level = engine->audio_level_high.load(std::memory_order_relaxed);
            else if (i == 3) level = engine->audio_level_lfo.load(std::memory_order_relaxed);
            else if (i == 4) level = engine->audio_level_pitch.load(std::memory_order_relaxed);
            else if (i == 5) level = engine->audio_level_vel.load(std::memory_order_relaxed);
        }

        float bar_h = (h - 16.0f) * std::clamp(level, 0.0f, 1.0f);

        draw->AddRectFilled(ImVec2(x0, p.y + 4), ImVec2(x0 + bw - 4, p.y + h - 12), C_BEVEL_SH);

        float bar_top = p.y + h - 12 - bar_h;
        for (float sy = p.y + h - 12; sy > bar_top; sy -= 3.0f) {
            float ratio = (p.y + h - 12 - sy) / (h - 12.0f);
            unsigned int c = colors[i];
            if (ratio > 0.85f) c = C_RED;
            else if (ratio > 0.65f) c = C_AMBER;
            draw->AddRectFilled(ImVec2(x0 + 1, sy - 2), ImVec2(x0 + bw - 5, sy), c);
        }

        draw->AddText(ImVec2(x0 + (bw - 4) * 0.5f - 10, p.y + h - 10), C_TEXT_DIM, bands[i]);
    }

    ImGui::Dummy(ImVec2(w, h));
    EndSectionFrame();
}