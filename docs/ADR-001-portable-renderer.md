# ADR-001: Use Sokol over a Vulkan-only renderer

## Status

Accepted. The rendering-backend choice (Sokol) is **superseded by ADR-002**
(direct OpenGL 3.3 core / OpenGL ES 3.0). The portability goal and the
compatibility gate defined here remain in force.

## Context

The engine must run across a broad range of PC hardware and Android devices.
The initial project prototype uses SDL3 and Vulkan. Vulkan is capable, but a
Vulkan-only requirement excludes systems with insufficient driver support and
adds device, swapchain, synchronization, and shader packaging complexity before
the voxel-world core exists.

VOXOV already demonstrates the desired portability model with Sokol.

## Decision

Use Sokol as the application and rendering abstraction:

- Desktop Linux/Windows: prefer the platform-appropriate Sokol backend, with
  an OpenGL 3.3 compatibility floor where supported.
- Android: OpenGL ES 3.0.
- macOS: Metal.
- Web, if added later: WebGL2.

The game-facing renderer exposes platform-neutral buffers, images, pipelines,
and draw submission. World, meshing, networking, and NPC code must not include
Sokol headers.

## Consequences

- A wider practical compatibility envelope than Vulkan-only deployment.
- One rendering API for GL/GLES3/D3D11/Metal/WebGL2 backends.
- A generated multi-backend shader interface replaces the current Vulkan-only
  GLSL-to-SPIR-V build path.
- Existing Vulkan files remain temporarily until the Sokol triangle smoke test
  and Android lifecycle test pass; do not remove a working renderer first.

## Migration order

1. Add Sokol as a pinned, licensed third-party dependency.
2. Build a desktop and Android Sokol smoke target with the same clear color and
   input/lifecycle behavior as the Vulkan prototype.
3. Introduce a narrow renderer interface and implement the Sokol backend.
4. Port the test cube and camera controls.
5. Verify Linux and Android builds, then remove Vulkan code and dependencies.
6. Begin the voxel world-core and mesh renderer only after this portability gate
   passes.

## Compatibility gate

The minimum supported graphics feature set is OpenGL 3.3 / OpenGL ES 3.0. A
device that cannot create one of those contexts receives a clear unsupported
graphics message rather than attempting a silent software fallback.
