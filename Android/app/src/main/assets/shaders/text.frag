#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragIsColor;
layout(location = 2) in vec4 fragColor; // ★ 追加

layout(binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(texSampler, fragUV);

    if (fragIsColor > 0.5) {
        outColor = texColor; // カラー絵文字
    } else {
        // ★ 変更：テクスチャの濃さ(アルファ)に、頂点から受け取った色を掛ける
        outColor = vec4(fragColor.rgb, texColor.r * fragColor.a);
    }
}