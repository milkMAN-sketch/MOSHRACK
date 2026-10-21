#ifndef NOMINMAX
#define NOMINMAX
#endif
#define MINIAUDIO_IMPLEMENTATION
#include <GL/glew.h>
#include "MoshRackEngine.h"
#include "imgui.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>
#pragma comment(lib, "winmm.lib")
#include <iostream>
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#include <dshow.h>
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#include "MoshRackSplash.h"
#include <string>


MoshRackEngine::MoshRackEngine() {
    fx_chain = {
        "MOSH", "FLOW", "KSCOPE", "CHROMA",
        "MESH", "ZOOM", "ROTATION", "TRANSFORM_3D", "RGB"
    };

    for (const auto& fx : fx_chain) {
        fx_enabled[fx] = false;
    }
    splash_blend = 1.0f;
    wet_dry_mix = 1.0f;

    // --- EFFECT GAIN DEFAULTS ---
    current_fx_gains["MOSH"] = 0.0f;
    current_fx_gains["MOSH_DECAY"] = 0.0f;  // Default to low decay for long mosh smears

    // KSCOPE DEFAULTS
    current_fx_gains["KSCOPE_ROT"] = 0.0f;
    current_fx_gains["KSCOPE_OFFSET"] = 0.0f;
    current_fx_gains["KSCOPE_BLUR"] = 0.0f;
    current_fx_gains["KSCOPE_ZOOM"] = 1.0f; // Critical: Must be 1.0 so it doesn't crush to zero!

    // CHROMA 3-KNOB DEFAULTS
    current_fx_gains["CHROMA"] = 0.0f;       // Depth (RGB split)
    current_fx_gains["CHROMA_WARP"] = 0.0f;  // Warp Speed (Rainbow scroll)
    current_fx_gains["CHROMA_CRAZY"] = 0.0f; // Crazy (Neon tri-color split)

    if (ma_context_init(NULL, 0, NULL, &m_audioContext) == MA_SUCCESS) {
        m_audioContextInitialized = true;
        // Automatically start capturing desktop audio from the very first frame!
        InitDefaultLoopback();
    }

    // --- EFFECT TOGGLE DEFAULTS ---
    fx_enabled["CHROMA"] = false;
    fx_enabled["CHROMA_WARP"] = false;
    fx_enabled["CHROMA_CRAZY"] = false;
    fx_enabled["MESH"] = false;
    fx_enabled["MESH_H"] = false;
    fx_enabled["MESH_V"] = false;
    fx_enabled["MESH_Z"] = false;
    fx_enabled["FLOW"] = false;

    fx_enabled["TRANSFORM_3D"] = false;
    fx_enabled["ZOOM"] = false;
    fx_enabled["RGB"] = false;

    fx_enabled["LFO"] = false;
    fx_enabled["MOSH"] = false;
    fx_enabled["SPLASH"] = true;

    fx_enabled["RAINBOW"] = false;
    fx_enabled["MOSH_SIZE"] = false;
    fx_enabled["MOSH_DECAY"] = false;
    fx_enabled["KSCOPE"] = false;

    current_fx_gains["ZOOM"] = 0.0f;
    current_fx_gains["ROT_X"] = 0.0f;
    current_fx_gains["ROT_Y"] = 0.0f;
}

// 2. In MoshRackEngine.cpp, replace your old reset_to_defaults() with these
void MoshRackEngine::clear_patchbay() {
    active_cables.clear();
    module_sensitivities.clear();
    patch_matrix.clear();
}

void MoshRackEngine::reset_knobs() {
    // 1. Reset all standard map knobs to 0.0f
    for (auto& pair : current_fx_gains) {
        pair.second = 0.0f;
    }

    // 2. Safely override the knobs that will break the engine if left at 0.0f
    current_fx_gains["KSCOPE_SEGMENTS"] = 6.0f;
    current_fx_gains["KSCOPE_ZOOM"] = 1.0f;
    current_fx_gains["MESH_SCALE"] = 25.0f;
    current_fx_gains["MESH_Z"] = 1.0f;

    // 3. Reset standalone engine variables that aren't stored in the map
    lfo_rate = 1.0f;
    lfo_gain = 0.8f;
    lfo_bias = 0.0f;

    rgb_gains[0] = 0.0f; // Red
    rgb_gains[1] = 0.0f; // Green
    rgb_gains[2] = 0.0f; // Blue

    key_amount = 0.0f;
}
void MoshRackEngine::SavePatchFile(const std::string& filepath) {
    nlohmann::json patch;

    // Save FX enabled flags
    nlohmann::json fx_states = nlohmann::json::object();
    for (const auto& pair : fx_enabled) {
        fx_states[pair.first] = pair.second;
    }
    patch["fx_enabled"] = fx_states;

    // Save FX gain/knob values
    nlohmann::json gains = nlohmann::json::object();
    for (const auto& pair : current_fx_gains) {
        gains[pair.first] = pair.second;
    }
    patch["gains"] = gains;

    // Save Patchbay routing connections (dest -> source)
    nlohmann::json cables = nlohmann::json::object();
    for (const auto& pair : active_cables) {
        cables[pair.first] = pair.second;
    }
    patch["active_cables"] = cables;

    // Save Module Sensitivity / Attenuverter multipliers
    nlohmann::json sensitivities = nlohmann::json::object();
    for (const auto& pair : module_sensitivities) {
        sensitivities[pair.first] = pair.second;
    }
    patch["module_sensitivities"] = sensitivities;

    // Save Mesh specific state (topology & parameters)
    patch["mesh_state"]["triangles"] = mesh_triangles;
    patch["mesh_state"]["horiz"] = mesh_horiz;
    patch["mesh_state"]["vert"] = mesh_vert;

    // Save FX Chain order if applicable
    nlohmann::json chain = nlohmann::json::array();
    for (const auto& name : fx_chain) {
        chain.push_back(name);
    }
    patch["fx_chain"] = chain;

    // Write to disk
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << patch.dump(4);
        file.close();
    }
}

void MoshRackEngine::LoadPatchFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return;

    nlohmann::json patch;
    try {
        file >> patch;
    }
    catch (...) {
        file.close();
        return;
    }
    file.close();

    // Load FX enabled flags
    if (patch.contains("fx_enabled") && patch["fx_enabled"].is_object()) {
        for (auto& [key, val] : patch["fx_enabled"].items()) {
            if (val.is_boolean()) {
                fx_enabled[key] = val.get<bool>();
            }
        }
    }

    // Load FX gains/knobs
    if (patch.contains("gains") && patch["gains"].is_object()) {
        for (auto& [key, val] : patch["gains"].items()) {
            if (val.is_number()) {
                current_fx_gains[key] = val.get<float>();
            }
        }
    }

    // Load Patchbay routing connections
    if (patch.contains("active_cables") && patch["active_cables"].is_object()) {
        active_cables.clear();
        for (auto& [key, val] : patch["active_cables"].items()) {
            if (val.is_string()) {
                active_cables[key] = val.get<std::string>();
            }
        }
    }

    // Load Module Sensitivity / Attenuverter multipliers
    if (patch.contains("module_sensitivities") && patch["module_sensitivities"].is_object()) {
        for (auto& [key, val] : patch["module_sensitivities"].items()) {
            if (val.is_number()) {
                module_sensitivities[key] = val.get<float>();
            }
        }
    }

    // Load Mesh state
    if (patch.contains("mesh_state") && patch["mesh_state"].is_object()) {
        if (patch["mesh_state"].contains("triangles")) mesh_triangles = patch["mesh_state"]["triangles"].get<bool>();
        if (patch["mesh_state"].contains("horiz")) mesh_horiz = patch["mesh_state"]["horiz"].get<bool>();
        if (patch["mesh_state"].contains("vert")) mesh_vert = patch["mesh_state"]["vert"].get<bool>();
        // Force geometry update flag reset
        lastGridSize = -1;
    }

    // Load FX Chain order
    if (patch.contains("fx_chain") && patch["fx_chain"].is_array()) {
        fx_chain.clear();
        for (auto& item : patch["fx_chain"]) {
            if (item.is_string()) {
                fx_chain.push_back(item.get<std::string>());
            }
        }
    }
}

MoshRackEngine::~MoshRackEngine() {
    // Clean up MIDI
    if (m_midiIn != nullptr) {
        delete m_midiIn;
        m_midiIn = nullptr;
    }

    // Clean up audio device
    StopAudioDevice();

    // Clean up audio context
    if (m_audioContextInitialized) {
        ma_context_uninit(&m_audioContext);
        m_audioContextInitialized = false;
    }
}


void MoshRackEngine::EnumerateWindowsCameras(std::vector<DeviceInfo>& devices, std::vector<std::string>& source_names) {
    devices.clear();
    source_names.clear();

    ICreateDevEnum* pSysDevEnum = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pSysDevEnum);
    if (FAILED(hr)) {
        devices.push_back({ 0, "Default Camera" });
        source_names.push_back("Default Camera");
        return;
    }

    IEnumMoniker* pEnumCat = nullptr;
    hr = pSysDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumCat, 0);
    if (hr == S_OK && pEnumCat != nullptr) {
        IMoniker* pMoniker = nullptr;
        int index = 0;
        while (pEnumCat->Next(1, &pMoniker, NULL) == S_OK) {
            IPropertyBag* pPropBag = nullptr;
            hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);
            if (SUCCEEDED(hr)) {
                VARIANT var;
                VariantInit(&var);
                hr = pPropBag->Read(L"FriendlyName", &var, 0);
                if (SUCCEEDED(hr)) {
                    std::wstring wstr(var.bstrVal);
                    std::string name(wstr.begin(), wstr.end());

                    // Clever validation: Test-open with OpenCV using CAP_DSHOW 
                    // to guarantee the index matches and the device isn't a phantom/offline stream.
                    cv::VideoCapture test_cap;
                    test_cap.open(index, cv::CAP_DSHOW);

                    if (test_cap.isOpened()) {
                        devices.push_back({ index, name });
                        source_names.push_back(name + " (Index " + std::to_string(index) + ")");
                        test_cap.release();
                    }
                    else {
                        // Even if it fails test-open, you can still list it if preferred, 
                        // but filtering keeps your dropdown clean of disconnected virtual drivers.
                        devices.push_back({ index, name + " [Offline]" });
                        source_names.push_back(name + " [Offline] (Index " + std::to_string(index) + ")");
                    }

                    VariantClear(&var);
                }
                pPropBag->Release();
            }
            pMoniker->Release();
            index++;
        }
        pEnumCat->Release();
    }
    if (pSysDevEnum) pSysDevEnum->Release();

    if (devices.empty()) {
        devices.push_back({ 0, "Default Camera" });
        source_names.push_back("Default Camera (Index 0)");
    }
}
#else
void MoshRackEngine::EnumerateWindowsCameras(std::vector<DeviceInfo>& devices, std::vector<std::string>& source_names) {
    // Non-Windows stub
}
#endif

