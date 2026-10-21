#include "MoshRackSplash.h"
#include <algorithm>
#include <iostream>

MoshRackSplash::MoshRackSplash() {
    last_time = std::chrono::steady_clock::now();

    // Seed the cache immediately so it's never empty on frame one
    cv::Mat initial_frame(cv::Size(640, 480), CV_8UC3, cv::Scalar(10, 10, 12));
    cv::putText(initial_frame, "[ MOSHRACK <3 ]",
        cv::Point(50, 72),
        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 220, 255), 2, cv::LINE_AA);
    screensaver_cache = initial_frame;
}

void MoshRackSplash::load_custom_image(const std::string& filepath) {
    custom_splash_img = cv::imread(filepath, cv::IMREAD_COLOR);
    if (!custom_splash_img.empty()) {
        std::cout << "[Splash] Loaded custom image: " << filepath << std::endl;
    }
    else {
        std::cerr << "[Splash Error] Could not load image: " << filepath << std::endl;
    }
}

void MoshRackSplash::clear_custom_image() {
    custom_splash_img.release();
    std::cout << "[Splash] Cleared custom image. Using screensaver." << std::endl;
}
void MoshRackSplash::update_physics() {
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float, std::chrono::seconds::period>(now - last_time).count();

    // RATE LIMITER: Prevent physics from ticking faster than ~60 times per second (16.6ms)
    // This stops high-refresh monitors (144Hz/240Hz) from making the DVD logo zip around.
    if (dt < 0.015f) {
        return;
    }

    last_time = now;

    // Safety clamp for dt if window is dragged or stalls
    if (dt > 0.1f) dt = 0.016f;

    const float VIRTUAL_W = 640.0f;
    const float VIRTUAL_H = 480.0f;

    // Scaled down for a slow, hypnotic DVD player drift
    float speed_mult = dt * 22.0f;
    screensaver_pos.x += screensaver_vel.x * speed_mult;
    screensaver_pos.y += screensaver_vel.y * speed_mult;

    const float textW = 140.0f;
    const float textH = 30.0f;

    if (screensaver_pos.x <= 0.0f || screensaver_pos.x + textW >= VIRTUAL_W) {
        screensaver_vel.x *= -1.0f;
        screensaver_color = cv::Scalar(rand() % 155 + 100, rand() % 155 + 100, 255);
    }
    if (screensaver_pos.y <= 0.0f || screensaver_pos.y + textH >= VIRTUAL_H) {
        screensaver_vel.y *= -1.0f;
        screensaver_color = cv::Scalar(255, rand() % 155 + 100, rand() % 155 + 100);
    }
    screensaver_pos.x = std::clamp(screensaver_pos.x, 0.0f, VIRTUAL_W - textW);
    screensaver_pos.y = std::clamp(screensaver_pos.y, 0.0f, VIRTUAL_H - textH);

    // Render frame cache
    cv::Mat frame(cv::Size(static_cast<int>(VIRTUAL_W), static_cast<int>(VIRTUAL_H)), CV_8UC3, cv::Scalar(10, 10, 12));
    cv::putText(frame, "[ MOSHRACK <3 ]",
        cv::Point(static_cast<int>(screensaver_pos.x), static_cast<int>(screensaver_pos.y + 22)),
        cv::FONT_HERSHEY_SIMPLEX, 0.7, screensaver_color, 2, cv::LINE_AA);

    screensaver_cache = frame;
}

cv::Mat MoshRackSplash::get_active_layer(cv::Size target_size, int target_type) {
    // NOTE: update_physics() is NOT called here anymore! 
    // It is called strictly once per frame by the engine.

    cv::Mat layer;
    if (!custom_splash_img.empty()) {
        layer = custom_splash_img;
    }
    else {
        layer = screensaver_cache;
    }

    if (layer.empty()) return cv::Mat();

    cv::Mat resized;
    if (layer.size() != target_size) {
        cv::resize(layer, resized, target_size);
    }
    else {
        resized = layer;
    }

    cv::Mat converted;
    if (resized.type() != target_type) {
        if (target_type == CV_8UC1) {
            cv::cvtColor(resized, converted, cv::COLOR_BGR2GRAY);
        }
        else {
            resized.convertTo(converted, target_type);
        }
    }
    else {
        converted = resized;
    }

    return converted;
}