#version 450
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in int inIsColor; // ★今回追加

layout(push_constant) uniform PushConstants {
    float screenWidth;
    float screenHeight;
    vec4 color;
} pc;

layout(location = 0) out vec2 fragUV;
layout(location = 1) flat out int fragIsColor; // ★今回追加

void main() {
    float x = (inPos.x / pc.screenWidth) * 2.0 - 1.0;
    float y = (inPos.y / pc.screenHeight) * 2.0 - 1.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
    fragUV = inUV;
    fragIsColor = inIsColor; // ★フラグを渡す
}