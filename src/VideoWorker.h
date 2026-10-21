#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

#include <opencv2/opencv.hpp>
 
#include "MoshRackEngine.h"



class VideoWorker {
public:
    VideoWorker(MoshRackEngine* eng);
    ~VideoWorker();
    
    static void EnumerateWindowsCameras(
        std::vector<MoshRackEngine::DeviceInfo>& devices,
        std::vector<std::string>& source_names);
    void start();
    void stop();
    bool isRunning() const;
    bool getLatestFrame(cv::Mat& outFrame);
    void setDeviceIndex(int idx) { m_deviceIdx = idx; }
    enum class SourceType { CAMERA, FILE };

    // New functions to switch sources on the fly
    void setCameraSource(int deviceIdx);
    void setFileSource(const std::string& filepath);
private:
    void run();

    MoshRackEngine* m_engine;
    std::atomic<bool> m_running{ false };
    std::thread m_thread;
    std::mutex m_frameMutex;
    cv::VideoCapture m_cap;
    cv::Mat m_currentFrame;
    int m_deviceIdx{ 0 };
    SourceType m_currentSourceType = SourceType::CAMERA;
    std::string m_filePath = "";
    bool m_sourceChanged = false; // Tells the thread to switch sources
    bool m_hasNewFrame = false;
};
