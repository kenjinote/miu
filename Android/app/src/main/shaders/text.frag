#version 450
layout(location = 0) in vec2 fragUV;

layout(push_constant) uniform PushConstants {
    float screenWidth;
    float screenHeight;
    vec4 color;
} pc;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    // テクスチャ(アトラス)から文字の濃さを読み取り、指定色と合成
    float alpha = texture(texSampler, fragUV).r;
    outColor = vec4(pc.color.rgb, pc.color.a * alpha);
}