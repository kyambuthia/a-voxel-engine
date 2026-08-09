# Deterministic screenshot workflow

The engine can save the rendered OpenGL back buffer as a PNG. Capture happens
after world rendering and before presentation, so it does not depend on an
operating-system screenshot tool or window decorations.

```sh
./build/linux-debug/a-voxel-engine \
  --seed 2024 --capture artifacts/spawn-overview.png --capture-frame 48
```

`--capture` makes the process stop after the capture frame. The default capture
frame is 48; at the current four-chunk-per-frame generation budget this allows
the initial 13x13 area around the spawn camera to finish generating. Use
`--capture-frame N` to choose another frame, and `--frames N` for a bounded
interactive smoke test without a capture.

For a useful visual regression, keep the seed, capture frame, viewport, and
camera starting state fixed. Save the PNG next to a short report of the seed,
frame, and observed defect. Treat visual review as one signal alongside the
unit tests and deterministic terrain/mesh checks; a screenshot is not a
replacement for those contracts.

The capture code uses `glReadPixels` with RGBA8 data, flips OpenGL's
bottom-up rows, and writes the PNG through SDL3. It is therefore available to
both the desktop OpenGL 3.3 and Android OpenGL ES 3.0 backends.

For an interactive play session, capture a sequence instead:

```sh
./build/linux-release/a-voxel-engine \
  --capture-dir artifacts/play-session --capture-every 120
```

This saves `frame-000120.png`, `frame-000240.png`, and so on without stopping
the game. At 60 FPS, an interval of 120 is roughly one image every two seconds.
