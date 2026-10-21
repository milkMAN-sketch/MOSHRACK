#pragma once
#ifndef MOSHRACK_H
#define MOSHRACK_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

// Include cross-platform hardware libraries
#include "RtMidi.h"
#include "miniaudio.h"
#include "MoshRackSplash.h"
#include "Shader.h"

class MoshRackEngine {
public:
    struct AudioDeviceInfo {
        int id;
        std::string name;
        bool isInput;
        ma_device_id maId;
    };
    struct DeviceInfo {
        int id;
        std::string name;
    };
    void InitGL(); // Call this AFTER your OpenGL context is created
    MoshRackEngine();
    ~MoshRackEngine(); // Changed from = default so we can clean up MIDI/Audio safely

    void SavePatchFile(const std::string& filepath);
    void LoadPatchFile(const std::string& filepath);
    void PollMidiDevices();
    void PollAudioDevices();
    void ConnectMidiDevice(int index);
    void ConnectAudioDevice(int index);
    bool is_learning_midi = false;
    void ConnectAudioLoopback();

    ma_device audio_device;
    bool is_audio_initialized = false;

    std::string param_to_learn = "";
   
	//fade between effects
    std::unordered_map<std::string, float> fx_fade_weights;

    std::vector<DeviceInfo> GetAvailableCameras();
    std::vector<DeviceInfo> availableMidiDevices;
    std::vector<AudioDeviceInfo> availableAudioDevices;
    //screensaver below

   
    MoshRackSplash splash_system;
    cv::Mat custom_splash_img;
    void load_splash_media(const std::string& filepath);
    void clear_splash_media();
         
    float splash_blend = 0.0f;
    std::string active_midi_controller_name = "default";
    std::unordered_map<int, std::string> cc_to_param; // CC Number -> Param ID String
    std::unordered_map<int, std::string> note_to_param; // Note Number -> Param ID String

   
    
   
    void SetParamFromNote(int note, int velocity);
    void SetParamFromCC(int cc, int value);
    void SaveMidiMap(const std::string& filename);
    void LoadMidiMap(const std::string& filename);
    void LoadBeatStepProDefaults();

    // Static callback for RtMidi
    static void MidiCallback(double timeStamp, std::vector<unsigned char>* message, void* userData);

    std::atomic<float> current_audio_rms{ 0.0f }; // Thread-safe audio energy level for visuals
    // Process a frame and return processed output
    cv::Mat process_frame(cv::Mat& frame, bool is_new_frame = true);

    // Core effect function stubs
    cv::Mat fx_mosh(const cv::Mat& frame);
    cv::Mat fx_mesh(const cv::Mat& frame);
    cv::Mat fx_flow(const cv::Mat& frame);

    cv::Mat fx_zoom(const cv::Mat& frame);
    cv::Mat fx_transform_3d(const cv::Mat& frame);

    cv::Mat fx_chroma(const cv::Mat& frame);
    cv::Mat fx_lfo(const cv::Mat& frame);

    cv::Mat fx_rgb(const cv::Mat& frame);
    cv::Mat fx_kscope(const cv::Mat& frame);

    // Modulation / control

    float get_mod(const std::string& param_name);
    void build_grids(int w, int h);

    std::unordered_map<std::string, float> current_fx_gains;
    std::unordered_map<std::string, bool> fx_enabled;

    std::atomic<float> audio_level_bass{ 0.0f };
    std::atomic<float> audio_level_mid{ 0.0f };
    std::atomic<float> audio_level_high{ 0.0f };
    std::atomic<float> audio_level_lfo{ 0.0f };
    std::atomic<float> audio_level_pitch{ 0.0f };
    std::atomic<float> audio_level_vel{ 0.0f };
    std::unordered_map<std::string, std::string> patch_matrix;
    std::mutex engine_mutex;
    std::vector<std::string> fx_chain;

    // --- MOSH STATE VARIABLES ---

