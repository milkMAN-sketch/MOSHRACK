#pragma once

#include <string>
#include <unordered_map>
#include <GL/glew.h> // Or whatever OpenGL loader you're using (glad, etc.)


class Shader {
public:
    GLuint id = 0;

    Shader() = default;
    ~Shader();

    // Load and compile shaders from source strings or files
    bool LoadFromMemory(const char* vertexSource, const char* fragmentSource);
    bool LoadFromFile(const std::string& vertexPath, const std::string& fragmentPath);

    void Bind() const;
    void Unbind() const;

    // Uniform setters for your effect knobs and parameters
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, float x, float y) const;
    void SetVec3(const std::string& name, float x, float y, float z) const;
    void SetVec4(const std::string& name, float x, float y, float z, float w) const;

private:
    std::unordered_map<std::string, GLint> uniformLocationCache;
    GLint GetUniformLocation(const std::string& name);
    bool CompileCheck(GLuint shader, const std::string& type);
    bool LinkCheck(GLuint program);
};
