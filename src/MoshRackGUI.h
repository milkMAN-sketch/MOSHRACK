#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <GL/glew.h>
#include <imgui.h>
#include "VideoWorker.h"
#include <unordered_map>
class MoshRackEngine;
class VideoWorker;
class MoshRackGUI {
public:
    MoshRackGUI(std::shared_ptr<MoshRackEngine> eng);
    ~MoshRackGUI();

    void Init();
    void Shutdown();
    void RenderControlPanel();                 // Handles all buttons, sliders, patchbay UI
    void RenderNativeOutputWindow(int w, int h); // Handles rendering the video texture to the second window

    enum class VideoInputType { HardwareCamera, FileStream };

    std::unordered_map<std::string, ImVec2> jack_positions;
    std::unordered_map<std::string, std::string> active_cables; // Maps Target_ID -> Source_ID

    bool is_dragging_cable = false;
    std::string drag_start_id = "";
    bool drag_start_is_source = false;

    MoshRackEngine* engine;
    MoshRackEngine* m_engine;
    bool midi_mapping_mode = false;
    VideoInputType currentVideoType = VideoInputType::HardwareCamera;
    int selectedVideoIdx = 0;
	int selectedMidiIdx = -1;
	int selectedAudioIdx = -1;
    std::string customVideoFilePath;
    
    bool is_rack_paused = false;

    bool m_isSelectingFile = false;
    bool show_reset_popup = false;
    bool has_unsaved_changes = false;

    bool m_isPlaying = false; // Tracks whether video playback is actively started

    std::vector<MoshRackEngine::DeviceInfo> videoDevices;
    std::vector<std::string> video_sources;

    std::vector<MoshRackEngine::AudioDeviceInfo> audioDevices;
    std::vector<std::string> audio_sources;

    std::vector<MoshRackEngine::DeviceInfo> midiDevices;
    std::vector<std::string> midi_sources;

    VideoWorker m_videoWorker;
    GLuint m_videoTextureId = 0;

    std::string status_text;
    float status_timer = 0.0f;
    unsigned int status_color = 0xFFFFFFFF;
    float rgb_gains[3] = { 1.0f, 1.0f, 1.0f };
   void UpdateVideoTexture(); 
private:
    void PollHardware();
    void OpenVideoFileDialog();
    void OpenImageFileDialog();
 
    

    void RenderMenuBar();
    void RenderOutputFeed(ImVec2 displaySize);
    void RenderHeader();
    void RenderVUMeter();
    void RenderSignalIngest();
    void RenderChainOrderPanel();
    
    
    void RenderLFOSection();
    void RenderMoshSection();
    void RenderFlowSection();
    void RenderChromaSection();
    void RenderKscopeSection();
    void RenderMeshSection();
    void RenderZoomSection();
    
    void RenderRGBSection();
    
    
    void RenderFooter();

    void RenderRightPanel();
    void RenderPatchBay();

    bool CircularKnob(const char* label, float* value, float min_val, float max_val,
        float default_val, unsigned int color, bool bipolar,
        const char* tooltip, const char* param_name);
    bool LEDToggle(const char* label, bool* state,
        unsigned int on_color = 0, unsigned int off_color = 0,
        float width = 80.0f, float height = 20.0f,
        const char* param_name = nullptr);
    void BeginSectionFrame(const char* title, unsigned int accent_color);
    void EndSectionFrame();

    void SavePatch(const std::string& filename);
    void LoadPatch(const std::string& filename);
    void DeletePatch(const std::string& filename);
    void ShowStatus(const std::string& msg, unsigned int color);
};