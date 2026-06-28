# a-voxel-engine — Technical Issues

> Generated from code review against Vulkan 1.3 spec, Vulkan SDK best practices,
> Sascha Willems' Vulkan samples, and GPU hardware first principles.

---

## 1. UBO uploaded to wrong swapchain image buffer

**Severity:** Critical — renders stale/orphaned camera data every frame after the first cycle.

**Location:** `src/main.cpp:162` vs `src/main.cpp:165`

### Observed code

```cpp
// line 162 — BEFORE beginFrame:
uniformBuffers[ctx.currentSwapchainImage()].upload(&ubo, sizeof(ubo));

// line 165:
if (!ctx.beginFrame()) continue;

// line 168, 184, 203 — AFTER beginFrame, using the NEW image index:
VkCommandBuffer cmd = commandBuffers[ctx.currentSwapchainImage()];
// ...
VkDescriptorSet currentDescSet = pipeline.descriptorSet(ctx.currentSwapchainImage());
```

### Root cause

`ctx.currentSwapchainImage()` returns `m_currentSwapchainImage`, which at line 162 still holds the **previous frame's** swapchain image index. `beginFrame()` at line 165 calls `vkAcquireNextImageKHR` which overwrites `m_currentSwapchainImage` with the **newly acquired** image index. All subsequent per-image lookups (command buffer, framebuffer, descriptor set) use this new index — but the UBO was written to the old index's buffer.

### First-principles reasoning

There are `N` swapchain images, each with a dedicated uniform buffer and descriptor set. The descriptor set for image `i` points to `uniformBuffers[i]`. The CPU must write the current frame's MVP data into `uniformBuffers[imageIndex]` **for the specific image that will be rendered to this frame**. That image index is unknown until `vkAcquireNextImageKHR` returns it. Writing before the acquire means writing to whichever image was acquired last frame — a different image than the one about to be drawn.

### Spec reference

Vulkan 1.3 spec, `vkAcquireNextImageKHR`:

> `pImageIndex` — A pointer to a `uint32_t` that is set to the index of the swapchain image that will be used for subsequent rendering.

The index is undefined before this call and only valid after a successful return.

### Evidence

Trace for a 3-image swapchain (`minImageCount=2`, `minImageCount+1=3`):

| Frame | `m_currentSwapchainImage` before `beginFrame` | UBO written to | New idx after `acquire` | Descriptor set used for render | `uniformBuffer` actually read | Result |
|-------|------|---|---|---|---|---|
| 0 | 0 | buffer[0] | 2 | set[2] | buffer[2] | **zero-filled / garbage** |
| 1 | 2 | buffer[2] | 1 | set[1] | buffer[1] | **zero-filled / garbage** |
| 2 | 1 | buffer[1] | 0 | set[0] | buffer[0] | data from frame 0 (stale) |
| 3 | 0 | buffer[0] | 2 | set[2] | buffer[2] | data from frame 1 (stale) |

After frame 2, every frame reads data that is 1–2 frames out of date.

### Fix

Move the UBO upload to **after** `beginFrame()`:

```cpp
if (!ctx.beginFrame()) continue;
uniformBuffers[ctx.currentSwapchainImage()].upload(&ubo, sizeof(ubo));
```

---

## 2. `recreateSwapchain()` — main.cpp per-image resources never recreated

**Severity:** Critical — out-of-bounds access on any window resize that changes swapchain image count.

**Locations:**
- `src/main.cpp:111` — `commandBuffers` sized to `ctx.imageCount()`
- `src/main.cpp:84` — `uniformBuffers` sized to `ctx.imageCount()`
- `src/main.cpp:75-76` — descriptor pool + sets sized to `ctx.imageCount()`
- `src/main.cpp:93-108` — descriptor set writes sized to `ctx.imageCount()`
- `vulkan/vulkan_context.cpp:818-833` — `recreateSwapchain()`

### Observed code

