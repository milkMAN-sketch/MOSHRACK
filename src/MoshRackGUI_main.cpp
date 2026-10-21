#include "MoshRackGUI.h"
#include "MoshRackEngine.h"
#include "VideoWorker.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#include <thread>
#include <imgui.h>
#include <fstream>
#include <iostream>
#include <algorithm>

// --- COLOR CONSTANTS ---
constexpr unsigned int C_AMBER = 0xFF00CC55;
constexpr unsigned int C_CYAN = 0xFFCCCC00;
constexpr unsigned int C_PURPLE = 0xFFAA44FF;
constexpr unsigned int C_BEVEL_SH = 0xFF0E0E14;

MoshRackGUI::MoshRackGUI(std::shared_ptr<MoshRackEngine> eng)
    : engine(eng.get()), m_engine(eng.get()), m_videoWorker(eng.get()) {
    video_sources.clear();
    audio_sources.clear();
    midi_sources.clear();
}

void MoshRackGUI::Init() {
    PollHardware();
}

void MoshRackGUI::Shutdown() {
    if (m_videoWorker.isRunning()) {
        m_videoWorker.stop();
    }
}
void MoshRackGUI::PollHardware() {
    if (engine) {
        engine->PollMidiDevices();
        engine->PollAudioDevices();

        // 1. Sync MIDI
        midiDevices.clear();
        midi_sources.clear();
        for (const auto& midi : engine->availableMidiDevices) {
            midiDevices.push_back(midi);
            midi_sources.push_back(midi.name);
        }

        // 2. Sync Audio Devices into audioDevices and audio_sources
        audioDevices.clear();
        audio_sources.clear();
        for (const auto& audio : engine->availableAudioDevices) {
            audioDevices.push_back(audio);
            audio_sources.push_back(audio.name); // Already tagged with [Input] / [Output]
        }

        // 3. Sync Cameras into videoDevices and video_sources
        videoDevices.clear();
        video_sources.clear();
        auto cameras = engine->GetAvailableCameras();
        for (const auto& cam : cameras) {
            videoDevices.push_back(cam);
            video_sources.push_back(std::to_string(cam.id) + ": " + cam.name);
        }
    }
}



void MoshRackGUI::OpenVideoFileDialog() {
    // Run asynchronously on a background thread so the main UI/screensaver loop never blocks
    std::thread([this]() {
#ifdef _WIN32
        char szFile[260] = { 0 };
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Video Files (*.mp4;*.avi;*.mov;*.mkv)\0*.MP4;*.AVI;*.MOV;*.MKV\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE) {
            std::string selectedPath = ofn.lpstrFile;
            if (engine) {
                std::lock_guard<std::mutex> lock(engine->engine_mutex);
                customVideoFilePath = selectedPath;
            }
            else {
                customVideoFilePath = selectedPath;
            }
            ShowStatus("Video file selected: " + customVideoFilePath, 0xFF00FF66);
        }
#elif defined(__APPLE__) || defined(__linux__)
        std::string inputPath;
        std::cout << "Enter video file path: ";
        std::getline(std::cin, inputPath);
        if (!inputPath.empty()) {
            if (engine) {
                std::lock_guard<std::mutex> lock(engine->engine_mutex);
                customVideoFilePath = inputPath;
            }
            else {
                customVideoFilePath = inputPath;
            }
            ShowStatus("Video file selected: " + customVideoFilePath, 0xFF00FF66);
        }
#endif
        }).detach();
}

