# Clean-room voxel-engine implementation plan

## Purpose and boundary

This repository is an independent C++ voxel engine for PC and Android. It may
use publicly observable Meese behavior as product research, but it must not
copy, translate, decompile into, extract from, or depend on Meese code or
assets. The publicly inspected Meese repository does not currently provide
source or a reuse license.

The first product reference is a compact first-person voxel sandbox: movement,
camera look, jumping, breaking and placing blocks, a coordinate display, and a
pause state. Those are behavior goals, not implementation constraints.

## Current foundation

- C++17, SDL3, OpenGL (3.3 core / ES 3.0), and GLM.
- Desktop executable and Android shared-library build paths.
- SDL lifecycle handling for Android GL-surface loss and recreation.
- An OpenGL test-cube prototype behind the portable `Renderer` interface
  (see ADR-002; the Vulkan prototype and the Sokol proposal are both retired).

## Target architecture

```text
platform (SDL input, lifecycle, file access)
  -> game (fixed-step simulation, player, interactions, UI state)
    -> world (world IDs, chunk storage, generation, edits, persistence)
      -> mesh (face visibility, greedy meshing, worker jobs, upload batches)
        -> renderer (Sokol resource ownership, chunk render registry)
          -> network (authoritative server, snapshots, interest sets)
            -> npc (server-side perception, navigation, behavior)
```

Dependencies must point downward. The renderer never mutates world data; the
client never authoritatively mutates a multiplayer world; and NPC behavior
depends on world queries rather than Vulkan resources.

## Milestones

### M0 - stable vertical-slice shell

The OpenGL renderer is already in place (ADR-002, desktop + Android). Remaining
M0 work: replace the single test cube with a first-person camera,
keyboard/mouse and touch controls, pause state, and diagnostic coordinate
overlay. Keep the Android lifecycle path passing.

### M1 - local voxel world

- Introduce `BlockId`, `ChunkCoord`, `WorldPosition`, and a versioned world seed.
- Implement a bounded active chunk radius and deterministic terrain generation.
- Add raycast-based break/place operations and an edit journal.
- Build an exposed-face mesher first, with per-chunk GPU buffers and frustum
  culling.

### M2 - render and streaming performance

- Add greedy meshing and ambient occlusion as independently tested upgrades.
- Move generation/meshing into bounded, cancellable worker queues.
- Add chunk mesh caching, upload budgets, frame and memory telemetry.
- Add distance LOD only after the near-world mesh path is measured.

### M3 - multiplayer authority

- Extract a shared headless world simulation used by desktop, Android, and a
  dedicated server executable.
- Define explicit versioned packet serialization, client input commands,
  snapshots, interpolation, and edit confirmation.
- Add region/chunk interest management before increasing player counts.

### M4 - AI NPCs

- Run NPC decisions server-side.
- Use a coarse voxel navigation representation with bounded local pathfinding.
- Add perception budgets, behavior states, replication LOD, and persistence.
- Begin with one deterministic worker NPC before introducing combat or crowds.

## Acceptance gates

- PC and Android run the same world seed and produce the same local terrain
  hashes for a fixed region.
- A block edit survives restart locally, then survives server reconnect.
- Chunk generation and meshing never run on the render thread.
- Network packets include protocol and generator versions.
- NPC simulation remains deterministic under a fixed world seed and input log.
- Performance is measured with resident-chunk bytes, triangles, draw calls,
  meshing backlog, GPU upload bytes, frame time, and server tick time.

## First implementation task

Create the world-core module with coordinates, fixed-size chunk storage,
deterministic block lookup, and focused unit tests. Keep Vulkan changes out of
this first step so the world contract can be reviewed independently.
