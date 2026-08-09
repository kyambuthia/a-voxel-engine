#pragma once

// Version-neutral GLSL bodies for the engine. The renderer prepends the
// correct #version (and, on GLES, a default float precision) before compiling,
// so one source works for both OpenGL 3.3 core (desktop) and OpenGL ES 3.0
// (Android). These use only the ES3-compatible subset of GLSL: layout(location)
// inputs, in/out varyings, texture() (not texture2D), and no desktop-only APIs.
//
// This is the block-terrain mesh shader. It receives per-vertex position /
// normal / color (one quad per voxel face, expanded to two triangles) and
// applies simple directional lighting. Textures and a proper atlas come with
// the world-core milestone; per-face color keeps the smoke test asset-free.

namespace voxel::gl {

constexpr const char* kMeshVertexShader = R"glsl(
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uViewProj;

out vec3 vNormal;
out vec4 vColor;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    gl_Position = uViewProj * world;
}
)glsl";

constexpr const char* kMeshFragmentShader = R"glsl(
in vec3 vNormal;
in vec4 vColor;

out vec4 fragColor;

uniform vec3 uLightDir;

void main() {
    vec3 n = normalize(vNormal);
    float diffuse = max(dot(n, -uLightDir), 0.0);
    vec3 lit = vColor.rgb * (0.35 + 0.65 * diffuse);
    fragColor = vec4(lit, vColor.a);
}
)glsl";

}  // namespace voxel::gl