void MoshRackGUI::OpenImageFileDialog() {
    // Run asynchronously on a background thread to keep screensaver physics flowing
    std::thread([this]() {
#ifdef _WIN32
        char szFile[260] = { 0 };
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Image Files (*.png;*.jpg;*.bmp;*.webp)\0*.PNG;*.JPG;*.BMP;*.WEBP\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn) == TRUE) {
            std::string imagePath = ofn.lpstrFile;
            if (engine) {
                {
                    std::lock_guard<std::mutex> lock(engine->engine_mutex);
                    engine->splash_system.load_custom_image(imagePath);
                }
                ShowStatus("Image loaded: " + imagePath, 0xFF00FF66);
            }
        }
#elif defined(__APPLE__) || defined(__linux__)
        std::string imagePath;
        std::cout << "Enter image file path: ";
        std::getline(std::cin, imagePath);
        if (!imagePath.empty() && engine) {
            {
                std::lock_guard<std::mutex> lock(engine->engine_mutex);
                engine->splash_system.load_custom_image(imagePath);
                engine->splash_blend = 1.0f;
            }
            ShowStatus("Image loaded: " + imagePath, 0xFF00FF66);
        }
#endif
        }).detach();
}

void MoshRackGUI::RenderChainOrderPanel() {
    if (!engine || engine->fx_chain.empty()) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(18, 20, 26, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 220, 255, 90));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

    bool child_open = ImGui::BeginChild("HorizontalChainBar", ImVec2(0, 60.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (child_open) {
        // Smooth horizontal mouse-wheel scrolling
        if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
            float current_scroll = ImGui::GetScrollX();
            ImGui::SetScrollX(current_scroll - ImGui::GetIO().MouseWheel * 40.0f);
        }

        // Helper to abbreviate any module name longer than 6 letters
        auto abbreviateName = [](const std::string& name) -> std::string {
            if (name.length() <= 6) return name;
            return name.substr(0, 6); // Truncate cleanly to 6 chars
            };

        // Calculate uniform sleek width based on the longest *abbreviated* name
        float max_text_width = 0.0f;
        for (const auto& name : engine->fx_chain) {
            std::string shortName = abbreviateName(name);
            float w = ImGui::CalcTextSize(shortName.c_str()).x;
            if (w > max_text_width) max_text_width = w;
        }
        float uniform_width = max_text_width + 16.0f; // Ultra-sleek compact padding
        if (uniform_width < 50.0f) uniform_width = 50.0f;

        // Perfectly aligns the text vertically with the upcoming buttons
        ImGui::AlignTextToFramePadding();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 255, 150, 255));
        ImGui::Text("IN [CAM] >>");
        ImGui::PopStyleColor();

        ImGui::SameLine();

        for (size_t i = 0; i < engine->fx_chain.size(); ++i) {
            std::string shortName = abbreviateName(engine->fx_chain[i]);

            // Render the compact, uniformly-sized button with the short name
            ImGui::Button((shortName + "##" + std::to_string(i)).c_str(), ImVec2(uniform_width, 26.0f));

            // Drag and drop source uses full name in the tooltip preview for clarity
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("REORDER_FX", &i, sizeof(size_t));
                ImGui::Text("Move %s", engine->fx_chain[i].c_str());
                ImGui::EndDragDropSource();
            }

            // Drag and drop target performs the scoot/insert shift
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("REORDER_FX")) {
                    size_t src = *(const size_t*)payload->Data;
                    if (src != i) {
                        std::string moved_item = engine->fx_chain[src];
                        engine->fx_chain.erase(engine->fx_chain.begin() + src);
                        engine->fx_chain.insert(engine->fx_chain.begin() + i, moved_item);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::SameLine();
        }

        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 255, 100, 255));
        ImGui::Text(">> OUT");
        ImGui::PopStyleColor();

        
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}
void MoshRackGUI::UpdateVideoTexture() {
    cv::Mat frame;
    bool has_frame = m_videoWorker.getLatestFrame(frame);

    // Keep a static cache of the last valid frame so we never flash black when waiting
    static cv::Mat s_lastFrame = cv::Mat::zeros(480, 640, CV_8UC3);

    if (has_frame && !frame.empty()) {
        s_lastFrame = frame.clone();
    }

    // Use the cached frame for processing and rendering
    cv::Mat processed_frame = s_lastFrame;
    if (engine) {
        // Only tell the engine it's a new frame if we actually received a fresh one
        processed_frame = engine->process_frame(s_lastFrame, has_frame);
    }

    if (processed_frame.empty() || processed_frame.data == nullptr || processed_frame.cols <= 0 || processed_frame.rows <= 0) {
        processed_frame = s_lastFrame;
    }

    if (m_videoTextureId == 0) {
        glGenTextures(1, &m_videoTextureId);
        glBindTexture(GL_TEXTURE_2D, m_videoTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    cv::Mat rgb;
    if (processed_frame.channels() == 3) {
        cv::cvtColor(processed_frame, rgb, cv::COLOR_BGR2RGB);
    }
    else {
        cv::cvtColor(processed_frame, rgb, cv::COLOR_GRAY2RGB);
    }

    if (!rgb.empty() && rgb.data != nullptr) {
        glBindTexture(GL_TEXTURE_2D, m_videoTextureId);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgb.cols, rgb.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data);
    }
}
void MoshRackGUI::RenderControlPanel() {
    if (engine) {
        engine->tick_lfo();
    }
    RenderMenuBar();
    UpdateVideoTexture();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;

    // Set up the control panel to fill or fit the primary window cleanly
    ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);

    ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

    if (ImGui::Begin("MOSHRACK // CONTROL PANEL", nullptr, host_flags)) {
        if (ImGui::BeginTable("MainLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, 0.0f))) {
            ImGui::TableSetupColumn("LeftControls", ImGuiTableColumnFlags_WidthFixed, 460.0f);
            ImGui::TableSetupColumn("RightPatchBay", ImGuiTableColumnFlags_WidthStretch);


            ImGui::TableNextRow();

            // --- LEFT COLUMN ---
            ImGui::TableSetColumnIndex(0);
            ImGui::Dummy(ImVec2(380.0f, 0.0f));
            RenderHeader();
            RenderSignalIngest();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("VFX MODULES RACK");

            float leftRemainingHeight = ImGui::GetContentRegionAvail().y - 10.0f;

            // Use the class-level variable so the Retro LCD can read it too!
            ImGuiWindowFlags effects_flags = is_rack_paused ?
                (ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar) : ImGuiWindowFlags_None;

            ImGui::BeginChild("EffectsScrollRegion", ImVec2(0, (std::max)(200.0f, leftRemainingHeight)), true, effects_flags);

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    is_rack_paused = !is_rack_paused;
                }
            }

            RenderLFOSection();
            RenderMoshSection();
            RenderFlowSection();
            RenderKscopeSection();
            RenderChromaSection();
            RenderMeshSection();
            RenderZoomSection();
            RenderRGBSection();
            RenderFooter();
            ImGui::EndChild();

            // --- RIGHT COLUMN ---
            ImGui::TableSetColumnIndex(1);
            float rightPanelHeight = ImGui::GetContentRegionAvail().y;
            ImGui::BeginChild("RightPanelScrollRegion", ImVec2(0, rightPanelHeight), false, ImGuiWindowFlags_None);
            RenderRightPanel();
            ImGui::EndChild();

            ImGui::EndTable();
        }
        ImGui::End();
    }
    ImGui::PopStyleVar();
}


