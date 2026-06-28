#include "raylib.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth  = 800;
    const int screenHeight = 600;

    InitWindow(screenWidth, screenHeight, "a-raylib-demo");

    // Define the triangle vertices
    Vector2 v1 = { screenWidth / 2.0f, 100.0f };               // top
    Vector2 v2 = { 100.0f, screenHeight - 100.0f };            // bottom-left
    Vector2 v3 = { screenWidth - 100.0f, screenHeight - 100.0f }; // bottom-right

    SetTargetFPS(60); // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            // Draw the triangle
            DrawTriangle(v1, v2, v3, MAROON);

            // Draw vertex labels
            DrawText("v1", (int)v1.x - 10, (int)v1.y - 30, 10, DARKGRAY);
            DrawText("v2", (int)v2.x - 10, (int)v2.y + 10, 10, DARKGRAY);
            DrawText("v3", (int)v3.x + 10, (int)v3.y + 10, 10, DARKGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
