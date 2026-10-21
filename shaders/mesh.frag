#version 330 core

out vec4 FragColor;
in vec2 TexCoords;
in float VertexLuma;

uniform sampler2D currentFrame;
uniform int b_w_switch;
uniform int invert_switch;

void main() {
    vec4 color = texture(currentFrame, TexCoords);

    if (b_w_switch != 0) {
        color.rgb = vec3(VertexLuma);
    }

    if (invert_switch != 0) {
        // Only invert non-keyed (non-black) pixels to keep background pristine
        if (color.rgb != vec3(0.0)) {
            color.rgb = vec3(1.0) - color.rgb;
        }
    }

    FragColor = color;
}