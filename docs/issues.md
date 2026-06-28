# a-voxel-engine — Technical Issues

> Generated from code review against Vulkan 1.3 spec, Vulkan SDK best practices,
> Sascha Willems' Vulkan samples, and GPU hardware first principles.

---

## 0. Windowing: GLFW → SDL3 Migration

**Status: FIXED** — GLFW removed, SDL3 vendored as git submodule.

**Details:**
- Replaced `GLFWwindow*` → `SDL_Window*` throughout
- `glfwInit()` → `SDL_Init(SDL_INIT_VIDEO)`
- `glfwCreateWindow()` → `SDL_CreateWindow()` with `SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE`
- `glfwPollEvents()` → `SDL_PollEvent()` loop with `SDL_EVENT_QUIT`
- `glfwCreateWindowSurface()` → `SDL_Vulkan_CreateSurface()`
- `glfwGetRequiredInstanceExtensions()` → `SDL_Vulkan_GetInstanceExtensions()`
- `glfwGetFramebufferSize()` → `SDL_GetWindowSizeInPixels()`
- `glfwWaitEvents()` → `SDL_WaitEvent()`
- `/proc/self/exe` → `SDL_GetBasePath()` (cross-platform)
- SDL3 added as git submodule in `dependencies/SDL`
- `CMakeLists.txt` updated: `add_subdirectory(dependencies/SDL)` + `SDL3::SDL3` target
- All `target_compile_definitions(GLFW_INCLUDE_NONE)` removed

---

## 1. UBO uploaded to wrong swapchain image buffer

**Status: FIXED** — UBO upload moved after `beginFrame()`.

**Severity:** Critical (was).

**Location:** `src/main.cpp:244-256`

### Observed code (original)

```cpp
// BEFORE beginFrame:
uniformBuffers[ctx.currentSwapchainImage()].upload(&ubo, sizeof(ubo));
if (!ctx.beginFrame()) continue;
```

### Fix applied

```cpp
if (!ctx.beginFrame()) continue;
uniformBuffers[ctx.currentSwapchainImage()].upload(&ubo, sizeof(ubo));
```

### Evidence

Trace for a 3-image swapchain:

| Frame | `m_currentSwapchainImage` before `beginFrame` | UBO written to | New idx after `acquire` | Descriptor set used for render | `uniformBuffer` actually read | Result |
|-------|------|---|---|---|---|---|
| 0 | 0 | buffer[0] | 2 | set[2] | buffer[2] | **zero-filled / garbage** |
| 1 | 2 | buffer[2] | 1 | set[1] | buffer[1] | **zero-filled / garbage** |
| 2 | 1 | buffer[1] | 0 | set[0] | buffer[0] | data from frame 0 (stale) |
| 3 | 0 | buffer[0] | 2 | set[2] | buffer[2] | data from frame 1 (stale) |

After fix: every frame writes to the correct buffer.

---

## 2. `recreateSwapchain()` — per-image resources not recreated

**Status: FIXED** — `reallocatePerImageResources()` called in main loop.

**Severity:** Critical (was).

**Location:** `src/main.cpp:39-108`, `src/main.cpp:249-253`

### Fix applied

`main.cpp` checks `ctx.imageCount()` against `uniformBuffers.size()` every frame after `beginFrame()` returns `true`, and calls `reallocatePerImageResources()` which:
1. Frees old command buffers
2. Destroys old uniform buffers
3. Destroys old descriptor pool
4. Reallocates uniform buffers at new count
5. Recreates descriptor pool + sets at new count
6. Updates descriptor set bindings
7. Reallocates command buffers at new count

---

## 3. `recreateSwapchain()` — sync objects never recreated

**Status: FIXED** — Sync objects destroyed and recreated in `recreateSwapchain()`.

**Severity:** Critical (was).

**Location:** `vulkan/vulkan_context.cpp:809-839`

### Fix applied

```cpp
void VulkanContext::recreateSwapchain() {
    vkDeviceWaitIdle(m_device);

    // Destroy old sync objects
    for (size_t i = 0; i < m_imageAvailableSemaphores.size(); ++i) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }

    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
    createSyncObjects();  // re-sized and re-created for new imageCount()

    m_currentSwapchainImage = 0;
    m_acquireImageIdx      = 0;
}
```

---

## 4. Vulkan errors use `assert()` — vanish in release builds

**Status: PARTIALLY FIXED** — `VK_CHECK` macro handles most Vulkan calls; remaining raw `assert`s converted to fatal error messages with `std::abort()`.

**Severity:** Medium (remaining risk is low).

### What was done

The `VK_CHECK` macro in `vulkan_types.h`:
```cpp
#define VK_CHECK(f)                                                       \
    do {                                                                  \
        VkResult _vr = (f);                                               \
        if (_vr != VK_SUCCESS) {                                          \
            std::cerr << "[VK_ERROR] " << #f << " returned " << _vr       \
                      << " at " << __FILE__ << ":" << __LINE__ << "\n";   \
            assert(false);                                                \
        }                                                                 \
    } while (0)
```

All `VK_CHECK` calls use this macro, which logs the error before asserting.

