#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragWorldPos;

layout(push_constant) uniform PushConstants {
    vec3 voxelColor;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    // Simple directional lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(fragNormal, lightDir), 0.0);

    // Use push constant color if non-zero, otherwise use vertex color
    vec3 baseColor = length(push.voxelColor) > 0.01 ? push.voxelColor : fragColor;

    vec3 ambient = 0.15 * baseColor;
    vec3 diffuse = 0.85 * diff * baseColor;

    outColor = vec4(ambient + diffuse, 1.0);
}
