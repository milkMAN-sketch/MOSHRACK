#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D u_CurrentFrame;   
uniform sampler2D u_HistoryBuffer;  

uniform float u_MoshIntensity;     
uniform float u_BlockSize;         
uniform float u_MotionThreshold;   
uniform float u_Shift;             // RAINBOW KNOB
uniform vec2  u_Resolution;        



void main() {
    vec2 uv = TexCoords;
    vec2 pixelSize = 1.0 / u_Resolution;
    
    vec4 currFrame = texture(u_CurrentFrame, uv);
    vec4 histFrame = texture(u_HistoryBuffer, uv);

    // NEW: Audio-Taper Exponential Curve
    float moshKnob = clamp(u_MoshIntensity, 0.0, 1.0);
    
    // Cubing the input stretches the low-end sweet spot across the knob's physical rotation
    float moshPower = moshKnob * moshKnob * moshKnob; 

    if (moshKnob <= 0.001) {
        FragColor = currFrame;
        return;
    }

  

    // 1. ISOLATE MOTION 
    vec3 diff = abs(currFrame.rgb - histFrame.rgb);
    float motion = max(diff.r, max(diff.g, diff.b));

    // 2. HARSH POSTERIZATION (Crisp Tear Shapes)
    float steps = mix(32.0, 2.0, clamp(u_BlockSize, 0.0, 1.0));
    float posterMotion = floor(motion * steps) / steps;

    // 3. HARD DIRECTIONAL VECTORS (8-way grid lock)
    vec2 dragDir = vec2(
        sign(currFrame.r - histFrame.r),
        sign(currFrame.b - histFrame.b)
    );

    float thresh = (1.0 - clamp(u_MotionThreshold, 0.0, 1.0)) * 0.3;
    vec4 finalColor;

    if (posterMotion > thresh) {
        
        // 4. PIXEL-PERFECT DRAG (Crisp, no haze)
        float dragDistance = posterMotion * (moshPower * 12.0);
        vec2 offset = dragDir * dragDistance * pixelSize;
        offset = floor(offset / pixelSize) * pixelSize; 

        vec2 distortedUV = clamp(uv - offset, 0.0, 1.0);
        
        // Grab the rigidly torn pixel
        finalColor = texture(u_HistoryBuffer, distortedUV);
     // 5. BULLETPROOF RAINBOW INJECTION (No White-Outs)
        if (u_Shift > 0.001) {
            // Find the brightness (luma) of the torn pixel so we don't lose the image texture
            float luma = dot(finalColor.rgb, vec3(0.299, 0.587, 0.114));
            
            // Cosine palette offsets for Red, Green, Blue
            vec3 neonPhase = vec3(0.0, 0.33, 0.67); 
            
            // The color is now driven by the pixel's inherent brightness and the knob!
            vec3 neonGlitch = 0.5 + 0.5 * cos(6.28318 * (u_Shift * 2.5 + luma + neonPhase));
            
            // Straight mix! By overriding instead of adding, it can never blow out to white.
            float blendAmt = clamp(u_Shift * 0.7, 0.0, 0.7);
            finalColor.rgb = mix(finalColor.rgb, neonGlitch, blendAmt);
        }
        
    } else {
        // CRISP FREEZE MASKING
        float freezeMask = step(1.0 - moshKnob, 0.99); 
        finalColor = mix(currFrame, histFrame, freezeMask);
    }

    FragColor = vec4(finalColor.rgb, 1.0);
}