std::vector<MoshRackEngine::DeviceInfo> MoshRackEngine::GetAvailableCameras() {
    std::cout << "[Diagnostics Engine] GetAvailableCameras() ENTERED." << std::endl;

    std::vector<DeviceInfo> devices;
    std::vector<std::string> dummy_source_names;

#ifdef _WIN32
    std::cout << "[Diagnostics Engine] Windows block active. Calling EnumerateWindowsCameras..." << std::endl;

    // Note: Ensure this has VideoWorker:: scope if it's defined in the VideoWorker class
    EnumerateWindowsCameras(devices, dummy_source_names);

    std::cout << "[Diagnostics Engine] Windows enumeration finished. Devices found: " << devices.size() << std::endl;
#elif defined(__APPLE__)
    std::cout << "[Diagnostics Engine] macOS block active. Scanning via OpenCV..." << std::endl;
    for (int i = 0; i < 4; i++) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            devices.push_back({ i, "Camera " + std::to_string(i) });
            std::cout << "[Diagnostics Engine] Found camera at index " << i << std::endl;
            cap.release();
        }
    }
    if (devices.empty()) {
        std::cout << "[Diagnostics Engine] No cameras found. Falling back to default." << std::endl;
        devices.push_back({ 0, "Default Camera" });
    }
#else
    std::cout << "[Diagnostics Engine] Linux/Other block active. Scanning via OpenCV..." << std::endl;
    for (int i = 0; i < 4; i++) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            devices.push_back({ i, "Camera " + std::to_string(i) });
            std::cout << "[Diagnostics Engine] Found camera at index " << i << std::endl;
            cap.release();
        }
    }
    if (devices.empty()) {
        std::cout << "[Diagnostics Engine] No cameras found. Falling back to default." << std::endl;
        devices.push_back({ 0, "Default Camera" });
    }
#endif

    std::cout << "[Diagnostics Engine] GetAvailableCameras() EXIT. Returning " << devices.size() << " device(s)." << std::endl;
    return devices;
}




// Real-time MIDI message callback
void MoshRackEngine::MidiCallback(double deltatime, std::vector<unsigned char>* message, void* userData) {
    if (message->empty() || message->size() < 3) return;

    MoshRackEngine* engine = static_cast<MoshRackEngine*>(userData);
    if (!engine) return;

    unsigned char status = message->at(0);
    int type = status & 0xF0; // Mask out the MIDI channel nibble
    int data1 = (int)message->at(1);
    int data2 = (int)message->at(2);

    // --- 1. MIDI MAPPING (LEARN) LOGIC ---
    if (engine->is_learning_midi) {
        if (type == 0xB0) { // Only map Control Change messages for now
            engine->cc_to_param[data1] = engine->param_to_learn;
            std::cout << "[MOSHRACK] Mapped CC " << data1 << " to " << engine->param_to_learn << std::endl;

            engine->is_learning_midi = false;
            engine->param_to_learn = "";

            engine->SaveMidiMap("midi_map_" + engine->active_midi_controller_name + ".txt");
            engine->SaveMidiMap("midi_map_last_used.txt");
        }
        return; // Stop processing so the UI doesn't jump while you're mapping
    }

    // --- 2. NORMAL RUNTIME ROUTING ---
    std::lock_guard<std::mutex> lock(engine->engine_mutex);

    if (type == 0xB0) {
        // Control Change (Knobs)
        engine->SetParamFromCC(data1, data2);
    }
    else if (type == 0x90) {
        // Note On (Pads)
        engine->SetParamFromNote(data1, data2);
    }
    else if (type == 0x80) {
        // Note Off (Pads release) - Pass 0 velocity to trigger release logic
        engine->SetParamFromNote(data1, 0);
    }
}
void MoshRackEngine::SetParamFromNote(int note, int velocity) {
    if (velocity <= 0) return; // Ignore Note Off (release) messages
    if (note_to_param.find(note) == note_to_param.end()) return;

    std::string paramId = note_to_param[note];

    // --- CORE MODULE TOGGLES ---
    if (paramId == "LFO_TOGGLE" || paramId == "LFO") {
        bool newState = !fx_enabled["LFO"];
        fx_enabled["LFO"] = newState;
        lfo_active = newState;
    }
    else if (paramId == "MOSH_TOGGLE" || paramId == "MOSH")         fx_enabled["MOSH"] = !fx_enabled["MOSH"];
    else if (paramId == "FLOW_TOGGLE" || paramId == "FLOW")         fx_enabled["FLOW"] = !fx_enabled["FLOW"];
    else if (paramId == "KSCOPE_TOGGLE" || paramId == "KSCOPE")     fx_enabled["KSCOPE"] = !fx_enabled["KSCOPE"];
    else if (paramId == "CHROMA_TOGGLE" || paramId == "CHROMA")     fx_enabled["CHROMA"] = !fx_enabled["CHROMA"];
    else if (paramId == "MESH_TOGGLE" || paramId == "MESH") fx_enabled["MESH"] = !fx_enabled["MESH"];
    else if (paramId == "RGB_TOGGLE" || paramId == "RGB")           fx_enabled["RGB"] = !fx_enabled["RGB"];

    // --- MUTUALLY EXCLUSIVE KEY TOGGLES ---
    else if (paramId == "LUMA_KEY_TOGGLE" || paramId == "LUMA_KEY") {
        bool newState = !fx_enabled["LUMA_KEY"];
        fx_enabled["LUMA_KEY"] = newState;
        if (newState) fx_enabled["CHROMA_KEY"] = false;
    }
    else if (paramId == "CHROMA_KEY_TOGGLE" || paramId == "CHROMA_KEY") {
        bool newState = !fx_enabled["CHROMA_KEY"];
        fx_enabled["CHROMA_KEY"] = newState;
        if (newState) fx_enabled["LUMA_KEY"] = false;
    }

    // --- MESH SUB-TOGGLES ---
    else if (paramId == "MESH_BW")     fx_enabled["MESH_BW"] = !fx_enabled["MESH_BW"];
    else if (paramId == "MESH_INVERT") fx_enabled["MESH_INVERT"] = !fx_enabled["MESH_INVERT"];
    else if (paramId == "MESH_LUMA")   fx_enabled["MESH_LUMA"] = !fx_enabled["MESH_LUMA"];

    // --- UTILITIES & TRIGGERS ---
    else if (paramId == "SPLASH_TOGGLE") splash_blend = (splash_blend > 0.5f) ? 0.0f : 1.0f;
    else if (paramId == "FREEZE")        is_paused = !is_paused;
    else if (paramId == "TAP_TEMPO")     tap_tempo();
}
// Route CC to the mapped parameter and scale values correctly
void MoshRackEngine::SetParamFromCC(int cc, int value) {
    if (cc_to_param.find(cc) == cc_to_param.end()) return;
    std::string paramId = cc_to_param[cc];
    float norm = static_cast<float>(value) / 127.0f;

    // Toggles (boolean values - latching on press, ignoring release)
    if (paramId.find("_TOGGLE") != std::string::npos) {
        if (value < 64) return; // Ignore MIDI release messages for momentary pads

        // Utilities
        if (paramId == "SPLASH_TOGGLE") {
            splash_blend = (splash_blend > 0.5f) ? 0.0f : 1.0f;
        }
        else if (paramId == "LFO_TOGGLE") {
            bool newState = !fx_enabled["LFO"];
            fx_enabled["LFO"] = newState;
            lfo_active = newState;
        }

        // Spatial & Color Toggles
        else if (paramId == "ZOOM_TOGGLE") {
            bool newState = !fx_enabled["ZOOM"];
            fx_enabled["ZOOM"] = newState;
            fx_enabled["TRANSFORM_3D"] = newState;
        }
        else if (paramId == "SPACE_TOGGLE")  fx_enabled["TRANSFORM_3D"] = !fx_enabled["TRANSFORM_3D"];
        else if (paramId == "RGB_TOGGLE")    fx_enabled["RGB"] = !fx_enabled["RGB"];

        // Mutually Exclusive Key Toggles
        else if (paramId == "LUMA_KEY_TOGGLE") {
            bool newState = !fx_enabled["LUMA_KEY"];
            fx_enabled["LUMA_KEY"] = newState;
            if (newState) fx_enabled["CHROMA_KEY"] = false;
        }
        else if (paramId == "CHROMA_KEY_TOGGLE") {
            bool newState = !fx_enabled["CHROMA_KEY"];
            fx_enabled["CHROMA_KEY"] = newState;
            if (newState) fx_enabled["LUMA_KEY"] = false;
        }

        // Core VFX Modules
        else if (paramId == "MOSH_TOGGLE")   fx_enabled["MOSH"] = !fx_enabled["MOSH"];
        else if (paramId == "FLOW_TOGGLE")   fx_enabled["FLOW"] = !fx_enabled["FLOW"];
        else if (paramId == "CHROMA_TOGGLE") fx_enabled["CHROMA"] = !fx_enabled["CHROMA"];
        else if (paramId == "KSCOPE_TOGGLE") fx_enabled["KSCOPE"] = !fx_enabled["KSCOPE"];
        else if (paramId == "MESH_TOGGLE")   fx_enabled["MESH"] = !fx_enabled["MESH"];

        return;
    }

    // Knobs & Sliders (Float values mapped to exact parameter ranges)
    if (paramId == "SPLASH_FADE") splash_blend = norm * 1.0f;
    else if (paramId == "wet_dry") wet_dry_mix = norm * 1.0f;

    // LFO & Utils
    else if (paramId == "LFO_RATE_KNOB") lfo_rate = 0.1f + norm * (5.0f - 0.1f);
    else if (paramId == "LFO_GAIN_KNOB") lfo_gain = norm * 1.0f;
    else if (paramId == "LFO_BIAS_KNOB") lfo_bias = -0.5f + norm * 1.0f;
    else if (paramId == "SLOWMO_AMT_KNOB") current_fx_gains["SLOWMO"] = norm * 1.0f;

    // Mosh & Flow
    else if (paramId == "MOSH") current_fx_gains["MOSH"] = norm * 2.2f;
    else if (paramId == "MOSH_DECAY") current_fx_gains["MOSH_DECAY"] = norm * 1.0f;
    else if (paramId == "MOSH_RNBW") mosh_wet_dry = norm * 5.0f;
    else if (paramId == "FLOW_AMT_KNOB") current_fx_gains["FLOW"] = norm * 3.5f;
    else if (paramId == "FLOW_HUE_KNOB") current_fx_gains["FLOW_HUE"] = norm * 1.0f;
    else if (paramId == "FLOW_SAT_KNOB") current_fx_gains["FLOW_SAT"] = norm * 1.0f;

    // Chroma & Color Knobs
    else if (paramId == "CHROMA_DEPTH_KNOB") current_fx_gains["CHROMA"] = -1.0f + norm * 2.0f;
    else if (paramId == "CHROMA_WARP_KNOB") current_fx_gains["CHROMA_WARP"] = norm * 10.0f;

    // RGB Bipolar (-1.0 to 1.0) & Keying (0.0 to 1.0)
    else if (paramId == "RGB_RED_KNOB") rgb_gains[0] = -1.0f + (norm * 2.0f);
    else if (paramId == "RGB_GREEN_KNOB") rgb_gains[1] = -1.0f + (norm * 2.0f);
    else if (paramId == "RGB_BLUE_KNOB") rgb_gains[2] = -1.0f + (norm * 2.0f);
    else if (paramId == "KEY_THRESH_KNOB") key_amount = norm * 1.0f;

    // Zoom & 3D Spatial
    else if (paramId == "ZOOM_KNOB") current_fx_gains["ZOOM"] = -1.0f + norm * 2.0f;
    else if (paramId == "ROT_X_KNOB") current_fx_gains["ROT_X"] = -90.0f + (norm * 180.0f); // -90 to +90 degrees
    else if (paramId == "ROT_Y_KNOB") current_fx_gains["ROT_Y"] = -90.0f + (norm * 180.0f); // -90 to +90 degrees
    else if (paramId == "POS_X_KNOB") current_fx_gains["POS_X"] = -1.0f + (norm * 2.0f); // Bipolar X Pan
    else if (paramId == "POS_Y_KNOB") current_fx_gains["POS_Y"] = -1.0f + (norm * 2.0f); // Bipolar Y Pan

    // Mesh
    else if (paramId == "MESH_H_KNOB") current_fx_gains["MESH_H"] = -1.0f + norm * 2.0f;
    else if (paramId == "MESH_V_KNOB") current_fx_gains["MESH_V"] = -1.0f + norm * 2.0f;
    else if (paramId == "MESH_Z_KNOB") current_fx_gains["MESH_Z"] = -1.0f + norm * 2.0f;

    // Kaleidoscope
    else if (paramId == "KSCOPE_ROT_KNOB") current_fx_gains["KSCOPE_ROT"] = -10.0f + norm * 20.0f;
    else if (paramId == "KSCOPE_OFFSET_KNOB") current_fx_gains["KSCOPE_OFFSET"] = norm * 1.0f;
    else if (paramId == "KSCOPE_BLUR_KNOB") current_fx_gains["KSCOPE_BLUR"] = norm * 1.0f;
    else if (paramId == "KSCOPE_ZOOM_KNOB") current_fx_gains["KSCOPE_ZOOM"] = 0.5f + norm * 1.5f;
}

