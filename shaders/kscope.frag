#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D currentFrame;
uniform sampler2D backbuffer;
uniform vec2 resolution;

uniform float offset;   // KSCOPE_OFFSET (Segment phase shift)
uniform float rot;      // Accumulated rotation angle
uniform float zoom;     // KSCOPE_ZOOM (Tunnel recursive scale)
uniform float segments; // KSCOPE_SEGMENTS (Dynamic number of mirror segments)

const float PI = 3.14159265359;

// Dynamic kaleidoscope coordinate function with 0-segment safety passthrough
vec2 getKaleidoUV(vec2 st, vec2 center) {
    // If segments is 0 (or less than 2), bypass the mirror and return normal texture coords
    if (segments < 2.0) {
        return st;
    }

    vec2 dir = st - center;
    float r = length(dir);
    float theta = atan(dir.y, dir.x);

    // Segment phase shift offset twist
    theta += offset * 2.0 * PI;

    float segment_angle = (2.0 * PI) / segments;
    theta = mod(theta, segment_angle);
    if (theta < 0.0) theta += segment_angle;

    float half_segment = segment_angle * 0.5;
    if (theta > half_segment) {
        theta = segment_angle - theta;
    }

    return center + vec2(r * cos(theta), r * sin(theta));
}

void main() {
    vec2 st = TexCoords;
    vec2 center = vec2(0.5, 0.5);

    // --- 1. DYNAMIC KALEIDOSCOPE REMAP ---
    vec2 mirroredUV = getKaleidoUV(st, center);
    vec4 curr = texture(currentFrame, clamp(mirroredUV, 0.0, 1.0));

    // --- 2. BACKBUFFER FEEDBACK WITH CRISP TUNNEL TRANSFORM ---
    vec2 uv = mirroredUV - center;
    
    float s = sin(rot * 0.2);
    float c = cos(rot * 0.2);
    mat2 rotMat = mat2(c, -s, s, c);
    
    uv = rotMat * uv;
    uv /= clamp(zoom, 0.2, 4.0);
    uv += center;
    uv = clamp(uv, 0.0, 1.0);

    vec4 prev = texture(backbuffer, uv);

    // Apply a mild gamma lift to keep the feedback tunnel sharp and defined
    prev.rgb = pow(clamp(prev.rgb, 0.0, 1.0), vec3(0.85));

    // --- 3. BLEND AND OUTPUT ---
    FragColor = mix(curr, prev, 0.60);
}