void MoshRackGUI::SavePatch(const std::string& filename) {
#ifdef _WIN32
    char szFile[260] = { 0 };
    strcpy_s(szFile, "default.patch");
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Patch Files (*.patch)\0*.PATCH\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn) == TRUE) {
        if (engine) {
            engine->SavePatchFile(ofn.lpstrFile);
            ShowStatus("PATCH SAVED: " + std::string(ofn.lpstrFile), 0xFF00FF66);
        }
    }
#else
    // Cross-platform: Use default patch file
    if (engine) {
        std::string patchPath = "default.patch";
        engine->SavePatchFile(patchPath);
        ShowStatus("PATCH SAVED: " + patchPath, 0xFF00FF66);
    }
#endif
}

void MoshRackGUI::LoadPatch(const std::string& filename) {
#ifdef _WIN32
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Patch Files (*.patch)\0*.PATCH\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        if (engine) {
            engine->LoadPatchFile(ofn.lpstrFile);
            ShowStatus("PATCH LOADED: " + std::string(ofn.lpstrFile), 0xFF00FF66);
        }
    }
#else
    // Cross-platform: Use default patch file
    if (engine) {
        std::string patchPath = "default.patch";
        engine->LoadPatchFile(patchPath);
        ShowStatus("PATCH LOADED: " + patchPath, 0xFF00FF66);
    }