```cpp
// src/main.cpp — init-time allocations (lines 84, 111):
std::vector<VulkanBuffer> uniformBuffers(ctx.imageCount());
std::vector<VkCommandBuffer> commandBuffers(ctx.imageCount());

// src/main.cpp — descriptor pool (line 75):
pipeline.createDescriptorPool(ctx.imageCount());
pipeline.createDescriptorSets(ctx.imageCount());

// vulkan_context.cpp — recreateSwapchain (line 818):
void VulkanContext::recreateSwapchain() {
    // ...
    cleanupSwapchain();
    createSwapchain();        // imageCount() might change here
    createImageViews();
    createDepthResources();
    createFramebuffers();
    // No: reallocation of uniformBuffers, commandBuffers, descriptor sets
}
```

### Root cause

`VulkanContext::recreateSwapchain()` does not notify `main.cpp` that per-image resources need reallocation. The swapchain can be recreated with a different `minImageCount` on different monitors or after driver events. `main.cpp` has no resize handler that reallocates its `std::vector`s or recreates descriptor sets.

### Spec reference

Vulkan 1.3 spec, `VkSurfaceCapabilitiesKHR::minImageCount`:

> `minImageCount` is the minimum number of images the specified device supports for a swapchain created for the surface.

There is no guarantee this stays constant across surface capability changes (monitor switch, VT switch, etc.). After `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` returns different capabilities, `createSwapchain` may negotiate a different `imageCount`.

### First-principles reasoning

The swapchain image count is a **contract** between the application and the Window System Integration (WSI). It depends on:
- GPU memory pressure
- Display configuration
- Driver version
- Surface properties

When the swapchain is recreated, the new count can differ. Any CPU-side array indexed by swapchain image index must be resized to match. If the new count exceeds the old vectors' `.size()`, `operator[]` produces undefined behavior (typically a segfault or silent memory corruption). If the count is smaller, the excess entries are wasted but safe.

### Practical impact

Most desktop drivers keep `minImageCount=2` and the `+1` heuristic produces a stable count of 3. But on mobile (Vulkan-on-Android) or with `VK_PRESENT_MODE_FIFO_KHR` vs `VK_PRESENT_MODE_MAILBOX_KHR`, the count can change. A window dragged from a 60 Hz monitor to a 144 Hz monitor may experience a different `minImageCount`.

### Fix

The `VulkanContext` needs to provide a mechanism for `main.cpp` to reallocate per-image resources. Options:

1. Return the new image count from `recreateSwapchain()` and let `main.cpp` reallocate
2. Add an observer callback: `setResizeCallback(std::function<void(uint32_t newCount)>)`
3. Make `VulkanContext` own all per-image resources internally and expose them via accessors

---

## 3. `recreateSwapchain()` — sync objects never recreated

**Severity:** Critical — out-of-bounds or stale semaphore/fence usage after resize.

**Location:** `vulkan/vulkan_context.cpp:818-833`

### Observed code

```cpp
void VulkanContext::init(GLFWwindow* window) {
    // ...
    createSwapchain();
    createImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createCommandPool();
    createSyncObjects();          // ← sized by imageCount() at init (line 48)
}

void VulkanContext::recreateSwapchain() {
    // ...
    cleanupSwapchain();
    createSwapchain();            // ← may change imageCount()
    createImageViews();
    createDepthResources();
    createFramebuffers();
    // createSyncObjects() NOT called — vectors keep old size
}
```

### Root cause

`createSyncObjects()` sizes three vectors to `imageCount()` and creates Vulkan semaphore/fence handles. `recreateSwapchain()` calls `cleanupSwapchain()` which destroys only swapchain-related resources — NOT the sync vectors. On the next frame, `beginFrame()` and `submitFrame()` index into these vectors using `m_currentSwapchainImage`, which now may be `>=` the old vector size.

### First-principles reasoning

The synchronization array length must equal the maximum possible swapchain image index + 1. If a new swapchain has 4 images but the vectors have 3 entries, accessing index 3 is an OOB write into adjacent heap memory. Even if the count stays the same, the existing semaphores are binary and after `vkDeviceWaitIdle` they are in a defined state (unsignaled) — so they can be reused. But the **size** mismatch is the bug.

### Vulkan lifecycle requirement

