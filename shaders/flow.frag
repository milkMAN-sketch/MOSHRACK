#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D currentFrame;
uniform float time;
uniform float flowAmt;
uniform float satMod;
uniform float wetMod;
uniform vec2 resolution;

float getLuma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec2 computeWaterFlow(vec2 uv, float t, float intensity, out float lumaOut) {
    vec2 p = uv * 6.0;
    vec3 baseSample = texture(currentFrame, uv).rgb;
    lumaOut = getLuma(baseSample);
    
    float wave1 = sin(p.x * 2.0 + t * 1.5 + lumaOut * 3.0) * cos(p.y * 2.0 - t * 1.5);
    float wave2 = sin(p.x * 4.0 - t * 2.0 + wave1) * cos(p.y * 3.0 + t * 1.2);
    
    vec2 displacement = vec2(wave1, wave2) * 0.03 * intensity;
    displacement *= (1.0 + lumaOut * 1.5);
    
    return clamp(uv + displacement, 0.0, 1.0);
}

vec3 applyWetGloss(vec3 color, vec2 uv, float t, float wetGain, float luma) {
    if (wetGain <= 0.001) return color;
    
    vec2 eps = vec2(1.0 / resolution.x, 0.0);
    float lumaX = getLuma(texture(currentFrame, uv + eps).rgb) - getLuma(texture(currentFrame, uv - eps).rgb);
    float lumaY = getLuma(texture(currentFrame, uv + eps.yx).rgb) - getLuma(texture(currentFrame, uv - eps.yx).rgb);
    vec3 normal = normalize(vec3(-lumaX * 4.0, -lumaY * 4.0, 1.0));
    
    vec3 lightDir = normalize(vec3(sin(t), cos(t), 0.6));
    vec3 viewDir = vec3(0.0, 0.0, 1.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    
    float spec = pow(max(dot(normal, halfDir), 0.0), 24.0);
    vec3 glossColor = color * spec * wetGain * 1.5 + vec3(spec * wetGain * 0.4);
    
    return clamp(color + glossColor, 0.0, 1.0);
}

void main() {
    if (flowAmt <= 0.001 && wetMod <= 0.001) {
        FragColor = texture(currentFrame, TexCoords);
        return;
    }

    float luma = 0.0;
    vec2 warpedUV = computeWaterFlow(TexCoords, time, flowAmt, luma);

    // Sample warped frame without chromatic splitting
    vec3 finalColor = texture(currentFrame, warpedUV).rgb;

    // Saturation adjustment
    vec3 gray = vec3(getLuma(finalColor));
    finalColor = mix(gray, finalColor, 1.0 + satMod * 0.5);

    // Wet gloss layer
    finalColor = applyWetGloss(finalColor, TexCoords, time, wetMod, luma);

    FragColor = vec4(finalColor, 1.0);
}