// Hardcoded default layout for Arturia BeatStep Pro
void MoshRackEngine::LoadBeatStepProDefaults() {
    cc_to_param.clear();
    note_to_param.clear();

    // --- ARTURIA BEATSTEP PRO ENCODERS (KNOBS / CC) ---
    cc_to_param[10] = "SPLASH_FADE";         // Encoder 1: Crossfade Splash/Video
    cc_to_param[16] = "LFO_RATE_KNOB";       // Encoder 2: LFO Speed
    cc_to_param[17] = "LFO_GAIN_KNOB";       // Encoder 3: LFO Amplitude
    cc_to_param[18] = "MOSH";                // Encoder 4: Mosh Intensity
    cc_to_param[19] = "MOSH_DECAY";          // Encoder 5: Motion Gate / Decay
    cc_to_param[71] = "FLOW_AMT_KNOB";       // Encoder 6: Optical Flow Amount
    cc_to_param[72] = "FLOW_SAT_KNOB";       // Encoder 7: Optical Flow Gloss/Sat
    cc_to_param[73] = "KSCOPE_ROT";          // Encoder 8: KScope Rotation Speed
    cc_to_param[74] = "KSCOPE_SEGMENTS";     // Encoder 9: KScope Segments
    cc_to_param[75] = "KSCOPE_OFFSET";       // Encoder 10: KScope Phase Offset
    cc_to_param[76] = "CHROMA_DEPTH_KNOB";   // Encoder 11: Chroma Dispersion Depth
    cc_to_param[77] = "CHROMA_WARP_KNOB";    // Encoder 12: Chroma Warp Speed
    cc_to_param[79] = "MESH_SCALE_KNOB";     // Encoder 13: Mesh Resolution
    cc_to_param[91] = "ZOOM_KNOB";           // Encoder 14: Master Zoom Scale
    cc_to_param[93] = "ROT_X_KNOB";          // Encoder 15: 3D X-Axis Tilt
    cc_to_param[114] = "RGB_RED_KNOB";        // Encoder 16: Red Bipolar Gain

    // --- ARTURIA BEATSTEP PRO PADS (TOGGLES / NOTES 36-50) ---
    note_to_param[36] = "LFO_TOGGLE";          // Pad 1: LFO ON/OFF
    note_to_param[37] = "MOSH_TOGGLE";         // Pad 2: MOSH ON/OFF
    note_to_param[38] = "FLOW_TOGGLE";         // Pad 3: FLOW ON/OFF
    note_to_param[39] = "KSCOPE_TOGGLE";       // Pad 4: KSCOPE ON/OFF
    note_to_param[40] = "CHROMA_TOGGLE";       // Pad 5: CHROMA ON/OFF
    note_to_param[41] = "MESH_TOGGLE";         // Pad 6: MESH ON/OFF
    note_to_param[42] = "RGB_TOGGLE";          // Pad 7: RGB ON/OFF
    note_to_param[43] = "LUMA_KEY_TOGGLE";     // Pad 8: LUMA KEY ON/OFF
    note_to_param[44] = "CHROMA_KEY_TOGGLE";   // Pad 9: CHROMA KEY ON/OFF
    note_to_param[45] = "MESH_BW";             // Pad 10: Mesh B&W
    note_to_param[46] = "MESH_INVERT";         // Pad 11: Mesh Invert
    note_to_param[47] = "MESH_LUMA";           // Pad 12: Mesh Luma Key
    note_to_param[48] = "SPLASH_TOGGLE";       // Pad 13: Splash Screen Toggle
    note_to_param[49] = "FREEZE";              // Pad 14: Freeze Video Frame
    note_to_param[50] = "TAP_TEMPO";           // Pad 15: Tap Tempo Trigger
}

// Save MIDI map to file based on controller name
void MoshRackEngine::SaveMidiMap(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return;
    file << "controller=" << active_midi_controller_name << "\n";
    for (const auto& pair : cc_to_param) {
        file << pair.first << "=" << pair.second << "\n";
    }
}

// Load MIDI map from file
void MoshRackEngine::LoadMidiMap(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;
    cc_to_param.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("controller=", 0) == 0) {
            active_midi_controller_name = line.substr(11);
            continue;
        }
        size_t delimiter = line.find('=');
        if (delimiter != std::string::npos) {
            int cc = std::stoi(line.substr(0, delimiter));
            std::string param = line.substr(delimiter + 1);
            cc_to_param[cc] = param;
        }
    }
}

// 1. The Low-Latency Audio Callback (Fires automatically as audio plays)
static void audio_loopback_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    if (!pInput || frameCount == 0) {
        if (pOutput) std::memset(pOutput, 0, frameCount * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
        return;
    }

    MoshRackEngine* engine = static_cast<MoshRackEngine*>(pDevice->pUserData);
    if (!engine) return;

    const float* samples = static_cast<const float*>(pInput);
    int totalSamples = frameCount * pDevice->capture.channels;

    float sumSquares = 0.0f;
    for (int i = 0; i < totalSamples; ++i) {
        sumSquares += samples[i] * samples[i]; // Fixed: properly square the samples
    }
    float rms = std::sqrt(sumSquares / totalSamples);

    engine->current_audio_rms.store(rms, std::memory_order_relaxed);

    // Ensure audio_input_gain defaults to 1.0f in your engine initialization!
    float scaledRms = rms * engine->audio_input_gain;
    engine->audio_level_bass.store(scaledRms * 1.3f, std::memory_order_relaxed);
    engine->audio_level_mid.store(scaledRms * 1.0f, std::memory_order_relaxed);
    engine->audio_level_high.store(scaledRms * 0.7f, std::memory_order_relaxed);

    //TODO: test and approve the --- BEATSYNC PEAK DETECTOR ---
    if (engine->lfo_beat_sync_active.load(std::memory_order_relaxed)) {
        // Much slower adaptation rate (e.g., 0.99f) so the baseline stays steady 
        // across fast buffer callbacks and doesn't swallow transients instantly.
        static float rolling_avg = 0.05f;
        rolling_avg = rolling_avg * 0.99f + scaledRms * 0.01f;

        static auto last_peak_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        float time_since_last = std::chrono::duration<float>(now - last_peak_time).count();

        // Tuned thresholds (adjust 0.015f lower if desktop loopback audio runs quiet)
        if (scaledRms > rolling_avg * 1.3f && scaledRms > 0.015f && time_since_last > 0.15f) {
            last_peak_time = now;
            engine->register_sync_beat(now);
        }
    }

    if (pOutput) {
        std::memset(pOutput, 0, frameCount * ma_get_bytes_per_frame(pDevice->playback.format, pDevice->playback.channels));
    }
}
// Add to MoshRackEngine.h:
// void ConnectAudioLoopback();

void MoshRackEngine::ConnectAudioLoopback() {
    // Stop and uninitialize the current device if it's running
    if (is_audio_initialized) {
        ma_device_uninit(&audio_device);
        is_audio_initialized = false;
    }

    // Configure for loopback (capturing desktop audio)
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.pDeviceID = NULL; // NULL tells miniaudio to use the default system output
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = 2;
    deviceConfig.sampleRate = 48000;

    // Assign YOUR new callback here!
    deviceConfig.dataCallback = audio_loopback_callback;
    deviceConfig.pUserData = this;

    if (ma_device_init(NULL, &deviceConfig, &audio_device) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to initialize loopback device." << std::endl;
        return;
    }

    if (ma_device_start(&audio_device) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to start loopback device." << std::endl;
        ma_device_uninit(&audio_device);
        return;
    }

    is_audio_initialized = true;
    std::cout << "[Audio] Successfully hooked into default desktop loopback!" << std::endl;
}
// 2. Initialize Default Desktop Audio Loopback on Startup
void MoshRackEngine::InitDefaultLoopback() {
    if (m_isAudioRunning) {
        StopAudioDevice();
    }

    if (!m_audioContextInitialized) return;

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_loopback);
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = 2; // Stereo desktop audio
    deviceConfig.sampleRate = 48000;
    deviceConfig.dataCallback = audio_loopback_callback;
    deviceConfig.pUserData = this;

    // Defensively initialize the loopback device using default system playback device
    if (ma_device_init(&m_audioContext, &deviceConfig, &m_audioDevice) == MA_SUCCESS) {
        if (ma_device_start(&m_audioDevice) == MA_SUCCESS) {
            m_isAudioRunning = true;
            std::cout << "[Audio] Successfully initialized default desktop audio loopback." << std::endl;
            return;
        }
        else {
            ma_device_uninit(&m_audioDevice);
        }
    }
    std::cerr << "[Audio Warning] Failed to start default desktop loopback. Falling back gracefully." << std::endl;
}

//  Cleanly Stop Active Audio Stream
void MoshRackEngine::StopAudioDevice() {
    if (m_isAudioRunning) {
        ma_device_uninit(&m_audioDevice);
        m_isAudioRunning = false;
    }
}




