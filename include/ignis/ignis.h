#ifndef IGNIS_H
#define IGNIS_H

#include "ignis_math.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <vulkan/vulkan_core.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t id;

    int width;
    int height;
    int numChannels;

    VkImage image;
    VkDeviceMemory memory;
    VkImageView imageView;
} Texture;

// Font data
typedef struct
{
    FT_Face face;
} Font;

// Color
typedef struct
{
    unsigned char r, g, b, a;
} Color;

typedef enum
{
    KEY_SPACE = 32,

    KEY_A = 65,
    KEY_B = 66,
    KEY_C = 67,
    KEY_D = 68,
    KEY_E = 69,
    KEY_F = 70,
    KEY_G = 71,
    KEY_H = 72,
    KEY_I = 73,
    KEY_J = 74,
    KEY_K = 75,
    KEY_L = 76,
    KEY_M = 77,
    KEY_N = 78,
    KEY_O = 79,
    KEY_P = 80,
    KEY_Q = 81,
    KEY_R = 82,
    KEY_S = 83,
    KEY_T = 84,
    KEY_U = 85,
    KEY_V = 86,
    KEY_W = 87,
    KEY_X = 88,
    KEY_Y = 89,
    KEY_Z = 90,

    KEY_RIGHT = 262,
    KEY_LEFT = 263,
    KEY_DOWN = 264,
    KEY_UP = 265,
} Key;

typedef enum
{
    MOUSE_BUTTON_LEFT = 0,
    MOUSE_BUTTON_MIDDLE = 1,
    MOUSE_BUTTON_RIGHT = 2
} MouseButton;

