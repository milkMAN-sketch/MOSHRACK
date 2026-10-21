#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D currentFrame;
uniform float depth;      // Knob 1: -1.0 to 1.0 (Balanced RGB Split)
uniform float warpSpd;    // Knob 2: 0.0 to 10.0 (Rainbow Scroll - slowed by 75%)
uniform float crazy;      // Knob 3: -1.0 to 1.0 (Custom Color Split)
uniform float time;

// RGB to HSV helper for Warp rotation
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// HSV to RGB helper for Warp rotation
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Calculates the "Subtle -> SUPER FAR" exponential displacement curve
float getSplitDist(float knobVal) {
    float absVal = abs(knobVal);
    // 1st Half: Linear and subtle
    // 2nd Half: Exponentially blows up
    float dist = (absVal <= 0.5) 
        ? (absVal * 0.05) 
        : (0.025 + pow((absVal - 0.5) * 2.0, 4.0) * 0.5);
    return dist * sign(knobVal);
}

void main() {
    vec2 uv = TexCoords;

    // --- 1. NORMAL CHROMA (DEPTH) - Balanced 3-Way Split ---
    float rgbDist = getSplitDist(depth);
    
    // 120-degree symmetrical vectors for R, G, and B so all three stay active on screen
    vec2 rDir = vec2(0.0, 1.0);
    vec2 gDir = vec2(0.866, -0.5);
    vec2 bDir = vec2(-0.866, -0.5);

    float r = texture(currentFrame, clamp(uv + rDir * rgbDist, 0.0, 1.0)).r;
    float g = texture(currentFrame, clamp(uv + gDir * rgbDist, 0.0, 1.0)).g;
    float b = texture(currentFrame, clamp(uv + bDir * rgbDist, 0.0, 1.0)).b;
    vec3 baseColor = vec3(r, g, b);

    // --- 2. CRAZY CHROMA (Tri-Color Split) ---
    if (abs(crazy) > 0.001) {
        float crazyDist = getSplitDist(crazy);
        
        // Depth split physically offsets the center point of the crazy split for interactive feedback
        vec2 interactionOffset = vec2(rgbDist * 0.3, -rgbDist * 0.3);
        
        vec3 col1 = vec3(0.0, 0.847, 1.0);     // Cyan (#00d8ff)
        vec3 col2 = vec3(0.0, 1.0, 0.094);     // Green (#00ff18)
        vec3 col3 = vec3(1.0, 0.0, 0.882);     // Pink (#ff00e1)

        vec2 cDir1 = vec2(0.5, 0.866);
        vec2 cDir2 = vec2(1.0, 0.0);
        vec2 cDir3 = vec2(0.0, -1.0);

        vec3 samp1 = texture(currentFrame, clamp(uv + interactionOffset + cDir1 * crazyDist, 0.0, 1.0)).rgb;
        vec3 samp2 = texture(currentFrame, clamp(uv + interactionOffset + cDir2 * crazyDist, 0.0, 1.0)).rgb;
        vec3 samp3 = texture(currentFrame, clamp(uv + interactionOffset + cDir3 * crazyDist, 0.0, 1.0)).rgb;

        vec3 luma = vec3(0.299, 0.587, 0.114);
        float lum1 = dot(samp1, luma);
        float lum2 = dot(samp2, luma);
        float lum3 = dot(samp3, luma);

        vec3 crazyLayer = (col1 * lum1 + col2 * lum2 + col3 * lum3);

        // Screen Blend interaction over the primary RGB base
        vec3 crazyActive = crazyLayer * abs(crazy);
        baseColor = baseColor + crazyActive - (baseColor * crazyActive);
    }

    // --- 3. CHROMA WARP (Rainbow Hue Scroll - Slowed by 75%) ---
    if (warpSpd > 0.001) {
        vec3 hsv = rgb2hsv(baseColor);
        hsv.x = mod(hsv.x + (time * warpSpd * 0.375), 1.0); 
        baseColor = hsv2rgb(hsv);
    }

    FragColor = vec4(baseColor, 1.0);
}