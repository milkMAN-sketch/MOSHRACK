#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <chrono>

class MoshRackSplash {
private:
    cv::Point2f screensaver_pos = cv::Point2f(50.0f, 50.0f);
    cv::Point2f screensaver_vel = cv::Point2f(2.5f, 2.0f);
    cv::Scalar  screensaver_color = cv::Scalar(0, 220, 255);

    cv::Mat custom_splash_img;
    cv::Mat screensaver_cache;

    std::chrono::steady_clock::time_point last_time;

public:
    MoshRackSplash();

    void load_custom_image(const std::string& filepath);
    void clear_custom_image();

    // Updates physics using true delta-time (smooth & independent of frame rate)
    void update_physics();

    // Returns the active splash image or screensaver, perfectly sized and typed
    cv::Mat get_active_layer(cv::Size target_size, int target_type);

    bool has_custom_image() const { return !custom_splash_img.empty(); }
};