// When linking with C++, prevent name mangling of C functions
#ifdef __cplusplus
extern "C"
{
#endif

// ----------------------------------------
// Core Functions
// ----------------------------------------

/**
 * @brief Initializes the engine.
 */
bool IgnisInit(void);

/**
 * @brief Shuts the engine down and releases any resources used.
 */
void IgnisCleanup(void);

/**
 * @brief Initializes the main window.
 * @param[in] width Window width
 * @param[in] height Window height
 * @param[in] title Window title
 */
void InitWindow(const int width, const int height, const char *title);

/**
 * @brief Closes the main window.
 */
void CloseWindow(void);

/**
 * @brief Checks whether the main window is closed.
 * @return Flag indicating whether the main window is closed or not.
 */
bool IsWindowClosed(void);

/**
 * @brief Gets the time elapsed since the engine was initialized.
 * @return Time elapsed since the engine was initialized
 */
float GetCurrentTime(void);

/**
 * @brief Sets the target framerate of the application.
 * @param[in] targetFPS Target framerate
 */
void SetTargetFPS(int targetFPS);

/**
 * @brief Prepares the next frame.
 */
void BeginFrame();

/**
 * @brief Ends the current frame.
 */
void EndFrame();


// ----------------------------------------
// Input Functions
// ----------------------------------------

/**
 * @brief Checks whether the specified key is pressed during the current frame.
 * @param[in] key Key to check
 * @return True if the specified key is pressed during the current frame. 
 */
bool IsKeyPressed(Key key);

/**
 * @brief Checks whether the specified key is currently being held down or not
 * @param[in] key Key to check
 * @return True if the specified key is currently being held down.
 */
bool IsKeyDown(Key key);

/**
 * @brief Checks whether the specified mouse button is pressed during the current frame.
 * @param[in] mouseButton Mouse button to check
 * @return True if the specified mouse button is pressed during the current frame. 
 */
bool IsMouseButtonPressed(MouseButton mouseButton);

/**
 * @brief Checks whether the specified mouse button is currently being held down or not.
 * @param[in] mouseButton Mouse button to check
 * @return True if the specified mouse button is currently being held down. 
 */
bool IsMouseButtonDown(MouseButton mouseButton);

// ----------------------------------------
// Drawing Functions
// ----------------------------------------

/** 
 * @brief Prepares the renderer for accepting drawing commands.
 */
void BeginDrawing(void);

/** 
 * @brief Signals the renderer to stop accepting drawing commands and be ready for rendering.
 */
void EndDrawing(void);

/** 
 * @brief Adds a rectangle to the list of things to be drawn by the renderer.
 * @param[in] position Rectangle position
 * @param[in] size Rectangle size
 * @param[in] origin Rectangle origin ([0, 0] for lower-left, [1, 1] for upper-right)
 * @param[in] color Rectangle color
 */
void DrawRectangle(Vector2 position, Vector2 size, Vector2 origin, Color color);

/**
 * @brief Adds a circle to the list of things to be drawn by the renderer.
 * @param[in] position Position
 * @param[in] radius Radius
 * @param[in] origin Origin
 * @param[in] color Color
 */
void DrawCircle(Vector2 position, float radius, Vector2 origin, Color color);

/**
 * @brief Draws a line between the starting and end point.
 * @param[in] start Start point
 * @param[in] end End point
 * @param[in] width Width
 * @param[in] color Color
 */
void DrawLine(Vector2 start, Vector2 end, float width, Color color);

/**
 * @brief Adds a textured rectangle to the list of things to be drawn by the renderer.
 * @param[in] position Rectangle position
 * @param[in] size Rectangle size
 * @param[in] origin Rectangle origin ([0, 0] for lower-left, [1, 1] for upper-right)
 * @param[in] texture Texture
 * @param[in] uvRect Rect representing the area within the texture to use
 * @param[in] tint Tint color
 */
void DrawTexturedRectangle(Vector2 position, Vector2 size, Vector2 origin, Texture texture, Rect uvRect, Color tint);

/**
 * @brief Adds a text to the list of things to be drawn by the renderer.
 * @param[in] position Text position
 * @param[in] text Text to render
 * @param[in] font Text font
 * @param[in] fontSize Text font size
 * @param[in] color Text color
 */
void DrawText(Vector2 position, const char *text, Font font, int fontSize, Color color);

/**
 * @brief Draws all objects to the framebuffer.
 */
void Draw(void);

// ----------------------------------------
// Bounding Volumes Functions
// ----------------------------------------

/**
 * @brief Checks whether the two specified rectangles are overlapping.
 * @param[in] rectA First rectangle
 * @param[in] rectB Second rectangle
 * @return True if the two rectangles are overlapping
 */
bool CheckOverlap(Rect rectA, Rect rectB);

// ----------------------------------------
// Random Functions
// ----------------------------------------

/**
 * @brief Sets the seed for the random number generator.
 * @param[in] seed Seed
 */
void SetRandomSeed(unsigned int seed);

/**
 * @brief Gets the seed for the random number generator.
 * @return Current seed
 */
unsigned int GetRandomSeed();

/**
 * @brief Gets a random integer within [min, max].
 * @param[in] min Minimum
 * @param[in] max Maximum
 * @return Random integer within [min, max]
 */
int GetRandomInt(int min, int max);

/**
 * @brief Gets a random float within [min, max].
 * @param[in] min Minimum
 * @param[in] max Maximum
 * @return Random float within [min, max]
 */
float GetRandomFloat(float min, float max);

// ----------------------------------------
// Texture Functions
// ----------------------------------------

/**
 * @brief Loads the texture at the specified path.
 * @param[in] texturePath Path to the texture file
 * @return Loaded texture object
 */
Texture LoadTexture(const char *texturePath);

/**
 * @brief Loads a texture from raw bytes data.
 * @param[in] pixels Raw pixel data
 * @param[in] width Image width
 * @param[in] height Image height
 * @param[in] numChannels Number of channels in the image
 */
Texture LoadTextureFromData(const uint8_t *pixels, int width, int height, int numChannels);

/**
 * @brief Unloads the specified texture.
 * @param[in] texture Texture to unload
 */
void UnloadTexture(Texture texture);

// ----------------------------------------
// Font Functions
// ----------------------------------------

/**
 * @brief Loads font data from the specified font file path.
 * @param[in] fontFilePath Font file path
 * @return Font data
 */
Font LoadFont(const char *fontFilePath);

/**
 * @brief Unloads the specified font data.
 * @param[in] font Font data 
 */
void UnloadFont(Font font);

// extern "C"
#ifdef __cplusplus
}
#endif

#endif // IGNIS_H
