
#include "MoshRackEngine.h"
#include "VideoWorker.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <dshow.h>
#endif

// Link the required Windows COM libraries automatically
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#include <iostream>

// ============================================================================
// VIDEO WORKER IMPLEMENTATION
// ============================================================================

VideoWorker::VideoWorker(MoshRackEngine* eng) : m_engine(eng) {}

VideoWorker::~VideoWorker() {
    stop();
}

void VideoWorker::start() {
    if (!m_running) {
        m_running = true;
        m_thread = std::thread(&VideoWorker::run, this);
    }
}

void VideoWorker::stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

bool VideoWorker::isRunning() const {
    return m_running;
}

void VideoWorker::setCameraSource(int deviceIdx) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_deviceIdx = deviceIdx;
    m_currentSourceType = SourceType::CAMERA;
    m_sourceChanged = true; // Signal the loop to switch
}

void VideoWorker::setFileSource(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    m_filePath = filepath;
    m_currentSourceType = SourceType::FILE;
    m_sourceChanged = true; // Signal the loop to switch
}

void VideoWorker::run() {
    bool is_open = false;
    int frame_delay_ms = 0; // Declared outside so the whole loop can see it!

    while (m_running) {
        // 1. Check if the UI asked us to change the video source
        bool needs_open = false;
        {
            std::lock_guard<std::mutex> lock(m_frameMutex);
            if (m_sourceChanged) {
                needs_open = true;
                m_sourceChanged = false;
            }
        }

        // 2. If a new source was selected (or startup), open it!
        if (needs_open || !is_open) {
            if (m_cap.isOpened()) m_cap.release();

            if (m_currentSourceType == SourceType::CAMERA) {
                m_cap.open(m_deviceIdx, cv::CAP_DSHOW);
                std::cout << "[Video] Switched to Camera: " << m_deviceIdx << std::endl;
                frame_delay_ms = 0; // Cameras handle their own hardware timing
            }
            else {
                m_cap.open(m_filePath);
                std::cout << "[Video] Switched to File: " << m_filePath << std::endl;
            }

            is_open = m_cap.isOpened();
            if (!is_open) {
                std::cerr << "[Video Error] Could not open source!" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            // Calculate Frame Delay for Files
            if (m_currentSourceType == SourceType::FILE) {
                double fps = m_cap.get(cv::CAP_PROP_FPS);
                if (fps <= 0.0) fps = 30.0; // Fallback if video header is weird
                frame_delay_ms = static_cast<int>(1000.0 / fps);
            }
        }

        // 3. Read the frame
       // 3. Read the frame
        cv::Mat temp;
        if (m_cap.read(temp) && !temp.empty()) {
            {
                std::lock_guard<std::mutex> lock(m_frameMutex);
                m_currentFrame = temp.clone();
                m_hasNewFrame = true; // <-- WE HAVE A FRESH FRAME!
            }

            // THE FIX: Throttle playback speed here so it matches real-time video speed!
            if (m_currentSourceType == SourceType::FILE && frame_delay_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay_ms));
            }
        }
        else {
            // If the frame is empty and we are playing a file, LOOP IT!
            if (m_currentSourceType == SourceType::FILE) {
                m_cap.set(cv::CAP_PROP_POS_FRAMES, 0);
            }
            else {
                // If a camera drops a frame, just wait a tiny bit and try again
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    if (m_cap.isOpened()) {
        m_cap.release();
    }
}

bool VideoWorker::getLatestFrame(cv::Mat& outFrame) {
    std::lock_guard<std::mutex> lock(m_frameMutex);

    // If the frame is empty OR we've already given this frame to the GUI, return false!
    if (m_currentFrame.empty() || !m_hasNewFrame) {
        return false;
    }

    m_currentFrame.copyTo(outFrame);
    m_hasNewFrame = false; // <-- CONSUME THE FRAME! (Next time GUI asks, it gets false)

    return true;
}

// ============================================================================
// WINDOWS CAMERA ENUMERATION (DIRECTSHOW)
// ============================================================================

void VideoWorker::EnumerateWindowsCameras(
    std::vector<MoshRackEngine::DeviceInfo>& devices,
    std::vector<std::string>& source_names)
{
    devices.clear();
    source_names.clear();

    std::cout << "[Diagnostics] Starting Windows Camera Enumeration (DirectShow)..." << std::endl;

    // 1. Initialize COM
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool coInit = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE) {
        std::cout << "[Diagnostics] CoInitializeEx: COM already initialized as STA (Safe to proceed)." << std::endl;
        coInit = false;
    }
    else if (FAILED(hr)) {
        std::cerr << "[Diagnostics ERROR] CoInitializeEx failed with HRESULT: 0x" << std::hex << hr << std::endl;
    }
    else {
        std::cout << "[Diagnostics] CoInitializeEx succeeded." << std::endl;
    }

    // 2. Create System Device Enumerator
    ICreateDevEnum* pDevEnum = NULL;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
    if (FAILED(hr)) {
        std::cerr << "[Diagnostics ERROR] CoCreateInstance(CLSID_SystemDeviceEnum) failed with HRESULT: 0x" << std::hex << hr << std::endl;
        if (coInit) CoUninitialize();
        return;
    }
    std::cout << "[Diagnostics] System Device Enumerator created successfully." << std::endl;

    // 3. Create Class Enumerator for Video Input Devices
    IEnumMoniker* pEnum = NULL;
    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);

    if (hr == S_FALSE) {
        std::cerr << "[Diagnostics WARNING] CreateClassEnumerator returned S_FALSE: No video capture devices found on this system." << std::endl;
    }
    else if (FAILED(hr)) {
        std::cerr << "[Diagnostics ERROR] CreateClassEnumerator failed with HRESULT: 0x" << std::hex << hr << std::endl;
    }
    else if (pEnum == NULL) {
        std::cerr << "[Diagnostics ERROR] CreateClassEnumerator succeeded but returned a null enumerator pointer." << std::endl;
    }
    else {
        std::cout << "[Diagnostics] Video Input Device Category enumerator created successfully. Scanning devices..." << std::endl;

        IMoniker* pMoniker = NULL;
        int deviceIndex = 0;
        ULONG fetched = 0;

        while (pEnum->Next(1, &pMoniker, &fetched) == S_OK) {
            std::cout << "[Diagnostics] Found device moniker at index " << deviceIndex << std::endl;

            IPropertyBag* pPropBag = NULL;
            hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));

            if (SUCCEEDED(hr)) {
                VARIANT var;
                VariantInit(&var);

                hr = pPropBag->Read(L"FriendlyName", &var, 0);
                if (SUCCEEDED(hr)) {
                    int length = WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1, NULL, 0, NULL, NULL);
                    std::string deviceName(length, 0);
                    WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1, &deviceName[0], length, NULL, NULL);

                    if (!deviceName.empty()) deviceName.pop_back();

                    std::cout << "[Diagnostics SUCCESS] Device " << deviceIndex << " Name: \"" << deviceName << "\"" << std::endl;

                    devices.push_back({ deviceIndex, deviceName });
                    source_names.push_back(std::to_string(deviceIndex) + ": " + deviceName);

                    VariantClear(&var);
                }
                else {
                    std::cerr << "[Diagnostics ERROR] Failed to read 'FriendlyName' property bag for device " << deviceIndex << " (HRESULT: 0x" << std::hex << hr << ")" << std::endl;
                }
                pPropBag->Release();
            }
            else {
                std::cerr << "[Diagnostics ERROR] BindToStorage failed for device " << deviceIndex << " (HRESULT: 0x" << std::hex << hr << ")" << std::endl;
            }
            pMoniker->Release();
            deviceIndex++;
        }
        pEnum->Release();
    }

    pDevEnum->Release();

    if (coInit) {
        CoUninitialize();
    }

    // Fallback if none found
    if (devices.empty()) {
        std::cout << "[Diagnostics] No devices populated. Adding fallback entry." << std::endl;
        devices.push_back({ 0, "Default Camera (Fallback)" });
        source_names.push_back("0: Default Camera (Fallback)");
    }
}