Each `VkSemaphore` and `VkFence` must be:
1. Created once (`vkCreateSemaphore`/`vkCreateFence`)
2. Used in a cycle of signal/wait
3. Destroyed once (`vkDestroySemaphore`/`vkDestroyFence`)

If the new image count is **smaller**, the extra semaphores leak (never destroyed). If it's **larger**, the missing ones cause OOB. The fix must destroy the old set and create a new set sized to the new count.

### Fix

Add sync object recreation to `recreateSwapchain()`:

```cpp
void VulkanContext::recreateSwapchain() {
    vkDeviceWaitIdle(m_device);
    cleanupSwapchain();

    // Destroy old sync objects
    for (size_t i = 0; i < m_imageAvailableSemaphores.size(); ++i) {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
    }

    createSwapchain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
    createSyncObjects();  // re-sizes and re-creates for new imageCount()
}
```

---

## 4. All Vulkan errors use `assert()` — vanish in release builds

**Severity:** High — silent corruption on any driver error in release configuration.

**Locations (20+):**

| File | Lines |
|---|---|
| `vulkan/vulkan_context.cpp` | 265, 301, 314, 323, 336, 457, 470, 541, 554, 568, 581, 595, 606, 619, 635, 713, 726, 744, 757, 771, 797 |
| `vulkan/vulkan_buffer.cpp` | 54, 65, 67 |
| `vulkan/vulkan_pipeline.cpp` | 118, 135, 155, 201, 216 |
| `vulkan/vulkan_shader.cpp` | 10, 33 |

### Observed pattern

```cpp
VkResult result = vkCreateSwapchainKHR(m_device, &info, nullptr, &m_swapchain);
assert(result == VK_SUCCESS);
```

### Spec reference

C17 standard, §7.2.1.1:

> If `NDEBUG` is defined as a macro name at the point in the source file where `<assert.h>` is included, the `assert` macro is defined simply as `#define assert(ignore) ((void)0)`.

Vulkan 1.3 spec, Chapter 4 (Error Handling):

> Vulkan is designed to catch programming errors at development time via validation layers, but in production the application must handle all return codes.

### First-principles reasoning

Every `VkResult` that is not `VK_SUCCESS` represents a driver-visible error state. The most common in production:
- `VK_ERROR_DEVICE_LOST` — GPU hang, TDR, or driver crash. The application should save state and recreate the device.
- `VK_ERROR_OUT_OF_HOST_MEMORY` — system OOM. Graceful degradation is better than a segfault.
- `VK_ERROR_OUT_OF_DEVICE_MEMORY` — VRAM exhaustion. Must free resources or downscale.

In debug builds, `assert` crashes with a diagnostic. In release (`-DNDEBUG`, which is standard for CMake `Release` config), the assert compiles away and the program continues with `VK_NULL_HANDLE` in every handle — causing delayed crashes in `vkDestroy*` or driver TIMEOUT.

### Industry practice

Sascha Willems' Vulkan examples use a custom `VK_CHECK_RESULT` macro that logs and aborts in debug, but handles errors gracefully in release:

```cpp
#define VK_CHECK_RESULT(f)                                                    \
    { VkResult res = (f);                                                     \
      if (res != VK_SUCCESS) {                                                \
          std::cerr << "Vulkan error " << res << " at "                       \
                    << __FILE__ << ":" << __LINE__ << "\n";                   \
          if (res == VK_ERROR_DEVICE_LOST) { /* trigger recovery */ }         \
          assert(res == VK_SUCCESS);                                          \
      }                                                                       \
    }
```

### Fix

Replace every `assert(result == VK_SUCCESS)` with a macro that:
1. Logs the error to `std::cerr` with file/line
2. Calls `assert(false)` only in debug
3. Returns an error code or triggers recovery in release

---

## 5. Per-face vertex colors are dead data

**Severity:** Low — wasted GPU memory and bandwidth, misleading code.

**Location:** `vulkan/vulkan_mesh.cpp:71-119`, `src/main.cpp:133`, `shaders/main.frag:19`

### Observed code

