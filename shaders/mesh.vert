#version 330 core
#extension GL_ARB_shader_texture_lod : enable

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoords;
out float VertexLuma;

uniform sampler2D currentFrame;
uniform sampler2D x_noise_image;
uniform sampler2D y_noise_image;
uniform sampler2D z_noise_image;

uniform int width;
uniform int height;

uniform float x_lfo_amp;
uniform float y_lfo_amp;
uniform float z_lfo_amp;

void main() {
    vec2 uv = aTexCoord;

    // Sample video luminance at this vertex coordinate
    vec4 col = textureLod(currentFrame, uv, 0.0);
    float luma = dot(col.rgb, vec3(0.299, 0.587, 0.114));

    // Sample the 3 chaotic Perlin noise oscillator textures
    float nx = textureLod(x_noise_image, uv, 0.0).r;
    float ny = textureLod(y_noise_image, uv, 0.0).r;
    float nz = textureLod(z_noise_image, uv, 0.0).r;

    // --- TRUE RUTT-ETRA X/Y SCAN PROCESSOR DISPLACEMENT ---
    vec3 displacedPos = aPos;
    
    displacedPos.x += (luma - 0.5) * (x_lfo_amp * 20.0) + (nx - 0.5) * (x_lfo_amp * 12.0);
    displacedPos.y += (luma - 0.5) * (y_lfo_amp * 20.0) + (ny - 0.5) * (y_lfo_amp * 12.0);
    
    // Z-noise wave folding into X/Y space
    displacedPos.x += (nz - 0.5) * (z_lfo_amp * 8.0);
    displacedPos.y += (nz - 0.5) * (z_lfo_amp * 8.0);

    // Convert pixel coordinates to Normalized Device Coordinates (-1.0 to 1.0) and flip Y for OpenGL
    float x = (displacedPos.x / float(width)) * 2.0 - 1.0;
    float y = 1.0 - (displacedPos.y / float(height)) * 2.0;

    gl_Position = vec4(x, y, 0.0, 1.0);
    TexCoords = uv;
    VertexLuma = luma;
}