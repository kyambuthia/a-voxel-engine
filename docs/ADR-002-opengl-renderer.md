# ADR-002: OpenGL portable renderer (supersedes the renderer choice in ADR-001)

## Status

Accepted. Implements the portability goal that ADR-001 set out; replaces the
"Sokol" backend proposal with a direct OpenGL backend.

## Context

ADR-001 named Sokol as the rendering abstraction to widen the compatibility
envelope beyond the Vulkan prototype. The engine must run on the widest
playable base: PC (Linux/Windows) and Android, with macOS plausible. Vulkan on
Android is viable on modern devices but excludes older drivers and adds
swapchain/synchronization/shader-packaging complexity before the voxel-world
core exists.

The project already vendors SDL3 and GLM as submodules. SDL3 provides GL
context creation, swap, and runtime function loading on every desktop and
mobile platform it supports, so no third-party GL loader (GLAD, glbinding) is
needed.

## Decision

Use a direct OpenGL backend behind the existing portable `Renderer` interface:

- **Desktop:** OpenGL 3.3 core context.
- **Android:** OpenGL ES 3.0 context.
- **macOS/others (future):** the same ES3-compatible GLSL subset works there.
- **Vulkan:** deferred; the `Renderer` interface is the swap point.

Implementation rules:

1. All GL entry points are resolved at runtime with
   `SDL_GL_GetProcAddress` (no link-time dependency on the GL driver, no
   `GL_GLEXT_PROTOTYPES`). See `render/opengl/gl_utils.{h,cpp}`.
2. Shader source is written once in the ES3-compatible GLSL subset
   (`layout(location)`, `in`/`out`, `texture()`, no desktop-only APIs). The
   renderer prepends `#version 330 core` on desktop and `#version 300 es` +
   default float precision on GLES. See `render/opengl/gl_shaders.h`.
3. The renderer owns the GL context, shader program, and GPU buffers. It
   exposes `initialize/shutdown/beginFrame/endFrame/onResize/onPause/onResume/
   renderFrame` — no SDL or GL types leak into `render/renderer.h`.
4. Android lifecycle: on surface loss/recreation the GL resources are rebuilt
   in `onResume()` (deleting stale names is a no-op in a fresh context, so the
   same path is safe on desktop minimize).
5. Geometry follows the Meese spec's face strategy: one quad per voxel face,
   triangulated to two triangles for GL core (which has no quads).

## Consequences

- Widest playable base: any device that can create an OpenGL 3.3 core or
  OpenGL ES 3.0 context.
- No shader compile step in the build; GLSL is embedded as version-neutral
  strings and compiled at context creation (also removes the old glslc→SPIR-V
  pipeline and its Android-asset copy step).
- Slightly more code than Sokol would have been; in return we own the GL path
  directly and the swap to Vulkan later is a bounded new backend behind the
  same `Renderer` interface.
- The compatibility gate from ADR-001 holds: devices that cannot create one of
  these contexts get a clear "unsupported graphics" message instead of a
  silent software fallback.

## Migration / swap-to-Vulkan note

When a Vulkan backend is added:

1. Add `RendererBackend::Vulkan` and a `Vulkan_Renderer` implementing the same
   interface (owning its device, swapchain, and render pass).
2. Keep the ES3-compatible shader bodies; compile them to SPIR-V for Vulkan
   (or use a GLSL-to-SPIR-V build step) with identical inputs/outputs.
3. `main.cpp` already creates the window via SDL3; use
   `SDL_Vulkan_CreateSurface` instead of `SDL_GL_CreateContext` in the Vulkan
   factory path.
4. World, meshing, game, and input code never include GL headers, so they are
   unaffected by the backend swap.

## Sokol reconsideration (review note, 2026-08-09)

ADR-001 proposed Sokol as the rendering abstraction. It was considered again
when this ADR was written and rejected for the current scope:

- The portability ADR-001 wanted is already delivered by SDL3 (GL context
  creation, swap, runtime function loading on every desktop and mobile
  platform) plus one hand-written GL/GLES backend. A second abstraction layer
  buys nothing on the two in-scope platforms (PC + Android).
- Sokol's payoff is multi-backend coverage (Metal, D3D11, WebGL2). Those
  platforms are not in scope now, and the cost of the extra dependency, its
  native-activity (non-SDL) Android shell, and its shader-convention layer
  outweighs the benefit while there is exactly one backend.
- The `Renderer` interface is the swap seam: a `Sokol_Renderer` or
  `Vulkan_Renderer` can be added later without touching world, meshing, or
  game code.

Revisit triggers: macOS/iOS (Metal), Windows D3D11, or Web targets enter
scope; or SDL's GL path proves insufficient on a device inside the
compatibility envelope. Until then, direct OpenGL stands.