```cpp
// vulkan_mesh.cpp — each vertex carries a unique per-face color:
{{-h, -h,  h}, { 0, 0, 1}, {1, 0, 0}},  // front face: red
{{ h, -h,  h}, { 0, 0, 1}, {1, 0, 0}},
// ...

// src/main.cpp:133 — push constant set to a solid color:
glm::vec3 voxelColor(0.8f, 0.3f, 0.1f);  // warm orange-brown

// shaders/main.frag:19 — push constant wins if non-zero:
vec3 baseColor = length(push.voxelColor) > 0.01 ? push.voxelColor : fragColor;
```

### First-principles reasoning

`length(vec3(0.8, 0.3, 0.1)) ≈ 0.86 > 0.01`, so the per-vertex `fragColor` is **never** sampled. Each `Vertex` struct is 36 bytes (3 × `glm::vec3`). The cube has 24 vertices = 864 bytes of vertex data, of which 288 bytes (the color attribute) are carried through the entire GPU pipeline (vertex shader fetch, rasterizer interpolation, fragment shader input) and then discarded.

On a modern GPU, 288 bytes per cube is negligible. But the code is misleading: a future developer will see per-face colors and assume they're functional. If they change `voxelColor` to `vec3(0.0)`, the cube suddenly renders with face colors — a surprise behavior.

### Fix

Either:
- Remove the color attribute from `Vertex` and the pipeline's vertex input description, or
- Delete the push constant and let per-face colors render, or
- Change the threshold to `0.001` and the push color to `vec3(0.0)` to actually demonstrate per-face coloring.

---

## 6. `#include <libgen.h>` — unused

**Severity:** Cosmetic.

**Location:** `src/main.cpp:10`

`libgen.h` provides `basename()` and `dirname()`. The code uses `std::filesystem::read_symlink` and `std::filesystem::absolute` instead. Dead include.

---

## 7. `#define GLFW_INCLUDE_NONE` in two translation units without include guard

**Severity:** Cosmetic — fragile against include reordering.

**Location:** `src/main.cpp:13`, `vulkan/vulkan_context.h:5`

`GLFW_INCLUDE_NONE` suppresses GLFW's automatic inclusion of OpenGL/Vulkan headers. If `vulkan_context.h` is ever included after a GLFW header that expects GL types, or if the order of includes changes, this can cause conflicting type definitions. The canonical fix: define it once in `CMakeLists.txt` via `target_compile_definitions` or in a single common header.

---

## 8. `endFrame()` is a public no-op with no callers

**Severity:** Cosmetic.

**Location:** `vulkan/vulkan_context.h:31`, `vulkan/vulkan_context.cpp:120-122`

```cpp
void endFrame();   // declared
void VulkanContext::endFrame() { /* Nothing to do */ }  // defined, never called
```

The frame lifecycle in `main.cpp` is: `beginFrame()` → record → `submitFrame()`. There is no `endFrame()` call and no documented contract for what it should do. Either wire it in (e.g., as an `endFrame()` that calls `submitFrame` + `present`) or remove it to reduce API surface.

---

## 9. Per-image synchronization design — unconventional and fragile

**Severity:** Medium — works but deviates from spec examples and complicates resize handling.

**Location:** `vulkan/vulkan_context.cpp:84-167`

### Design description

Each swapchain image has its own dedicated semaphore pair (acquire + render-finished) and fence. The acquire semaphore is indexed by `m_acquireImageIdx`, which saves the **outgoing** image index before `vkAcquireNextImageKHR` overwrites it.

### Conventional design

The Vulkan SDK samples and Sascha Willems' repository use **per-frame-in-flight** indexing:

