#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragIsColor;

layout(binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    float screenWidth;
    float screenHeight;
    vec2 padding;
    vec4 color;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(texSampler, fragUV);
    
    if (fragIsColor > 0.5) {
        outColor = texColor;
    } else {
        outColor = vec4(pc.color.rgb, texColor.r * pc.color.a);
    }
}