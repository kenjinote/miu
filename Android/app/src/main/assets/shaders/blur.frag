#version 450
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;
layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform BlurParams {
    float topMargin;
    float screenHeight;
} params;

void main() {
    float yPos = inUV.y * params.screenHeight;

    // マージンより下はそのまま出力して早期リターン（負荷軽減）
    if (yPos > params.topMargin) {
        outColor = texture(texSampler, inUV);
        return;
    }

    // 上に行くほどボカシを強くする (0.0 ～ 1.0)
    float blurAmount = 1.0 - (yPos / params.topMargin);
    // 最大のサンプリング半径（ピクセル）
    float maxRadius = 16.0 * blurAmount;

    vec2 texelSize = 1.0 / vec2(textureSize(texSampler, 0));
    vec4 color = vec4(0.0);
    float totalWeight = 0.0;

    // サンプリング数を抑えつつ、サンプリング間隔を広げて大きなボケを作る
    int samples = 3;
    for(int x = -samples; x <= samples; x++) {
        for(int y = -samples; y <= samples; y++) {
            vec2 offset = vec2(x, y) * (maxRadius / float(samples)) * texelSize;
            float weight = 1.0 - (length(vec2(x, y)) / float(samples * 1.414));
            if (weight > 0.0) {
                color += texture(texSampler, inUV + offset) * weight;
                totalWeight += weight;
            }
        }
    }
    outColor = color / totalWeight;
}