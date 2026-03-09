#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) flat in int fragIsColor; // ★受け取る

layout(push_constant) uniform PushConstants {
    float screenWidth;
    float screenHeight;
    vec4 color;
} pc;

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(texSampler, fragUV); // ★RGBA全体を読み取る

    if (fragIsColor == 1) {
        // --- カラー絵文字の場合 ---
        // テクスチャの色をそのまま使う。全体の透明度(pc.color.a)だけ掛ける。
        outColor = vec4(texColor.rgb, texColor.a * pc.color.a);
    } else {
        // --- 通常の文字（白黒）の場合 ---
        // テクスチャのRチャンネル（濃さ）をアルファマスクとして扱い、
        // 指定された文字色（pc.color.rgb）を塗る。
        outColor = vec4(pc.color.rgb, pc.color.a * texColor.r);
    }
}