void MoshRackEngine::build_grids(int w, int h) {
    grid_x = cv::Mat(h, w, CV_32FC1);
    grid_y = cv::Mat(h, w, CV_32FC1);

    for (int y = 0; y < h; ++y) {
        float* x_ptr = grid_x.ptr<float>(y);
        float* y_ptr = grid_y.ptr<float>(y);
        for (int x = 0; x < w; ++x) {
            x_ptr[x] = static_cast<float>(x);
            y_ptr[x] = static_cast<float>(y);
        }
    }
}

// 2. Scan for MIDI controllers (like your BeatStep Pro)



void MoshRackEngine::ConnectAudioDevice(int index) {
    StopAudioDevice();

    if (!m_audioContextInitialized) return;

    // Validate index against our unified list
    if (index < 0 || index >= static_cast<int>(availableAudioDevices.size())) {
        InitDefaultLoopback();
        return;
    }

    const auto& targetDev = availableAudioDevices[index];
    ma_device_config deviceConfig;

    if (targetDev.isInput) {
        // Configure for hardware microphone / line-in capture
        deviceConfig = ma_device_config_init(ma_device_type_capture);
        deviceConfig.capture.pDeviceID = &targetDev.maId;
    }
    else {
        // Configure for desktop audio loopback from the selected output device
        deviceConfig = ma_device_config_init(ma_device_type_loopback);
        deviceConfig.capture.pDeviceID = &targetDev.maId; // Loopback uses capture config mapped to playback ID
    }

    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = 2;
    deviceConfig.sampleRate = 48000;
    deviceConfig.dataCallback = audio_loopback_callback;
    deviceConfig.pUserData = this;

    if (ma_device_init(&m_audioContext, &deviceConfig, &m_audioDevice) == MA_SUCCESS) {
        if (ma_device_start(&m_audioDevice) == MA_SUCCESS) {
            m_isAudioRunning = true;
            std::cout << "[Audio] Connected to " << (targetDev.isInput ? "Input" : "Output Loopback")
                << " device: " << targetDev.name << std::endl;
            return;
        }
        ma_device_uninit(&m_audioDevice);
    }

    // Fallback if custom device selection fails
    InitDefaultLoopback();
}


void MoshRackEngine::PollAudioDevices() {
    availableAudioDevices.clear();

    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        std::cerr << "[Audio Error] Failed to initialize miniaudio context." << std::endl;
        return;
    }

    ma_device_info* pPlaybackInfos = nullptr;
    ma_uint32 playbackCount = 0;
    ma_device_info* pCaptureInfos = nullptr;
    ma_uint32 captureCount = 0;

    if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) == MA_SUCCESS) {
        int unifiedIndex = 0;

        // 1. Add Playback (Outputs -> Tagged as [Output])
        for (ma_uint32 i = 0; i < playbackCount; ++i) {
            AudioDeviceInfo dev;
            dev.id = unifiedIndex++;
            dev.name = "[Output] " + std::string(pPlaybackInfos[i].name);
            dev.isInput = false;
            dev.maId = pPlaybackInfos[i].id;
            
            availableAudioDevices.push_back(dev);
        }

        // 2. Add Capture (Inputs -> Tagged as [Input])
        for (ma_uint32 i = 0; i < captureCount; ++i) {
            AudioDeviceInfo dev;
            dev.id = unifiedIndex++;
            dev.name = "[Input] " + std::string(pCaptureInfos[i].name);
            dev.isInput = true;
            dev.maId = pCaptureInfos[i].id;
            
            availableAudioDevices.push_back(dev);
        }
    }

    ma_context_uninit(&context);
}

void MoshRackEngine::PollMidiDevices() {
    availableMidiDevices.clear();

    try {
#ifdef _WIN32
        // Force RtMidi to explicitly use the Windows Multimedia API on Windows
        RtMidiIn midiQuery(RtMidi::WINDOWS_MM);
#else
        // Let Mac and Linux auto-detect CoreMIDI or ALSA/JACK
        RtMidiIn midiQuery;
#endif
        unsigned int nPorts = midiQuery.getPortCount();

        for (unsigned int i = 0; i < nPorts; ++i) {
            std::string portName = midiQuery.getPortName(i);
            availableMidiDevices.push_back({ static_cast<int>(i), portName });
        }
    }
    catch (RtMidiError& error) {
        error.printMessage();
    }

    if (availableMidiDevices.empty()) {
        availableMidiDevices.push_back({ -1, "No MIDI Devices Found" });
    }
}

//  Connect to a selected MIDI device
void MoshRackEngine::ConnectMidiDevice(int index) {
    // 1. Completely destroy the old connection if it exists
    if (m_midiIn) {
        m_midiIn->closePort();
        delete m_midiIn;
        m_midiIn = nullptr;
    }

    if (index >= 0 && index < static_cast<int>(availableMidiDevices.size())) {
        try {
            // 2. Rebuild with cross-platform OS checks 
#ifdef _WIN32
            m_midiIn = new RtMidiIn(RtMidi::WINDOWS_MM);
#else
            m_midiIn = new RtMidiIn();
#endif

            std::string portName = availableMidiDevices[index].name;
            m_midiIn->openPort(index);

            // 3. Set callback and print a success message so you KNOW it reached here
            m_midiIn->setCallback(&MoshRackEngine::MidiCallback, this);
            m_midiIn->ignoreTypes(false, false, false); // Accept all MIDI messages

            std::cout << "[MOSHRACK] Successfully hooked callback to: " << portName << std::endl;

            active_midi_controller_name = portName;

            // Auto-detect Arturia BeatStep Pro
            if (portName.find("BeatStep Pro") != std::string::npos || portName.find("Arturia BeatStep Pro") != std::string::npos) {
                LoadBeatStepProDefaults();
            }
            else {
                std::string filename = "midi_map_" + portName + ".txt";
                std::ifstream testFile(filename);
                if (testFile.is_open()) {
                    LoadMidiMap(filename);
                }
            }
            SaveMidiMap("midi_map_last_used.txt");
        }
        catch (RtMidiError& error) {
            error.printMessage();
        }
    }
}
//  fetch audio reactivity values:


float MoshRackEngine::get_mod(const std::string& param_name) {
    // 1. Fetch base value from current_fx_gains, arrays, or audio variables
    float base_val = 0.0f;

    if (param_name == "AUDIO_RMS" || param_name == "AUDIO") {
        base_val = current_audio_rms.load(std::memory_order_relaxed) * audio_input_gain;
    }
    else if (param_name == "RGB_R" || param_name == "RGB_RED") {
        base_val = rgb_gains[0];
    }
    else if (param_name == "RGB_G" || param_name == "RGB_GREEN") {
        base_val = rgb_gains[1];
    }
    else if (param_name == "RGB_B" || param_name == "RGB_BLUE") {
        base_val = rgb_gains[2];
    }
    else if (param_name == "KEY_THRESH" || param_name == "KEY") {
        base_val = key_amount;
    }
    else {
        auto it = current_fx_gains.find(param_name);
        if (it != current_fx_gains.end()) {
            base_val = it->second;
        }
    }

    // 2. Map engine parameter names to their exact patch bay target jacks
    std::string target_key = "";
    if (param_name == "MOSH")           target_key = "TGT_MOSH_GAIN";
    else if (param_name == "MOSH_DECAY")     target_key = "TGT_MOSH_DECAY";
    else if (param_name == "MOSH_RNBW")      target_key = "TGT_MOSH_RNBW";

    else if (param_name == "CHROMA")         target_key = "TGT_CHROMA_AMT";
    else if (param_name == "CHROMA_WARP")    target_key = "TGT_CHROMA_WARP";

    else if (param_name == "KSCOPE_ROT")     target_key = "TGT_KSCOPE_ROT";
    else if (param_name == "KSCOPE_OFFSET")  target_key = "TGT_KSCOPE_OFF";
    else if (param_name == "KSCOPE_ZOOM")    target_key = "TGT_KSCOPE_ZOM";

    else if (param_name == "FLOW")           target_key = "TGT_FLOW_AMT";
    else if (param_name == "FLOW_HUE")       target_key = "TGT_FLOW_HUE";
    else if (param_name == "FLOW_SAT")       target_key = "TGT_FLOW_SAT";

    else if (param_name == "MESH_SCALE")     target_key = "TGT_MESH_SCL";
    else if (param_name == "MESH_H")         target_key = "TGT_MESH_X";
    else if (param_name == "MESH_V")         target_key = "TGT_MESH_Y";
    else if (param_name == "MESH_Z")         target_key = "TGT_MESH_Z";

    else if (param_name == "ZOOM")           target_key = "TGT_ZOOM";
    else if (param_name == "ROT_X")          target_key = "TGT_ROT_X";
    else if (param_name == "ROT_Y")          target_key = "TGT_ROT_Y";
    else if (param_name == "POS_X")          target_key = "TGT_POS_X";
    else if (param_name == "POS_Y")          target_key = "TGT_POS_Y";

    else if (param_name == "RGB_R" || param_name == "RGB_RED") target_key = "TGT_RGB_R";
    else if (param_name == "RGB_G" || param_name == "RGB_GREEN") target_key = "TGT_RGB_G";
    else if (param_name == "RGB_B" || param_name == "RGB_BLUE") target_key = "TGT_RGB_B";
    else {
        target_key = "TGT_" + param_name; // Safe fallback prefix
    }
    // 3. Look up connected source ID from the patch matrix
    std::string source_id = "";
    if (!target_key.empty() && patch_matrix.find(target_key) != patch_matrix.end()) {
        source_id = patch_matrix[target_key];
    }
    else if (patch_matrix.find("TGT_" + param_name) != patch_matrix.end()) {
        source_id = patch_matrix["TGT_" + param_name];
    }
    // 4. Apply modulation if a patch cable is connected
    if (!source_id.empty()) {
        float mod_val = 0.0f;
        if (source_id == "SRC_BASS")          mod_val = audio_level_bass.load(std::memory_order_relaxed);
        else if (source_id == "SRC_MID")      mod_val = audio_level_mid.load(std::memory_order_relaxed);
        else if (source_id == "SRC_HI")       mod_val = audio_level_high.load(std::memory_order_relaxed);
        else if (source_id == "SRC_LFO")      mod_val = audio_level_lfo.load(std::memory_order_relaxed);
        else if (source_id == "SRC_PITCH")    mod_val = audio_pitch_val;
        else if (source_id == "SRC_VELOCITY") mod_val = audio_velocity;

        // Scale raw audio signals so they physically move macro parameters
        if (param_name == "KSCOPE_ROT") {
            mod_val *= 10.0f;
        }
        else if (param_name == "ROT_X" || param_name == "ROT_Y") {
            mod_val *= 45.0f; // Audio transients swing the 3D plane up to 45 degrees
        }
        else if (param_name == "MESH_SCALE") {
            mod_val *= 20.0f; // Audio actively shifts the grid resolution
        }

        // Apply the pink GUI sensitivity ghost ring
        float sensitivity = 1.0f;
        if (module_sensitivities.find(target_key) != module_sensitivities.end()) {
            sensitivity = module_sensitivities[target_key];
        }

        return base_val + (mod_val * sensitivity);
    }

    return base_val;
}