#endif
}

void MoshRackGUI::DeletePatch(const std::string& filename) {
#ifdef _WIN32
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Patch Files (*.patch)\0*.PATCH\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        if (std::remove(ofn.lpstrFile) == 0) {
            ShowStatus("PATCH DELETED", 0xFFFF3333);
        }
        else {
            ShowStatus("FAILED TO DELETE PATCH", 0xFFFF3333);
        }
    }
#else
    // Cross-platform: Just show a message
    ShowStatus("Patch deletion only supported on Windows", 0xFFFF3333);
#endif
}

void MoshRackGUI::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Menu: Patches, MIDI, and RESET.")) {
            if (ImGui::MenuItem("Load Patch...")) LoadPatch("default.patch");

            std::string save_label = has_unsaved_changes ? "CLICK TO SAVE PATCH *" : "Save Patch...";
            if (ImGui::MenuItem(save_label.c_str())) {
                SavePatch("default.patch");
                has_unsaved_changes = false;
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Delete Patch...")) DeletePatch("default.patch");
            ImGui::Separator();

            if (ImGui::MenuItem("Reset Defaults...")) {
                show_reset_popup = true;
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Enable MIDI Mapping Mode", nullptr, &midi_mapping_mode)) {}
            if (ImGui::MenuItem("Save Current MIDI Map")) {
                engine->SaveMidiMap("midi_map_" + engine->active_midi_controller_name + ".txt");
                ShowStatus("MIDI MAP SAVED", 0xFF00FF66);
            }
            if (ImGui::MenuItem("Clear MIDI Map")) {
                engine->cc_to_param.clear();
                ShowStatus("MIDI CONFIG CLEARED", 0xFFFF3333);
            }
            ImGui::EndMenu();
        }

        if (midi_mapping_mode) {
            ImGui::SameLine();
            if (engine->is_learning_midi) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), " [MAPPING '%s' - TURN A KNOB OR TAP A PAD]", engine->param_to_learn.c_str());
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), " [MIDI MAPPING ENGAGED - CLICK A PARAMETER TO MAP]");
            }
        }
        ImGui::EndMainMenuBar();
    }

    if (show_reset_popup) {
        ImGui::OpenPopup("Reset to Defaults?");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Reset to Defaults?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure? This will reset ALL parameters.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Yes", ImVec2(120, 0))) {
            if (engine) {
                engine->clear_patchbay();
                engine->reset_knobs();
            }
            active_cables.clear();
            has_unsaved_changes = false;
            ShowStatus("RESET TO DEFAULTS", 0xFFFF3333);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("No", ImVec2(120, 0))) {
            has_unsaved_changes = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    show_reset_popup = false;
}
void MoshRackGUI::RenderNativeOutputWindow(int w, int h) {
    if (m_videoTextureId == 0) {
        return; // No texture generated yet, keep it black
    }

    // We use an OpenGL Framebuffer Blit to blast the shared texture directly to the screen.
    // No ImGui needed, which bypasses the context boundary issue entirely!

    // 1. Get the actual width and height of the generated texture
    GLint tex_w, tex_h;
    glBindTexture(GL_TEXTURE_2D, m_videoTextureId);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_h);

    // 2. Create a temporary framebuffer to read from the texture
    GLuint readFbo;
    glGenFramebuffers(1, &readFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_videoTextureId, 0);

    // 3. Bind the window's default screen buffer and blit (copy) the pixels over
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, tex_w, tex_h,   // Source coordinates
        0, h, w, 0, // Destination coordinates (scales to window automatically)
       
        GL_COLOR_BUFFER_BIT,
        GL_LINEAR);           // Smooth scaling

    // 4. Clean up
    glDeleteFramebuffers(1, &readFbo);
}


