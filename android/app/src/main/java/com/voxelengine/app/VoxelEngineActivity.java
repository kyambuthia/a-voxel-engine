package com.voxelengine.app;

import org.libsdl.app.SDLActivity;

/**
 * Custom SDL activity that loads the voxel engine native library.
 * SDLActivity by default looks for "libmain.so"; we override getLibraries()
 * to load "liba-voxel-engine.so" instead.
 */
public class VoxelEngineActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            "a-voxel-engine"
        };
    }
}
