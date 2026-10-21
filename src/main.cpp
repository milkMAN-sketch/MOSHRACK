#include <windows.h> // Required for HICON, LoadIcon, etc.
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include "MoshRackEngine.h"
#include "MoshRackGUI.h"
#include "VideoWorker.h"
#include <filesystem>

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

// Intercept and cancel close requests for the output window so "X" does nothing
static void output_window_close_callback(GLFWwindow* window) {
    glfwSetWindowShouldClose(window, GLFW_FALSE);
}

// --- CROSS-PLATFORM WINDOW ICON HELPER ---
static void SetGLFWWindowIcon(GLFWwindow* window, const std::vector<std::string>& candidatePaths) {
    cv::Mat iconImg;
    std::string loadedPath = "";
    for (const auto& path : candidatePaths) {
        iconImg = cv::imread(path, cv::IMREAD_UNCHANGED);
        if (!iconImg.empty()) {
            loadedPath = path;
            break;
        }
    }

    if (iconImg.empty()) {
        std::cerr << "[Icon Error] Failed to load icon from specified paths." << std::endl;
        return;
    }

    std::cout << "[Icon Success] Loaded icon: " << loadedPath << " (" << iconImg.cols << "x" << iconImg.rows << ")" << std::endl;

    cv::Mat rgbaImg;
    if (iconImg.channels() == 3) {
        cv::cvtColor(iconImg, rgbaImg, cv::COLOR_BGR2RGBA);
    }
    else if (iconImg.channels() == 4) {
        cv::cvtColor(iconImg, rgbaImg, cv::COLOR_BGRA2RGBA);
    }
    else {
        return;
    }

    GLFWimage image;
    image.width = rgbaImg.cols;
    image.height = rgbaImg.rows;
    image.pixels = rgbaImg.data; // Raw pointer to the pixel buffer

    glfwSetWindowIcon(window, 1, &image);
}

int main(int, char**) {
    // Setup window toolkit (GLFW)
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Icon fallback options
    std::vector<std::string> iconPaths = {
        "assets/Moshrackicon.ico",
        "assets/icon.ico",
        "assets/icon.png"
    };

    // 2. Create Main Control Window
    GLFWwindow* mainWindow = glfwCreateWindow(1280, 720, "MOSHRACK // CONTROL PANEL", nullptr, nullptr);
    if (mainWindow == nullptr)
        return 1;

    glfwMakeContextCurrent(mainWindow);
    glfwSwapInterval(1);
    std::cout << "Current Working Directory: " << std::filesystem::current_path() << std::endl;

    // Apply icon to main window
    SetGLFWWindowIcon(mainWindow, iconPaths);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        return 1;
    }

    // 2b. Create Secondary Native OS Output Window
    GLFWwindow* outputWindow = glfwCreateWindow(1280, 720, "MOSHRACK // MAIN OUTPUT", nullptr, mainWindow);
    if (outputWindow == nullptr)
        return 1;

    glfwSetWindowPos(outputWindow, 1300, 100);

    // Apply icon to output window and lock its "X" button
    SetGLFWWindowIcon(outputWindow, iconPaths);
    glfwSetWindowCloseCallback(outputWindow, output_window_close_callback);

    // 3. Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // 4. Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 5. Initialize Engine and GUI
    auto engine = std::make_shared<MoshRackEngine>();
    engine->InitGL();

    MoshRackGUI gui(engine);
    gui.Init();

    // 6. Main Application Loop
    while (!glfwWindowShouldClose(mainWindow)) {
        glfwPollEvents();

        // --- A. RENDER CONTROL PANEL WINDOW ---
        glfwMakeContextCurrent(mainWindow);
        gui.UpdateVideoTexture();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        gui.RenderControlPanel();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(mainWindow, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.04f, 0.04f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(mainWindow);

        // --- B. RENDER NATIVE OUTPUT WINDOW ---
        glfwMakeContextCurrent(outputWindow);
        int out_w, out_h;
        glfwGetFramebufferSize(outputWindow, &out_w, &out_h);
        glViewport(0, 0, out_w, out_h);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        gui.RenderNativeOutputWindow(out_w, out_h);

        glfwSwapBuffers(outputWindow);
    }

    // 7. Cleanup
    gui.Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(outputWindow);
    glfwDestroyWindow(mainWindow);
    glfwTerminate();

    return 0;
}