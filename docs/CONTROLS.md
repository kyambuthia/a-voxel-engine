# Controls

## Current skeleton (this build)

The current milestone renders the generated world with a free-fly camera:
deterministic voxel terrain (grass-topped hills, stone cliffs, sand shores,
water basins, caves, ores, and trees) streams in around you as you move.
Movement is a fly-mode (no gravity, per-axis collision keeps you out of solid
blocks); chunk generation and meshing follow the camera.

| Action | PC | Android |
| --- | --- | --- |
| Look | Move mouse (locked) | Touch and drag |
| Fly forward / back | `W` / `S` | (virtual joystick, future) |
| Strafe left / right | `A` / `D` | (virtual joystick, future) |
| Ascend / descend | `Space` / `C` | (future) |
| Boost (×4) | `Shift` (either) | (future) |
| Adjust speed | Mouse wheel (up = faster) | Pinch (future) |
| Reset speed | `R` | (future) |
| Quit | `Esc` | Back button / swipe away |

Speed ranges from 2 to 200 blocks/s (base 16; `Shift` multiplies by 4; the
mouse wheel scales it by 1.5× per notch; `R` resets to the base). The current
speed is shown in the window title. The world seed is fixed (2024) in normal
runs; pass a seed as the second argument to explore other terrain:
`a-voxel-engine [maxFrames] [seed]`.

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