```cpp
// N = MAX_FRAMES_IN_FLIGHT (2 or 3, fixed, independent of swapchain)
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
uint32_t m_currentFrame = 0;

// Per-frame slot, not per-image:
VkSemaphore m_imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
VkSemaphore m_renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
VkFence     m_inFlightFences[MAX_FRAMES_IN_FLIGHT];

void beginFrame() {
    vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
    vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);
}

void submitFrame(VkCommandBuffer cmd) {
    // wait semaphore: m_imageAvailableSemaphores[m_currentFrame]
    // signal semaphore: m_renderFinishedSemaphores[m_currentFrame]
    // signal fence: m_inFlightFences[m_currentFrame]
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

### Spec analysis

Vulkan 1.3 spec, §7.4.1 — Binary Semaphore Lifetime:

> A binary semaphore is considered to be signaled after the signal operation completes and unsignaled after the wait operation begins.

The acquire semaphore signals "image is ready to render." Semantically, this is a **frame-level** event, not an **image-level** event. The semaphore tells the GPU "start the next frame." It doesn't matter which image index is returned — the semaphore's signal is consumed by the submit regardless.

The per-image design works only because:
1. There are `N` semaphores and `N` swapchain images
2. The round-robin of `vkAcquireNextImageKHR` cycles through all images before reusing any semaphore slot
3. The fence wait in `beginFrame` serializes access to each semaphore slot

But it introduces a **temporal coupling** between `beginFrame` and `submitFrame` via `m_acquireImageIdx`, which is absent from the conventional design.

### Theoretical spec concern

Per-frame designs use 2-3 frame slots regardless of image count. When a slot wraps around, all previous queue operations using that slot have completed (the fence guarantees it). In the **per-image** design:

1. `submitFrame(Frame N)` signals `renderSem[img]` and `fence[img]`
2. `vkQueuePresentKHR` waits on `renderSem[img]` — this is a **separate queue operation** from the submit
3. `beginFrame(Frame N+K)` waits on `fence[img]` — this guarantees the submit, NOT the present
4. At this point, `renderSem[img]` may still be signaled if the presentation engine hasn't consumed it yet

Vulkan spec §7.4.1:

> All elements of `pSignalSemaphores` must be in the unsignaled state when the semaphore signal operation completes.

If `renderSem[img]` is still signaled from Frame N when `submitFrame(Frame N+K)` tries to signal it again, this is a **spec violation**. In practice, with 2+ swapchain images and the round-robin cycle, the presentation engine consumes the semaphore before the same image is re-acquired. But with 1 swapchain image (possible on some tiled mobile renderers), this is a real hazard.

---

## 10. `vkResetFences` return value discarded

**Severity:** Low

**Location:** `vulkan/vulkan_context.cpp:115`

```cpp
vkResetFences(m_device, 1, &m_inFlightFences[m_currentSwapchainImage]);
```

`vkResetFences` can return `VK_ERROR_DEVICE_LOST` (§7.4.2). In a device-lost scenario, the application should not continue rendering.

---

## 11. Render pass subpass dependency uses suboptimal src/dst stage masks

**Severity:** Low — functionally correct but over-synchronizes.

**Location:** `vulkan/vulkan_context.cpp:706-714`

```cpp
VkSubpassDependency dep{};
dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
dep.dstSubpass    = 0;
dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
```

The external subpass dependency waits for `COLOR_ATTACHMENT_OUTPUT` and `EARLY_FRAGMENT_TESTS` from previous renders, and blocks those same stages in this render pass. This is correct for a swapchain image that was previously presented (the presentation engine may still be reading it). However, the `VkQueueSubmit` already provides the semaphore-based acquire→render ordering, so the render pass dependency is redundant. It doesn't cause incorrect behavior but adds a pipeline stall.

---

## Perpetrators

| Issue | Introduced in |
|---|---|
| #1 UBO upload before beginFrame | `8f35001` (initial render loop) |
| #2 per-image resources not recreated on resize | `8f35001` |
| #3 sync objects not recreated on resize | `8f35001` (partially fixed in `17113f4` for init path, not recreate) |
| #4 assert instead of error handling | `f556807` (initial Vulkan layer) |
| #5 dead vertex colors | `f556807` (makeCube) + `c04da90` (shader priority) |
| #6 unused include | `8f35001` |
| #7 duplicate GLFW_INCLUDE_NONE | `f556807` / `8f35001` |
| #8 dead endFrame | `f556807` |
| #9 per-image sync design | `8f35001` (refactored in `17113f4`) |
| #10 discarded VkResult | `f556807` |
| #11 over-synchronized subpass dep | `f556807` |