cv::Mat MoshRackEngine::process_frame(cv::Mat& frame, bool is_new_frame) {
    // --- 1. THREAD-SAFE LOCK ---
    std::lock_guard<std::mutex> lock(engine_mutex);

    // --- 2. ALWAYS TICK SCREENSAVER (NEVER STOPS) ---
    splash_system.update_physics();

    // --- 3. HANDLE NO VIDEO / FILE PICKER STATE ---
    if (frame.empty()) {
        if (splash_blend > 0.0f) {
            // No video is loaded, but splash is active: return the screensaver
            return splash_system.get_active_layer(cv::Size(640, 480), CV_8UC3);
        }
        // No video and no splash: return a black frame to prevent crashing
        return cv::Mat::zeros(480, 640, CV_8UC3);
    }

    // --- 4. VIDEO EXISTS: SETUP ---
    int w = frame.cols;
    int h = frame.rows;

    if (w != cached_w || h != cached_h) {
        cached_w = w;
        cached_h = h;
        build_grids(w, h);
    }

    cv::Mat current_frame;
    cv::Mat dry_frame = frame.clone(); // Untouched original video frame

    // --- 5. RUN VIDEO VFX (ONLY IF NOT PAUSED AND NEW FRAME ARRIVED) ---
    if (is_new_frame && !is_paused) {
        current_frame = frame.clone();
        bool warp_applied = false;

        // Safely copy fx_chain 
        std::vector<std::string> safe_chain = fx_chain;

        // Helper lambda to check if an effect has active patchbay cables connected
        auto is_effect_patched = [&](const std::string& name) {
            for (const auto& pair : patch_matrix) {
                const std::string& tgt = pair.first;
                if (name == "MOSH" && tgt.rfind("TGT_MOSH_", 0) == 0) return true;
                if (name == "CHROMA" && tgt.rfind("TGT_CHROMA_", 0) == 0) return true;
                if (name == "KSCOPE" && tgt.rfind("TGT_KSCOPE_", 0) == 0) return true;
                if (name == "FLOW" && tgt.rfind("TGT_FLOW_", 0) == 0) return true;
                if (name == "MESH" && tgt.rfind("TGT_MESH_", 0) == 0) return true;
                if (name == "RGB" && tgt.rfind("TGT_RGB_", 0) == 0) return true;
                if ((name == "ZOOM" || name == "TRANSFORM_3D") &&
                    (tgt == "TGT_ZOOM" || tgt == "TGT_ROTATE" || tgt == "TGT_ROT_X" ||
                        tgt == "TGT_ROT_Y" || tgt == "TGT_POS_X" || tgt == "TGT_POS_Y")) return true;
            }
            return false;
            };

        // --- VFX CHAIN WITH SMOOTH CROSSFADE TRANSITIONS ---
        for (const auto& fx_name : safe_chain) {

            bool is_active = false;
            if (fx_name == "MESH") {
                is_active = (fx_enabled.count("MESH") && fx_enabled["MESH"]) || is_effect_patched(fx_name);
            }
            else if (fx_name == "RGB") {
                is_active = fx_enabled["RGB"] || fx_enabled["LUMA_KEY"] || fx_enabled["CHROMA_KEY"] || is_effect_patched(fx_name);
            }
            else {
                is_active = (fx_enabled.count(fx_name) && fx_enabled[fx_name]) || is_effect_patched(fx_name);
            }

            float target_weight = is_active ? 1.0f : 0.0f;
            float& weight = fx_fade_weights[fx_name];

            weight += (target_weight - weight) * 0.005f;

            if (weight < 0.001f && target_weight == 0.0f) {
                continue;
            }

            cv::Mat pre_fx_frame = current_frame.clone();

            if (fx_name == "RGB") current_frame = fx_rgb(current_frame);
            else if (fx_name == "CHROMA") current_frame = fx_chroma(current_frame);
            else if (fx_name == "ZOOM") current_frame = fx_zoom(current_frame);
            else if (fx_name == "TRANSFORM_3D") current_frame = fx_transform_3d(current_frame);
            else if (fx_name == "MESH") current_frame = fx_mesh(current_frame);
            else if (fx_name == "KSCOPE") current_frame = fx_kscope(current_frame);
            else if (fx_name == "FLOW") { current_frame = fx_flow(current_frame); warp_applied = true; }
            else if (fx_name == "MOSH") {
                current_frame = fx_mosh(current_frame);
                warp_applied = true;
            }

            // --- BYPASS CROSSFADE BLENDING IF PATCHED ---
            // If the effect has an active patch cable, we skip cv::addWeighted blending 
            // so modulation hits instantly without transition damping. Intensity is governed by knobs/scroll.
            if (!is_effect_patched(fx_name) && weight < 0.999f) {
                if (pre_fx_frame.size() != current_frame.size()) {
                    cv::resize(pre_fx_frame, pre_fx_frame, current_frame.size());
                }

                if (pre_fx_frame.type() != current_frame.type()) {
                    if (current_frame.channels() == 3 && pre_fx_frame.channels() == 1) {
                        cv::cvtColor(pre_fx_frame, pre_fx_frame, cv::COLOR_GRAY2BGR);
                    }
                    else if (current_frame.channels() == 1 && pre_fx_frame.channels() == 3) {
                        cv::cvtColor(pre_fx_frame, pre_fx_frame, cv::COLOR_BGR2GRAY);
                    }
                }

                if (pre_fx_frame.size() == current_frame.size() && pre_fx_frame.type() == current_frame.type()) {
                    cv::addWeighted(pre_fx_frame, 1.0f - weight, current_frame, weight, 0.0f, current_frame);
                }
            }
        }
        prev_frame = current_frame.clone();
    }
    else {
        // Paused or not a new frame: rely on the last successfully processed frame
        if (!prev_frame.empty()) {
            current_frame = prev_frame.clone();
        }
        else {
            current_frame = frame.clone();
            prev_frame = current_frame.clone();
        }
    }

    // --- 6. APPLY MASTER WET/DRY MIX ---
    cv::Mat final_out = current_frame;

    if (wet_dry_mix < 0.99f) {
        if (wet_dry_mix <= 0.01f) {
            final_out = dry_frame;
        }
        else {
            cv::Mat dry_matched = dry_frame;
            if (dry_matched.size() != current_frame.size()) {
                cv::resize(dry_matched, dry_matched, current_frame.size());
            }
            if (dry_matched.type() != current_frame.type()) {
                if (current_frame.channels() == 1) {
                    cv::cvtColor(dry_matched, dry_matched, cv::COLOR_BGR2GRAY);
                }
                else if (dry_matched.channels() == 1) {
                    cv::cvtColor(dry_matched, dry_matched, cv::COLOR_BGR2GRAY);
                }
            }
            cv::addWeighted(current_frame, wet_dry_mix, dry_matched, 1.0f - wet_dry_mix, 0, final_out);
        }
    }

    // --- 7. BLEND SCREENSAVER OVER FINAL OUTPUT ---
    if (splash_blend > 0.0f) {
        cv::Size target_size(final_out.cols, final_out.rows);
        cv::Mat splash_frame = splash_system.get_active_layer(target_size, CV_8UC3);

        if (!splash_frame.empty()) {
            if (splash_blend >= 1.0f) {
                final_out = splash_frame.clone(); // Fully opaque splash
            }
            else {
                if (splash_frame.type() != final_out.type()) {
                    if (final_out.channels() == 3 && splash_frame.channels() == 1) {
                        cv::cvtColor(splash_frame, splash_frame, cv::COLOR_GRAY2BGR);
                    }
                    else if (final_out.channels() == 1 && splash_frame.channels() == 3) {
                        cv::cvtColor(splash_frame, splash_frame, cv::COLOR_BGR2GRAY);
                    }
                }
                cv::addWeighted(splash_frame, splash_blend, final_out, 1.0f - splash_blend, 0.0f, final_out);
            }
        }
    }

    cached_post_mix_frame = final_out.clone();
    return final_out;
}

struct Vertex {
    float x, y, z;
    float u, v;
};