The following raw asset locations were converted:
- `vulkan_context.cpp:109` — `assert(result == VK_SUCCESS || ...)` → logged warning, no abort (SUBOPTIMAL is expected)
- `vulkan_context.cpp:176` — `assert(false && "...")` → `std::cerr` + `std::abort()` with file/line
- `vulkan_shader.cpp:11` — `assert(false)` → `std::cerr` + `std::abort()` with file/line

### Remaining concern

The `VK_CHECK` macro still calls `assert(false)` which is a no-op in `NDEBUG` builds. For a production engine, this should eventually be replaced with error recovery (device lost handling, OOM graceful degradation).

---

## 5. Per-face vertex colors are dead data

**Status: OPEN** — Low priority.

**Severity:** Low — wasted GPU memory and bandwidth, misleading code.

**Location:** `vulkan/vulkan_mesh.cpp:71-119`, `src/main.cpp:210`, `shaders/main.frag:19`

### Analysis

The `Vertex` struct no longer includes a color attribute (removed before this audit). However, the push constant color `glm::vec3 voxelColor(0.8f, 0.3f, 0.1f)` still overrides `fragColor` in the fragment shader because `length(vec3(0.8, 0.3, 0.1)) ≈ 0.86 > 0.01`.

### Options
- Remove the push constant entirely and rely on per-vertex color (if re-added)
- Keep as-is: push constant gives a uniform color which is the expected behavior for a voxel engine demo

---

## 6. Windowing library — SDL3 migration

**Status: FIXED** — See issue #0 above.

GLFW completely removed from the codebase. SDL3 vendored via `dependencies/SDL` git submodule.

---

## 7. `GLFW_INCLUDE_NONE` fragility

**Status: FIXED** — GLFW removed from the project entirely. No `GLFW_INCLUDE_NONE` define needed.

---

## 8. `endFrame()` is a public no-op with no callers

**Status: FIXED** — `endFrame()` was already removed from the codebase.

---

## 9. Per-image synchronization design — unconventional and fragile

**Status: OPEN** — Medium severity. Works correctly for ≥2 swapchain images but deviates from the conventional per-frame-in-flight design.

**Location:** `vulkan/vulkan_context.cpp:84-167`

### Description

Each swapchain image has its own dedicated semaphore pair (acquire + render-finished) and fence. The acquire semaphore is indexed by `m_acquireImageIdx`.

### Conventional design

The Vulkan SDK samples use **per-frame-in-flight** indexing (2-3 fixed slots, independent of swapchain image count).

### Theoretical spec concern

With 1 swapchain image (possible on tiled mobile renderers), the per-image design could cause a spec violation where `renderSem[img]` is still signaled when `submitFrame` tries to signal it again. In practice with 2+ images the round-robin prevents this.

---

## 10. `vkResetFences` return value discarded

**Status: OPEN** — Low severity.

**Location:** `vulkan/vulkan_context.cpp:115`

```cpp
VK_CHECK(vkResetFences(m_device, 1, &m_inFlightFences[m_currentSwapchainImage]));
```

Already wrapped in `VK_CHECK` — logs error + asserts on failure.

---

## 11. Render pass subpass dependency uses suboptimal src/dst stage masks

**Status: OPEN** — Low severity.

**Location:** `vulkan/vulkan_context.cpp:696-714`

Functionally correct but over-synchronizes. The `VkQueueSubmit` already provides semaphore-based acquire→render ordering, making the render pass dependency redundant.

---

## Resolution Summary

| Issue | Status | Notes |
|-------|--------|-------|
| #0 SDL3 migration | **FIXED** | GLFW → SDL3, vendored submodule, cross-platform paths |
| #1 UBO ordering | **FIXED** | Moved upload after `beginFrame()` |
| #2 Per-image resize | **FIXED** | `reallocatePerImageResources()` in main loop |
| #3 Sync on resize | **FIXED** | Destroy + recreate in `recreateSwapchain()` |
| #4 assert handling | **PARTIAL** | `VK_CHECK` covers all Vulkan calls; raw asserts → fatal error logging |
| #5 Dead vertex colors | Open | Low priority, push constant intentionally overrides |
| #6 SDL3 migration | **FIXED** | Superseded by #0 |
| #7 GLFW_INCLUDE_NONE | **FIXED** | GLFW removed |
| #8 endFrame | **FIXED** | Already removed |
| #9 Per-image sync | Open | Works correctly; refactor to per-frame design is optional |
| #10 vkResetFences | Open | Already wrapped in VK_CHECK |
| #11 Subpass dep | Open | Over-synchronization, low priority |

## Perpetrators

| Issue | Introduced in |
|---|---|
| #1 UBO upload before beginFrame | `8f35001` (initial render loop) |
| #2 per-image resources not recreated on resize | `8f35001` |
| #3 sync objects not recreated on resize | `8f35001` (partially fixed in `17113f4` for init path, not recreate) |
| #4 assert instead of error handling | `f556807` (initial Vulkan layer) |
| #5 dead vertex colors | `f556807` (makeCube) + `c04da90` (shader priority) |
| #9 per-image sync design | `8f35001` (refactored in `17113f4`) |
| #10 discarded VkResult | `f556807` |
| #11 over-synchronized subpass dep | `f556807` |