    float mosh_wet_dry = 1.0f;
    float mosh_gain = 0.0f;
    float mosh_decay = 0.0f;

    //LFO Variables
    float lfo_rate = 1.0f;
    float lfo_gain = 0.8f;
    float lfo_bias = 0.0f;
    float lfo_phase = 0.0f;
    float lfo_current_value = 0.0f;
    bool lfo_active = false;
    void register_sync_beat(std::chrono::steady_clock::time_point t);
    void tap_tempo();
    void toggle_beat_sync(bool active);
    std::vector<std::chrono::steady_clock::time_point> sync_beat_times;
    std::atomic<bool> lfo_beat_sync_active{ false };
    std::string lfo_status_text = "Idle";
    std::mutex sync_mutex;
    bool beat_sync_enabled = false;
    void tick_lfo();

    //optical flow variables
    cv::Mat flow_prev_gray; // Persistent buffer for optical flow calculations
    cv::Mat flow_smoothed;

    float chroma_amount = 0.0f;
    std::unordered_map<std::string, float> module_sensitivities;

    void clear_patchbay();
    void reset_knobs();

    std::unordered_map<std::string, std::string> active_cables;
    
    float wet_dry_mix = 1.0f;


    float key_amount = 0.0f;
    float rgb_gains[3] = { 0.0f, 0.0f, 0.0f };


    float audio_pitch_val = 0.0f;
    float audio_velocity = 0.0f;

    Shader moshShader;
    Shader meshShader;
    Shader chromaShader;
    Shader flowShader;
    Shader kscopeShader;

    // mesh stuff
    struct Vertex {
        float x, y, z;
        float u, v;
    };

    cv::Mat generateNoiseMat(float theta, float resolution, int w, int h);
    void updateMeshGeometry(int gridsize, bool showTriangles, bool showHoriz, bool showVert, int w, int h, GLuint& outVAO, GLuint& outVBO, int& outVertexCount, GLenum& outPrimitive);
    int mesh_type;

   

    int lastMeshTypeTracker;
    float mesh_phase_x;
    float mesh_phase_y;
    float mesh_phase_z;

    bool mesh_triangles = true;
    bool mesh_horiz = false;
    bool mesh_vert = false;
    int lastGridSize = -1;
    int lastTopologyState = -1;
    //KSCOPE STUFF
    float kscope_angle = 0.0f;

    GLuint GetVideoTextureID() const { return videoTexture; }
    int GetOutputWidth() const { return cached_w; }
    int GetOutputHeight() const { return cached_h; }
    float audio_input_gain = 1.0f;
    bool is_paused = false;
    int cached_w = 0;
    int cached_h = 0;

private:
    cv::Mat grid_x;
    cv::Mat grid_y;
    cv::Mat prev_frame;
    cv::Mat slowmo_buffer;
    cv::Mat cached_post_mix_frame;

    cv::Mat accum_vectors;
    cv::Mat kscope_buffer;
    cv::Mat mosh_prev_gray;
    cv::Mat mosh_prev_frame;
    cv::Mat mosh_buffer;
    cv::Mat flow_mat;
    float chroma_warp_phase = 0.0f;

    // OpenGL Quad and Texture handles
    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    unsigned int videoTexture = 0;

    // Multiplier to scale incoming audio energy (0.0f to 5.0f)
    float last_snare = 0.0f;
    double last_beat_time = 0.0;

    std::vector<double> tap_history;

    ma_device m_audioDevice;
    bool m_isAudioRunning = false;

    void InitDefaultLoopback();
    void StopAudioDevice();
    // --- NEW: Internal Hardware Pointers ---
    RtMidiIn* m_midiIn = nullptr;
    ma_context m_audioContext;
    bool m_audioContextInitialized = false;

    // Windows-specific camera enumeration
    void EnumerateWindowsCameras(
        std::vector<DeviceInfo>& devices,
        std::vector<std::string>& source_names);
};

#endif // MOSHRACK_H