cv::Mat MoshRackEngine::generateNoiseMat(float theta, float resolution, int w, int h) {
    cv::Mat noiseImg(h, w, CV_8UC1);
    float res = resolution * 0.05f;
    float t = theta * 0.1f;
    for (int y = 0; y < h; y++) {
        uchar* ptr = noiseImg.ptr<uchar>(y);
        for (int x = 0; x < w; x++) {
            float nx = x * res;
            float ny = y * res;
            // Organic multi-octave pseudo-noise
            float val = 0.5f * std::sin(nx + t) + 0.5f * std::cos(ny - t) + 0.25f * std::sin(nx * 2.0f - ny * 2.0f + t * 1.5f);
            val = std::clamp((val + 1.25f) / 2.5f, 0.0f, 1.0f);
            ptr[x] = static_cast<uchar>(val * 255.0f);
        }
    }
    return noiseImg;
}
void MoshRackEngine::updateMeshGeometry(int gridsize, bool showTriangles, bool showHoriz, bool showVert, int w, int h, GLuint& outVAO, GLuint& outVBO, int& outVertexCount, GLenum& outPrimitive) {
    std::vector<Vertex> vertices;
    int new_gridsize = (showHoriz || showVert) ? gridsize * 2 : gridsize;

    for (int i = 0; i < new_gridsize; i++) {
        for (int j = 0; j < new_gridsize; j++) {
            float x0 = static_cast<float>(j * w) / new_gridsize;
            float x1 = static_cast<float>((j + 1) * w) / new_gridsize;
            float y0 = static_cast<float>(i * h) / new_gridsize;
            float y1 = static_cast<float>((i + 1) * h) / new_gridsize;

            float tx0 = static_cast<float>(j) / new_gridsize;
            float tx1 = static_cast<float>(j + 1) / new_gridsize;
            float ty0 = static_cast<float>(i) / new_gridsize;
            float ty1 = static_cast<float>(i + 1) / new_gridsize;

            if (showTriangles) {
                vertices.push_back({ x0, y0, 0.0f, tx0, ty0 });
                vertices.push_back({ x1, y0, 0.0f, tx1, ty0 });
                vertices.push_back({ x1, y1, 0.0f, tx1, ty1 });

                vertices.push_back({ x1, y1, 0.0f, tx1, ty1 });
                vertices.push_back({ x0, y1, 0.0f, tx0, ty1 });
                vertices.push_back({ x0, y0, 0.0f, tx0, ty0 });
            }
            if (showHoriz) {
                vertices.push_back({ x0, y0, 0.0f, tx0, ty0 });
                vertices.push_back({ x1, y0, 0.0f, tx1, ty0 });
            }
            if (showVert) {
                vertices.push_back({ x0, y0, 0.0f, tx0, ty0 });
                vertices.push_back({ x0, y1, 0.0f, tx0, ty1 });
            }
        }
    }

    outVertexCount = static_cast<int>(vertices.size());
    outPrimitive = (showHoriz || showVert) ? GL_LINES : GL_TRIANGLES;

    if (outVAO == 0) glGenVertexArrays(1, &outVAO);
    if (outVBO == 0) glGenBuffers(1, &outVBO);

    glBindVertexArray(outVAO);
    glBindBuffer(GL_ARRAY_BUFFER, outVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}
void MoshRackEngine::load_splash_media(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(engine_mutex);
    splash_system.load_custom_image(filepath);
}

void MoshRackEngine::clear_splash_media() {
    std::lock_guard<std::mutex> lock(engine_mutex);
    splash_system.clear_custom_image();
}

void MoshRackEngine::tick_lfo()
{
    // Master switch check: If "LFO" is disabled in the map, turn it off
    if (!fx_enabled["LFO"])
    {
        lfo_current_value = 0.0f;
        audio_level_lfo.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // --- HIGH-RESOLUTION TIME DELTA (dt) ---
    static auto last_time = std::chrono::high_resolution_clock::now();
    auto current_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> elapsed = current_time - last_time;
    last_time = current_time;
    float dt = elapsed.count();

    if (dt > 0.1f) dt = 0.1f;

    // --- PHASE ACCUMULATION ---
    lfo_phase += 2.0f * 3.14159265f * lfo_rate * dt;
    if (lfo_phase > 2.0f * 3.14159265f)
    {
        lfo_phase -= 2.0f * 3.14159265f;
    }

    // --- SINE CALCULATION & MODULATION ---
    float sine_val = std::sin(lfo_phase);
    lfo_current_value = (sine_val * lfo_gain) + lfo_bias;

    // --- STORE RAW BIPOLAR VALUE FOR PATCH BAY ---
    // Delete the std::abs() line completely!
    audio_level_lfo.store(lfo_current_value, std::memory_order_relaxed);
}

cv::Mat MoshRackEngine::fx_lfo(const cv::Mat& input)
{
    if (input.empty())
        return input;

    // Ensure tick runs even if called as an active video effect
    tick_lfo();

    // Pass frame through cleanly
    return input.clone();
}


cv::Mat MoshRackEngine::fx_mosh(const cv::Mat& frame) {
    if (frame.empty() || videoTexture == 0) return frame;

    cv::Mat rgbFrame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    }
    else {
        rgbFrame = frame;
    }

    // 1. Static offscreen GPU resources (created once, reused every frame)
    static GLuint moshFBO = 0;
    static GLuint moshOutTexture = 0;
    static GLuint moshHistoryTexture = 0;
    static int fboWidth = 0;
    static int fboHeight = 0;

    // Initialize or resize the offscreen FBO if frame dimensions change
    if (moshFBO == 0 || fboWidth != rgbFrame.cols || fboHeight != rgbFrame.rows) {
        fboWidth = rgbFrame.cols;
        fboHeight = rgbFrame.rows;

        if (moshFBO != 0) {
            glDeleteFramebuffers(1, &moshFBO);
            glDeleteTextures(1, &moshOutTexture);
            glDeleteTextures(1, &moshHistoryTexture);
        }

        glGenFramebuffers(1, &moshFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, moshFBO);

        // Main output texture
        glGenTextures(1, &moshOutTexture);
        glBindTexture(GL_TEXTURE_2D, moshOutTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fboWidth, fboHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, moshOutTexture, 0);

        // History backbuffer texture
        glGenTextures(1, &moshHistoryTexture);
        glBindTexture(GL_TEXTURE_2D, moshHistoryTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fboWidth, fboHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // --- RESET STATE IF TOGGLED OFF ---
    if (!fx_enabled["MOSH"]) {
        cached_post_mix_frame.release();

        if (moshHistoryTexture != 0 && fboWidth > 0 && fboHeight > 0) {
            std::vector<GLubyte> blackData(fboWidth * fboHeight * 3, 0);
            glBindTexture(GL_TEXTURE_2D, moshHistoryTexture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fboWidth, fboHeight, GL_RGB, GL_UNSIGNED_BYTE, blackData.data());
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        return frame.clone();
    }

    // 2. Bind our isolated offscreen FBO
    glBindFramebuffer(GL_FRAMEBUFFER, moshFBO);
    glViewport(0, 0, fboWidth, fboHeight);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

   
   // 3. Bind Shader and push normalized uniforms matching the new GLSL spec
    moshShader.Bind();
    moshShader.SetFloat("u_MoshIntensity", get_mod("MOSH"));
    moshShader.SetFloat("u_BlockSize", get_mod("MOSH_SIZE"));
    moshShader.SetFloat("u_IFrameInterval", 0.0f);
    moshShader.SetFloat("u_MotionThreshold", get_mod("MOSH_DECAY"));
    moshShader.SetFloat("u_Shift", get_mod("MOSH_RNBW")); // Syncs to TGT_MOSH_RNBW
    moshShader.SetFloat("u_Time", static_cast<float>(ImGui::GetTime()));
    moshShader.SetVec2("u_Resolution", static_cast<float>(fboWidth), static_cast<float>(fboHeight));

    // 4. Update input video texture (u_CurrentFrame -> Texture Unit 0)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, videoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(rgbFrame.step / rgbFrame.elemSize()));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rgbFrame.cols, rgbFrame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    moshShader.SetInt("u_CurrentFrame", 0);

    // 5. Bind history backbuffer texture (u_HistoryBuffer -> Texture Unit 1)
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, moshHistoryTexture);
    moshShader.SetInt("u_HistoryBuffer", 1);

    // 6. Draw full-screen quad
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 7. Read pixels safely from the offscreen FBO texture attachment
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    cv::Mat outputFrame(fboHeight, fboWidth, CV_8UC3);
    glReadPixels(0, 0, fboWidth, fboHeight, GL_RGB, GL_UNSIGNED_BYTE, outputFrame.data);

    // 8. FEEDBACK LOOP: Update history backbuffer for the next frame
    glBindTexture(GL_TEXTURE_2D, moshHistoryTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fboWidth, fboHeight, GL_RGB, GL_UNSIGNED_BYTE, outputFrame.data);

    // Cleanup bindings
    glBindVertexArray(0);
    moshShader.Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 9. Convert back to BGR for OpenCV pipeline compatibility
    cv::Mat bgrOutput;
    cv::cvtColor(outputFrame, bgrOutput, cv::COLOR_RGB2BGR);

    return bgrOutput;
}

cv::Mat MoshRackEngine::fx_kscope(const cv::Mat& frame) {
    if (frame.empty() || videoTexture == 0) return frame;

    cv::Mat rgbFrame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    }
    else {
        rgbFrame = frame;
    }

    static GLuint kscopeFBO = 0;
    static GLuint kscopeOutTexture = 0;
    static GLuint kscopeHistoryTexture = 0;
    static int fboWidth = 0;
    static int fboHeight = 0;

    if (kscopeFBO == 0 || fboWidth != rgbFrame.cols || fboHeight != rgbFrame.rows) {
        fboWidth = rgbFrame.cols;
        fboHeight = rgbFrame.rows;

        if (kscopeFBO != 0) {
            glDeleteFramebuffers(1, &kscopeFBO);
            glDeleteTextures(1, &kscopeOutTexture);
            glDeleteTextures(1, &kscopeHistoryTexture);
        }

        glGenFramebuffers(1, &kscopeFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, kscopeFBO);

        glGenTextures(1, &kscopeOutTexture);
        glBindTexture(GL_TEXTURE_2D, kscopeOutTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fboWidth, fboHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, kscopeOutTexture, 0);

        glGenTextures(1, &kscopeHistoryTexture);
        glBindTexture(GL_TEXTURE_2D, kscopeHistoryTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fboWidth, fboHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool kscope_is_active = false;
    for (const auto& fx : fx_chain) {
        if (fx == "KSCOPE" && fx_enabled["KSCOPE"]) {
            kscope_is_active = true;
            break;
        }
    }

    if (!kscope_is_active) {
        cached_post_mix_frame.release();
        if (kscopeHistoryTexture != 0 && fboWidth > 0 && fboHeight > 0) {
            std::vector<GLubyte> blackData(fboWidth * fboHeight * 3, 0);
            glBindTexture(GL_TEXTURE_2D, kscopeHistoryTexture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fboWidth, fboHeight, GL_RGB, GL_UNSIGNED_BYTE, blackData.data());
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        kscope_angle = 0.0f;
        return frame.clone();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, kscopeFBO);
    glViewport(0, 0, fboWidth, fboHeight);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    // Grab GUI Mod mappings
    float raw_rot = get_mod("KSCOPE_ROT");
    float raw_offset = get_mod("KSCOPE_OFFSET");
    float raw_zoom = current_fx_gains.count("KSCOPE_ZOOM") ? current_fx_gains["KSCOPE_ZOOM"] : 1.0f;
    float raw_segments = current_fx_gains.count("KSCOPE_SEGMENTS") ? current_fx_gains["KSCOPE_SEGMENTS"] : 6.0f;

    // Quantize segments to even numbers: 0, 2, 4, 6, 8, 10, 12...
    float kscope_segs = std::round(raw_segments / 2.0f) * 2.0f;
    kscope_segs = std::clamp(kscope_segs, 0.0f, 16.0f);

    kscope_angle += raw_rot * 0.5f;

    kscopeShader.Bind();
    kscopeShader.SetFloat("offset", raw_offset);
    kscopeShader.SetFloat("rot", kscope_angle);
    kscopeShader.SetFloat("zoom", raw_zoom);
    kscopeShader.SetFloat("segments", kscope_segs);
    kscopeShader.SetVec2("resolution", static_cast<float>(fboWidth), static_cast<float>(fboHeight));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, videoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(rgbFrame.step / rgbFrame.elemSize()));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rgbFrame.cols, rgbFrame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    kscopeShader.SetInt("currentFrame", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, kscopeHistoryTexture);
    kscopeShader.SetInt("backbuffer", 1);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    cv::Mat outputFrame(fboHeight, fboWidth, CV_8UC3);
    glReadPixels(0, 0, fboWidth, fboHeight, GL_RGB, GL_UNSIGNED_BYTE, outputFrame.data);

    glBindVertexArray(0);
    kscopeShader.Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    cv::Mat bgrOutput;
    cv::cvtColor(outputFrame, bgrOutput, cv::COLOR_RGB2BGR);

    return bgrOutput;
}

cv::Mat MoshRackEngine::fx_flow(const cv::Mat& frame)
{
    if (frame.empty() || videoTexture == 0)
        return frame;

    cv::Mat rgbFrame;
    if (frame.channels() == 3)
    {
        cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    }
    else
    {
        return frame.clone();
    }

    static GLuint flowFBO = 0;
    static GLuint flowOutTexture = 0;
    static int fboWidth = 0;
    static int fboHeight = 0;

    if (flowFBO == 0 || fboWidth != rgbFrame.cols || fboHeight != rgbFrame.rows)
    {
        fboWidth = rgbFrame.cols;
        fboHeight = rgbFrame.rows;

        if (flowFBO != 0) glDeleteFramebuffers(1, &flowFBO);
        if (flowOutTexture != 0) glDeleteTextures(1, &flowOutTexture);

        glGenFramebuffers(1, &flowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, flowFBO);

        glGenTextures(1, &flowOutTexture);
        glBindTexture(GL_TEXTURE_2D, flowOutTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fboWidth, fboHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, flowOutTexture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return frame.clone();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

   
        // get_mod() handles the base knob + CV modulation automatically.
        // Syncing to "FLOW_HUE" restores the patch bay and MIDI connections.
    float live_flow = std::clamp(get_mod("FLOW"), 0.0f, 1.0f);
    float f_sat = std::clamp(get_mod("FLOW_SAT"), 0.0f, 1.0f);
    float f_hue = std::clamp(get_mod("FLOW_HUE"), 0.0f, 1.0f);

    bool power_on = fx_enabled.count("FLOW") ? fx_enabled["FLOW"] : false;
    bool any_fx_active = live_flow > 0.001f || f_sat > 0.001f || f_hue > 0.001f;

    if (!power_on || !any_fx_active)
    {
        return frame.clone();
    }

    static const auto flowStartTime = std::chrono::steady_clock::now();
    float flowTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - flowStartTime).count();

    glBindFramebuffer(GL_FRAMEBUFFER, flowFBO);
    glViewport(0, 0, fboWidth, fboHeight);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    flowShader.Bind();
    flowShader.SetFloat("flowAmt", live_flow);
    flowShader.SetFloat("satMod", f_sat);
    flowShader.SetFloat("wetMod", f_hue);
    flowShader.SetFloat("time", flowTime);
    flowShader.SetVec2("resolution", static_cast<float>(fboWidth), static_cast<float>(fboHeight));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, videoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(rgbFrame.step / rgbFrame.elemSize()));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rgbFrame.cols, rgbFrame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    flowShader.SetInt("currentFrame", 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    cv::Mat outputFrame(fboHeight, fboWidth, CV_8UC3);
    glReadPixels(0, 0, fboWidth, fboHeight, GL_RGB, GL_UNSIGNED_BYTE, outputFrame.data);

    glBindVertexArray(0);
    flowShader.Unbind();
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    cv::Mat bgrOutput;
    cv::cvtColor(outputFrame, bgrOutput, cv::COLOR_RGB2BGR);
    return bgrOutput;
}

cv::Mat MoshRackEngine::fx_chroma(const cv::Mat& frame) {
    if (frame.empty() || videoTexture == 0) return frame;

    cv::Mat rgbFrame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    }
    else {
        rgbFrame = frame;
    }

    static GLuint chromaFBO = 0;
    static GLuint chromaOutTexture = 0;
    static int fboWidth = 0;
    static int fboHeight = 0;

    if (chromaFBO == 0 || fboWidth != rgbFrame.cols || fboHeight != rgbFrame.rows) {
        fboWidth = rgbFrame.cols;
        fboHeight = rgbFrame.rows;

        if (chromaFBO != 0) {
            glDeleteFramebuffers(1, &chromaFBO);
            glDeleteTextures(1, &chromaOutTexture);
        }

        glGenFramebuffers(1, &chromaFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, chromaFBO);

        glGenTextures(1, &chromaOutTexture);
        glBindTexture(GL_TEXTURE_2D, chromaOutTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fboWidth, fboHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, chromaOutTexture, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool chroma_is_active = fx_enabled.count("CHROMA") ? fx_enabled["CHROMA"] : false;

    
// Grab knob gains and include audio modulation for CHROMA depth if patched
    float chroma_depth = get_mod("CHROMA");
    float chroma_warp = get_mod("CHROMA_WARP");
    float chroma_crazy = get_mod("CHROMA_CRAZY");

    if (!chroma_is_active || std::abs(chroma_depth) < 0.001f) {
        return frame.clone();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, chromaFBO);
    glViewport(0, 0, fboWidth, fboHeight);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    float currentTime = static_cast<float>(cv::getTickCount()) / static_cast<float>(cv::getTickFrequency());

    // Bind Shader and push uniforms
    chromaShader.Bind();
    chromaShader.SetFloat("depth", chroma_depth);
    chromaShader.SetFloat("warpSpd", chroma_warp);
    chromaShader.SetFloat("time", currentTime);
    chromaShader.SetFloat("crazy", chroma_crazy);
    chromaShader.SetVec2("resolution", static_cast<float>(fboWidth), static_cast<float>(fboHeight));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, videoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(rgbFrame.step / rgbFrame.elemSize()));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rgbFrame.cols, rgbFrame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    chromaShader.SetInt("currentFrame", 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    cv::Mat outputFrame(fboHeight, fboWidth, CV_8UC3);
    glReadPixels(0, 0, fboWidth, fboHeight, GL_RGB, GL_UNSIGNED_BYTE, outputFrame.data);

    glBindVertexArray(0);
    chromaShader.Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    cv::Mat bgrOutput;
    cv::cvtColor(outputFrame, bgrOutput, cv::COLOR_RGB2BGR);

    return bgrOutput;
}


cv::Mat MoshRackEngine::fx_mesh(const cv::Mat& frame) {
    if (frame.empty() || videoTexture == 0) return frame;

    cv::Mat rgbFrame;
    if (frame.channels() == 3) {
        cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
    }
    else {
        rgbFrame = frame;
    }

    // --- PRE-PROCESS: APPLY LUMA KEY TO INCOMING FRAME ---
    bool luma_active = fx_enabled.count("MESH_LUMA") && fx_enabled["MESH_LUMA"];
    if (luma_active) {
        float lumaThreshold = current_fx_gains.count("MESH_LUMA_VAL") ? current_fx_gains["MESH_LUMA_VAL"] : 0.35f;
        cv::Mat gray;
        cv::cvtColor(rgbFrame, gray, cv::COLOR_RGB2GRAY);

        cv::Mat mask;
        cv::threshold(gray, mask, lumaThreshold * 255.0f, 255.0f, cv::THRESH_BINARY);

        cv::Mat keyedFrame = cv::Mat::zeros(rgbFrame.size(), rgbFrame.type());
        rgbFrame.copyTo(keyedFrame, mask);
        rgbFrame = keyedFrame; // Now every downstream part gets the luma-keyed image!
    }

    static GLuint meshFBO = 0;
    static GLuint meshOutTexture = 0;
    static GLuint meshVAO = 0;
    static GLuint meshVBO = 0;
    static GLuint noiseTexX = 0, noiseTexY = 0, noiseTexZ = 0;
    static int fboWidth = 0;
    static int fboHeight = 0;
    static int lastGridSize = -1;
    static int lastTopologyState = -1;
    static int vertexCount = 0;
    static GLenum primitiveMode = GL_TRIANGLES;

    int width = rgbFrame.cols;
    int height = rgbFrame.rows;

    if (meshFBO == 0 || fboWidth != width || fboHeight != height) {
        fboWidth = width;
        fboHeight = height;

        if (meshFBO != 0) {
            glDeleteFramebuffers(1, &meshFBO);
            glDeleteTextures(1, &meshOutTexture);
        }

        glGenFramebuffers(1, &meshFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, meshFBO);

        glGenTextures(1, &meshOutTexture);
        glBindTexture(GL_TEXTURE_2D, meshOutTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, fboWidth, fboHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, meshOutTexture, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glGenTextures(1, &noiseTexX);
        glGenTextures(1, &noiseTexY);
        glGenTextures(1, &noiseTexZ);
    }

    bool mesh_active = fx_enabled.count("MESH") ? fx_enabled["MESH"] : false;
    if (!mesh_active) {
        return frame.clone();
    }

    // Update Oscillators & Noise Textures
    mesh_phase_x += 0.05f;
    mesh_phase_y += 0.04f;
    mesh_phase_z += 0.03f;

    cv::Mat noiseX = generateNoiseMat(mesh_phase_x, 1.0f, 180, 120);
    cv::Mat noiseY = generateNoiseMat(mesh_phase_y, 1.0f, 180, 120);
    cv::Mat noiseZ = generateNoiseMat(mesh_phase_z, 1.0f, 180, 120);

    auto uploadNoise = [](GLuint tex, const cv::Mat& mat) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, mat.cols, mat.rows, 0, GL_RED, GL_UNSIGNED_BYTE, mat.data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        };

    uploadNoise(noiseTexX, noiseX);
    uploadNoise(noiseTexY, noiseY);
    uploadNoise(noiseTexZ, noiseZ);

    // Update Mesh Geometry if resolution or combined topology state changed
    float scaleGain = get_mod("MESH_SCALE");
    if (scaleGain < 2.0f) scaleGain = 25.0f; // Default baseline if knob is at 0
    int gridResolution = std::clamp(static_cast<int>(scaleGain), 2, 80);
    int currentTopologyState = (mesh_triangles ? 1 : 0) | (mesh_horiz ? 2 : 0) | (mesh_vert ? 4 : 0);

    if (gridResolution != lastGridSize || currentTopologyState != lastTopologyState) {
        updateMeshGeometry(gridResolution, mesh_triangles, mesh_horiz, mesh_vert, width, height, meshVAO, meshVBO, vertexCount, primitiveMode);
        lastGridSize = gridResolution;
        lastTopologyState = currentTopologyState;
    }

    // --- RENDER PASS STATE SETUP ---
    glBindFramebuffer(GL_FRAMEBUFFER, meshFBO);
    glViewport(0, 0, fboWidth, fboHeight);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    meshShader.Bind();

    // Upload pre-keyed frame pixels to videoTexture on unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, videoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(rgbFrame.step / rgbFrame.elemSize()));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rgbFrame.cols, rgbFrame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbFrame.data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    meshShader.SetInt("currentFrame", 0);

    // Bind noise textures to units 1, 2, 3
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, noiseTexX); meshShader.SetInt("x_noise_image", 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, noiseTexY); meshShader.SetInt("y_noise_image", 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, noiseTexZ); meshShader.SetInt("z_noise_image", 3);

    // Push GUI gains & patch modulation to uniforms
    float xAmp = get_mod("MESH_H");
    float yAmp = get_mod("MESH_V");
    float zAmp = get_mod("MESH_Z") * 4.0f; // Boost Z depth intensity

    meshShader.SetFloat("x_lfo_amp", xAmp);
    meshShader.SetFloat("y_lfo_amp", yAmp);
    meshShader.SetFloat("z_lfo_amp", zAmp);

    meshShader.SetInt("width", width);
    meshShader.SetInt("height", height);

    meshShader.SetInt("b_w_switch", (fx_enabled.count("MESH_BW") && fx_enabled["MESH_BW"]) ? 1 : 0);
    meshShader.SetInt("invert_switch", (fx_enabled.count("MESH_INVERT") && fx_enabled["MESH_INVERT"]) ? 1 : 0);

    // Draw the custom mesh geometry
    glBindVertexArray(meshVAO);
    glDrawArrays(primitiveMode, 0, vertexCount);

    // Read pixels back to OpenCV Mat
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    cv::Mat outputFrame(fboHeight, fboWidth, CV_8UC3);
    glReadPixels(0, 0, fboWidth, fboHeight, GL_RGB, GL_UNSIGNED_BYTE, outputFrame.data);

    cv::flip(outputFrame, outputFrame, 0);

    glBindVertexArray(0);
    meshShader.Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    cv::Mat bgrOutput;
    cv::cvtColor(outputFrame, bgrOutput, cv::COLOR_RGB2BGR);

    return bgrOutput;
}



cv::Mat MoshRackEngine::fx_transform_3d(const cv::Mat& input) {
    if (input.empty()) return input;

    // Fetch modulated values for rotation and translation (balanced ±90° & ±1.0)
    float rx = get_mod("ROT_X");
    float ry = get_mod("ROT_Y");
    float px = get_mod("POS_X");
    float py = get_mod("POS_Y");

    // Bypass if all transform values are effectively zero
    if (std::abs(rx) < 0.001f && std::abs(ry) < 0.001f &&
        std::abs(px) < 0.001f && std::abs(py) < 0.001f) {
        return input;
    }

    int w = input.cols;
    int h = input.rows;
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    // Convert degrees to radians
    float radX = rx * (3.14159265f / 180.0f);
    float radY = ry * (3.14159265f / 180.0f);

    // Balanced virtual camera focal length (prevents extreme one-sided perspective skewing at high angles)
    float f = std::max(w, h) * 1.8f;

    // Trig values
    float cosX = std::cos(radX), sinX = std::sin(radX);
    float cosY = std::cos(radY), sinY = std::sin(radY);

    // Camera Intrinsic & Extrinsic projection matrices
    cv::Matx33f K(f, 0, cx,
        0, f, cy,
        0, 0, 1);

    cv::Matx33f InvK(1.0f / f, 0, -cx / f,
        0, 1.0f / f, -cy / f,
        0, 0, 1);

    // Rotation matrices around X and Y axes
    cv::Matx33f Rx(1, 0, 0,
        0, cosX, -sinX,
        0, sinX, cosX);

    cv::Matx33f Ry(cosY, 0, sinY,
        0, 1, 0,
        -sinY, 0, cosY);

    // Combined 3D rotation (Yaw * Pitch)
    cv::Matx33f R = Ry * Rx;

    // Symmetric translation mapping for bidirectional X/Y panning
    float tx = px * (w * 0.5f);
    float ty = py * (h * 0.5f);

    cv::Matx33f T(1, 0, tx,
        0, 1, ty,
        0, 0, 1);

    // Full 3D Homography Matrix: H = T * K * R * InvK
    cv::Matx33f H = T * (K * R * InvK);

    cv::Mat output;
    // BORDER_WRAP tiles copies symmetrically across edges when pushed past boundaries
    cv::warpPerspective(input, output, cv::Mat(H), input.size(),
        cv::INTER_LINEAR, cv::BORDER_WRAP, cv::Scalar(0, 0, 0));
    return output;
}

cv::Mat MoshRackEngine::fx_zoom(const cv::Mat& input) {
    if (input.empty()) return input;

    // Fetch modulated zoom value (-1.0 to 1.0)
    float zoom_val = get_mod("ZOOM");

    // Map to a deep, smooth scale factor
    float scale = 1.0f;
    if (zoom_val > 0.0f) {
        // Zooming IN: 1.0x up to 5.0x
        scale = 1.0f + zoom_val * 4.0f;
    }
    else if (zoom_val < 0.0f) {
        // Zooming OUT REALLY FAR: 1.0x exponentially down to 0.02x (50x zoom out)
        float t = -zoom_val; // 0.0 to 1.0
        scale = std::pow(0.02f, t);
    }

    // Bypass if scale is essentially 1.0 for performance
    if (std::abs(scale - 1.0f) < 0.001f) {
        return input;
    }

    int w = input.cols;
    int h = input.rows;
    cv::Point2f center(w * 0.5f, h * 0.5f);

    // Build affine transformation matrix for scaling around the center
    cv::Mat M = cv::getRotationMatrix2D(center, 0.0, scale);
    cv::Mat output;

    // Use BORDER_WRAP to tile and repeat copies of the frame across the edges when zooming out
    cv::warpAffine(input, output, M, input.size(), cv::INTER_LINEAR, cv::BORDER_WRAP, cv::Scalar(0, 0, 0));

    return output;
}



cv::Mat MoshRackEngine::fx_rgb(const cv::Mat& input) {
    if (input.empty()) return input;

    // 1. Bipolar RGB Gains (Center 0.0 = 1.0x normal)
    float r_gain = 1.0f + rgb_gains[0];
    float g_gain = 1.0f + rgb_gains[1];
    float b_gain = 1.0f + rgb_gains[2];

    cv::Mat output;
    std::vector<cv::Mat> channels;
    cv::split(input, channels);

    if (channels.size() == 3) {
        channels[0] *= b_gain;
        channels[1] *= g_gain;
        channels[2] *= r_gain;
        cv::merge(channels, output);
    }
    else {
        output = input.clone();
    }

    // 2. Luma Keying
    bool luma_active = fx_enabled.count("LUMA_KEY") && fx_enabled["LUMA_KEY"];
    bool chroma_active = fx_enabled.count("CHROMA_KEY") && fx_enabled["CHROMA_KEY"];

    if (luma_active) {
        cv::Mat gray;
        cv::cvtColor(output, gray, cv::COLOR_BGR2GRAY);
        float thresh = key_amount * 255.0f;
        cv::Mat mask;
        cv::threshold(gray, mask, thresh, 255, cv::THRESH_BINARY);
        cv::Mat keyedOutput = cv::Mat::zeros(output.size(), output.type());
        output.copyTo(keyedOutput, mask);
        output = keyedOutput;
    }
    // 3. Chroma Keying (Hue-based range around green screen standard)
    else if (chroma_active) {
        cv::Mat hsv;
        cv::cvtColor(output, hsv, cv::COLOR_BGR2HSV);

        int centerHue = 60; // Green spectrum baseline
        int tolerance = static_cast<int>(key_amount * 90.0f);
        int lowerHue = std::max(0, centerHue - tolerance);
        int upperHue = std::min(180, centerHue + tolerance);

        cv::Mat mask;
        cv::inRange(hsv, cv::Scalar(lowerHue, 50, 50), cv::Scalar(upperHue, 255, 255), mask);
        cv::bitwise_not(mask, mask); // Retain subject, drop background

        cv::Mat keyedOutput = cv::Mat::zeros(output.size(), output.type());
        output.copyTo(keyedOutput, mask);
        output = keyedOutput;
    }

    return output;
}



void MoshRackEngine::tap_tempo()
{
    static std::vector<std::chrono::steady_clock::time_point> tap_times;
    auto now = std::chrono::steady_clock::now();

    // If more than 2 seconds have passed since the last tap, reset the tap history
    if (!tap_times.empty())
    {
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - tap_times.back()).count();
        if (diff > 2000)
        {
            tap_times.clear();
        }
    }

    tap_times.push_back(now);

    // Keep only the last 4 taps for a responsive average
    if (tap_times.size() > 4)
    {
        tap_times.erase(tap_times.begin());
    }

    // Need at least 2 taps to calculate an interval
    if (tap_times.size() >= 2)
    {
        float total_interval = 0.0f;
        for (size_t i = 1; i < tap_times.size(); ++i)
        {
            std::chrono::duration<float> interval = tap_times[i] - tap_times[i - 1];
            total_interval += interval.count();
        }

        float avg_interval = total_interval / (tap_times.size() - 1);

        if (avg_interval > 0.05f) // Prevent division by zero or lightning-fast spam taps
        {
            float bpm = 60.0f / avg_interval;

            // Clamp BPM to your GUI rate limits (0.1 Hz to 5.0 Hz = 6 BPM to 300 BPM)
            if (bpm < 6.0f) bpm = 6.0f;
            if (bpm > 300.0f) bpm = 300.0f;

            // Convert BPM back to Hz rate for the LFO
            lfo_rate = bpm / 60.0f;
        }
    }
}

void MoshRackEngine::toggle_beat_sync(bool) {}
void MoshRackEngine::register_sync_beat(std::chrono::steady_clock::time_point t)
{
    std::lock_guard<std::mutex> lock(sync_mutex);
    if (!lfo_beat_sync_active) return;

    sync_beat_times.push_back(t);

    // We collect 16 beat hits (15 intervals) to ensure rock-solid accuracy
    if (sync_beat_times.size() >= 16)
    {
        float total_duration = std::chrono::duration<float>(sync_beat_times.back() - sync_beat_times.front()).count();
        int intervals = static_cast<int>(sync_beat_times.size()) - 1;

        if (intervals > 0 && total_duration > 0.0f)
        {
            float avg_interval = total_duration / intervals;
            float bpm = 60.0f / avg_interval;

            // Clamp to musical ranges (60 to 200 BPM)
            if (bpm < 40.0f) bpm *= 2.0f; // Double-time correction if too slow
            if (bpm > 200.0f) bpm *= 0.5f; // Half-time correction if too fast

            if (bpm >= 40.0f && bpm <= 240.0f)
            {
                lfo_rate = bpm / 60.0f; // Convert BPM to Hz for the LFO
                lfo_status_text = "Synced (" + std::to_string(static_cast<int>(std::round(bpm))) + " BPM)";
            }
            else
            {
                lfo_status_text = "Sync Failed";
            }
        }
        sync_beat_times.clear();
        lfo_beat_sync_active.store(false, std::memory_order_relaxed); // Turn off sync button automatically
    }
    else
    {
        lfo_status_text = "Syncing... (" + std::to_string(sync_beat_times.size()) + "/16)";
    }
}
void MoshRackEngine::InitGL() {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glGenTextures(1, &videoTexture);
    glBindTexture(GL_TEXTURE_2D, videoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    
    moshShader.LoadFromFile("shaders/fullscreen.vert", "shaders/mosh.frag");
    flowShader.LoadFromFile("shaders/fullscreen.vert", "shaders/flow.frag");
    chromaShader.LoadFromFile("shaders/fullscreen.vert", "shaders/chroma.frag");
    kscopeShader.LoadFromFile("shaders/fullscreen.vert", "shaders/kscope.frag");
    meshShader.LoadFromFile("shaders/mesh.vert", "shaders/mesh.frag");

    // --- INDIVIDUAL SHADER DIAGNOSTIC REPORTS ---
    auto reportShader = [](const std::string& name, const Shader& s) {
        if (s.id == 0) {
            std::cout << "\n[CRITICAL ERROR] " << name << " ID is 0! Failed to load or compile." << std::endl;
        }
        else {
            std::cout << "[SUCCESS] " << name << " loaded with ID: " << s.id << std::endl;
        }
        };

	
    reportShader("Mosh Shader", moshShader);
    reportShader("Flow Shader", flowShader);
    reportShader("Chroma Shader", chromaShader);
    reportShader("Kscope Shader", kscopeShader);
    reportShader("Mesh Shader", meshShader);
    
}







