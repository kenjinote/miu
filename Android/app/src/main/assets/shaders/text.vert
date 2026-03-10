#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in float inIsColor;
layout(location = 3) in vec4 inColor; // ★ 追加：色情報

layout(push_constant) uniform PushConstants {
    float screenWidth;
    float screenHeight;
    vec2 padding;
    vec4 color;
} pc;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out float fragIsColor;
layout(location = 2) out vec4 fragColor; // ★ 追加

void main() {
    vec2 ndcPos = (inPos / vec2(pc.screenWidth, pc.screenHeight)) * 2.0 - 1.0;
    gl_Position = vec4(ndcPos, 0.0, 1.0);

    fragUV = inUV;
    fragIsColor = inIsColor;
    fragColor = inColor; // ★ 追加
}