void MoshRackGUI::RenderOutputFeed(ImVec2 displaySize) {
    if (displaySize.x <= 0.0f || displaySize.y <= 0.0f) return;

   

    if (m_videoTextureId != 0) {
        ImGui::Image((void*)(intptr_t)m_videoTextureId, displaySize);
    }
}

void MoshRackGUI::RenderHeader() {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x;

    draw->AddRectFilled(p, ImVec2(p.x + w, p.y + 2), C_AMBER);
    ImGui::Dummy(ImVec2(0, 4));

    ImGui::PushStyleColor(ImGuiCol_Text, C_AMBER);
    ImGui::Text("MOSH-RACK  ||  HYBRID VIDEO SYNTH  ||  CV PATCH ENGINE");
    ImGui::PopStyleColor();
    ImGui::SameLine(w - 40);
    ImGui::TextDisabled("v1.1");

    draw->AddRectFilled(ImVec2(p.x, p.y + 24), ImVec2(p.x + w, p.y + 25), C_BEVEL_SH);
    ImGui::Dummy(ImVec2(0, 2));
}

void MoshRackGUI::RenderSignalIngest() {
    BeginSectionFrame("1. SIGNAL INGEST & HARDWARE", C_AMBER);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));

    // --- ROW 1: Video Source Combo & Load Video Button ---
    std::string videoPreview = "None";
    if (currentVideoType == VideoInputType::FileStream) {
        videoPreview = "File: " + customVideoFilePath;
    }
    else {
        if (!video_sources.empty() && selectedVideoIdx >= 0 && selectedVideoIdx < video_sources.size()) {
            videoPreview = video_sources[selectedVideoIdx];
        }
    }

    float availWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth((std::max)(120.0f, availWidth - 85.0f));
    if (ImGui::BeginCombo("##VideoSourceCombo", videoPreview.c_str())) {
        for (int i = 0; i < static_cast<int>(video_sources.size()); ++i) {
            const bool isSelected = (currentVideoType == VideoInputType::HardwareCamera && selectedVideoIdx == i);
            if (ImGui::Selectable(video_sources[i].c_str(), isSelected)) {
                selectedVideoIdx = i;
                currentVideoType = VideoInputType::HardwareCamera;
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        if (ImGui::Selectable("   [ Load Video File... ]", currentVideoType == VideoInputType::FileStream)) {
            currentVideoType = VideoInputType::FileStream;
            OpenVideoFileDialog();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    static int activeVideoIdx = -1;
    static VideoInputType activeVideoType = VideoInputType::HardwareCamera;
    static std::string activeVideoFilePath = ""; // <-- Track active file path too

    std::string buttonLabel;
    bool hasSourceChanged = false;

    if (activeVideoIdx == -1) {
        buttonLabel = "Load Video";
    }
    else {
        // Check if the source type, camera index, or file path changed
        if (activeVideoType != currentVideoType) {
            hasSourceChanged = true;
        }
        else if (currentVideoType == VideoInputType::HardwareCamera && activeVideoIdx != selectedVideoIdx) {
            hasSourceChanged = true;
        }
        else if (currentVideoType == VideoInputType::FileStream && activeVideoFilePath != customVideoFilePath) {
            hasSourceChanged = true;
        }

        buttonLabel = hasSourceChanged ? "Swap" : "Loaded";
    }

    bool isDisabled = (buttonLabel == "Loaded");
    if (isDisabled) ImGui::BeginDisabled();

    bool isSwap = (buttonLabel == "Swap");
    if (isSwap) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 1.0f, 1.0f)); // Bright Cyan
    }

    if (ImGui::Button(buttonLabel.c_str())) {
        activeVideoIdx = selectedVideoIdx;
        activeVideoType = currentVideoType;
        activeVideoFilePath = customVideoFilePath; // <-- Update stored active path

        if (!m_videoWorker.isRunning()) {
            m_videoWorker.start();
        }
        if (currentVideoType == VideoInputType::HardwareCamera) {
            int hw_id = videoDevices[selectedVideoIdx].id;
            m_videoWorker.setCameraSource(hw_id);
        }
        else if (currentVideoType == VideoInputType::FileStream) {
            m_videoWorker.setFileSource(customVideoFilePath);
        }
    }

    if (isSwap) {
        ImGui::PopStyleColor();
    }
    if (isDisabled) ImGui::EndDisabled();

    // --- ROW 2: Poll Hardware Button in front of MIDI Dropdown ---
    if (ImGui::Button("Poll HW", ImVec2(75.0f, 24.0f))) {
        PollHardware();
        ShowStatus("Hardware re-polled!", C_AMBER);
    }
    ImGui::SameLine();

    std::string midiPreview = "Select MIDI Source...";
    if (selectedMidiIdx >= 0 && selectedMidiIdx < static_cast<int>(midiDevices.size())) {
        midiPreview = midiDevices[selectedMidiIdx].name;
    }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##MidiSourceCombo", midiPreview.c_str())) {
        for (int i = 0; i < static_cast<int>(midiDevices.size()); ++i) {
            const bool isSelected = (selectedMidiIdx == i);
            if (ImGui::Selectable(midiDevices[i].name.c_str(), isSelected)) {
                selectedMidiIdx = i;
                if (midiDevices[i].id >= 0) engine->ConnectMidiDevice(midiDevices[i].id);
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // --- ROW 3: Audio Source Dropdown ---
    std::string audioPreview = "Select Audio Source...";
    if (selectedAudioIdx >= 0 && selectedAudioIdx < static_cast<int>(audioDevices.size())) {
        audioPreview = audioDevices[selectedAudioIdx].name;
    }
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##AudioSourceCombo", audioPreview.c_str())) {
        for (int i = 0; i < static_cast<int>(audioDevices.size()); ++i) {
            const bool isSelected = (selectedAudioIdx == i);
            if (ImGui::Selectable(audioDevices[i].name.c_str(), isSelected)) {
                selectedAudioIdx = i;
                engine->ConnectAudioDevice(i);
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    // --- ROW 4: Freeze, Splash, Knobs, and Pseudo-RGB LCD Status Screen ---

    // 1. Freeze Button
    bool isPaused = (engine && engine->is_paused);
    std::string freezeLabel = isPaused ? " UNFREEZE " : " FREEZE ";

    ImVec4 icyBlueBase = ImVec4(0.2f, 0.65f, 0.9f, 1.0f);
    ImVec4 icyBlueHover = ImVec4(0.35f, 0.78f, 1.0f, 1.0f);
    ImVec4 icyBlueActive = ImVec4(0.1f, 0.5f, 0.75f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, icyBlueBase);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, icyBlueHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, icyBlueActive);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    if (ImGui::Button(freezeLabel.c_str(), ImVec2(75.0f, 32.0f))) {
        if (engine) {
            engine->is_paused = !engine->is_paused;
        }
    }
    ImGui::PopStyleColor(4);
    ImGui::SameLine();

    // 2. Splash Toggle
    bool splash_state = engine->splash_blend > 0.5f;
    if (LEDToggle("SPLASH", &splash_state, C_CYAN, C_CYAN, 65, 32, "SPLASH_TOGGLE")) {
        engine->splash_blend = splash_state ? 1.0f : 0.0f;
    }
    if (ImGui::BeginPopupContextItem("SplashMenu", ImGuiPopupFlags_MouseButtonRight)) {
        if (ImGui::MenuItem("Load Custom Image...")) {
            OpenImageFileDialog();
        }
        if (ImGui::MenuItem("Clear Custom image (Use MOSHRACK screensaver)")) {
            if (engine) {
                engine->clear_splash_media();
                ShowStatus("Cleared custom splash!", C_AMBER);
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();

    // 3. Fade Knob
    ImGui::BeginGroup();
    CircularKnob("FADE", &engine->splash_blend, 0.0f, 1.0f, 1.0f, C_AMBER, false, "Crossfade Splash/Video", "SPLASH_FADE");
    ImGui::EndGroup();
    ImGui::SameLine();

    // 4. Wet/Dry Knob
    ImGui::BeginGroup();
    CircularKnob("WET/DRY", &engine->wet_dry_mix, 0.0f, 1.0f, 1.0f, C_PURPLE, false, "Wet/Dry Master", "wet_dry");
    ImGui::EndGroup();
    ImGui::SameLine();

    // 5. Retro Pseudo-RGB LCD Status Screen occupying the remaining empty space on the right
    float remainingWidth = (std::max)(150.0f, ImGui::GetContentRegionAvail().x);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(10, 14, 12, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(30, 90, 50, 200));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 4.0f));

    if (ImGui::BeginChild("RetroLCDDisplay", ImVec2(remainingWidth, 42.0f), true, ImGuiWindowFlags_NoScrollbar)) {
        // --- MAIN LCD TEXT ---
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 130, 255));
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + remainingWidth - 6.0f);

        if (!status_text.empty()) {
            ImGui::Text("%s", status_text.c_str());
        }
        else {
            // Bonus: I wired up live FPS so the retro display is actually accurate!
            ImGui::Text("SYS: ONLINE | FPS: %.1f\nAUDIO/MIDI: LINKED", ImGui::GetIO().Framerate);
        }

        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor(); // Pop main green text

        // --- SCROLL STATUS INDICATOR (Pinned to bottom-right) ---
        // Calculate a flashing alpha for the retro PAUSED LED effect
        int alpha = is_rack_paused ? (int)(((std::sin(ImGui::GetTime() * 10.0f) * 0.5f) + 0.5f) * 255) : 255;
        ImU32 statusColor = is_rack_paused ? IM_COL32(255, 50, 50, alpha) : IM_COL32(0, 150, 50, 255);
        const char* scrollText = is_rack_paused ? "SCROLL: PAUSED" : "SCROLL: ACTIVE";

        // Mathematically push the text to the far right, on the second line (Y: 22.0f)
        ImVec2 textSize = ImGui::CalcTextSize(scrollText);
        ImGui::SetCursorPos(ImVec2(remainingWidth - textSize.x - 8.0f, 22.0f));

        ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
        ImGui::Text("%s", scrollText);
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // ── FULL-WIDTH VFX CHAIN ORDER PANEL ──
    ImGui::TextDisabled("VFX CHAIN ORDER");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(18, 20, 26, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 220, 255, 120));

    if (ImGui::BeginChild("ChainPanelChild", ImVec2(-1.0f, 75.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        RenderChainOrderPanel();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);

    ImGui::PopStyleVar();
    EndSectionFrame();
}


void MoshRackGUI::RenderFooter() {
    ImGui::Dummy(ImVec2(0, 10));

    if (ImGui::Button("[ CLEAR PATCHBAY ]", ImVec2(180, 30))) {
        if (engine) {
            engine->clear_patchbay();
        }
        active_cables.clear(); // Keeps your local GUI tracking synced

        ShowStatus("Patchbay cleared", C_AMBER);
    }

    ImGui::SameLine();

    if (ImGui::Button("[ RESET KNOBS ]", ImVec2(180, 30))) {
        if (engine) {
            engine->reset_knobs();
        }
        ShowStatus("Knobs reset to defaults", C_AMBER);
    }
}

void MoshRackGUI::ShowStatus(const std::string& msg, unsigned int color) {
    status_text = msg;
    status_timer = 5.0f;
    status_color = color;
}

MoshRackGUI::~MoshRackGUI() {
    Shutdown();
}