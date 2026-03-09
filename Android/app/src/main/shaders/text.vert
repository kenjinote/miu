#version 450
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    float screenWidth;
    float screenHeight;
    vec4 color;
} pc;

layout(location = 0) out vec2 fragUV;

void main() {
    // 画面のピクセル座標を、Vulkanの座標系(-1.0 〜 1.0)に変換
    float x = (inPos.x / pc.screenWidth) * 2.0 - 1.0;
    float y = (inPos.y / pc.screenHeight) * 2.0 - 1.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
    fragUV = inUV;
}