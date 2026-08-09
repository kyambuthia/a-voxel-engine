# Controls

## Current skeleton (this build)

The current milestone renders the generated world: deterministic voxel terrain
(grass-topped hills, stone cliffs, sand shores, water basins, caves, ores, and
trees) streams in over a few seconds and is drawn with an orbit camera above
the spawn area. There is no first-person movement yet — that is the next
milestone (player controller + block interaction).

| Action | PC | Android |
| --- | --- | --- |
| Orbit camera | Hold left mouse button + drag | Touch and drag |
| Zoom | Mouse wheel | Pinch (future) |
| Quit | `Esc` | Back button / swipe away |

The world seed is fixed (2024) in normal runs; pass a seed as the second
argument to explore other terrain: `a-voxel-engine [maxFrames] [seed]`.

## Planned full control scheme (world + player milestone)

Based on the Meese Engine spec's documented (unbound in the GCN build) scheme,
mapped to PC and Android:

| Action | Meese (GameCube, original) | PC | Android |
| --- | --- | --- | --- |
| Move | Left stick | `W` `A` `S` `D` | Left half: virtual joystick |
| Camera | C-stick | Mouse look (hold right or locked) | Right half: drag look |
| Jump | `A` | `Space` | Jump button |
| Break block | `L` | Left click | Break button |
| Place block | — | Right click | Place button |
| Show position | `Z` | `F3` | Toggle in pause |
| Pause | `Start` | `Esc` / `P` | Pause button |

These bindings will be implemented together with the first-person controller
and the block-interaction raycast.
