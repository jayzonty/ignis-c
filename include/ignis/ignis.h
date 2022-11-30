#ifndef IGNIS_H
#define IGNIS_H

#include "ignis_math.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t id;

    int width;
    int height;
    int numChannels;
} Texture;

// Font data
typedef struct
{
    int id;
} Font;

// Color
typedef struct
{
    unsigned char r, g, b, a;
} Color;

typedef enum
{
    KEY_SPACE = 32,

    KEY_0 = 48,
    KEY_1 = 49,
    KEY_2 = 50,
    KEY_3 = 51,
    KEY_4 = 52,
    KEY_5 = 53,
    KEY_6 = 54,
    KEY_7 = 55,
    KEY_8 = 56,
    KEY_9 = 57,

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
    MOUSE_BUTTON_RIGHT = 1,
    MOUSE_BUTTON_MIDDLE = 2,
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
 * @brief Initializes the library.
 * @param[in] windowWidth Window width
 * @param[in] windowHeight Window height
 * @param[in] windowTitle Window title
 * @return True if the initialization was successful
 */
bool IgnisInit(const int windowWidth, const int windowHeight, const char* windowTitle);

/**
 * @brief Shuts the library down and releases any resources used.
 */
void IgnisCleanup(void);

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
 * @brief Gets the time elapsed between the previous and the current frame.
 * @return Time elapsed between the previous and current frame
 */
float GetDeltaTime(void);

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
 * @brief Checks whether the specified key is not pressed/held down.
 * @param[in] key Key to check
 * @return True if the specified key is not pressed/held down.
 */
bool IsKeyUp(Key key);

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
 * @brief Checks whether the specified key is released during the current frame.
 * @param[in] key Key to check
 * @return True if the specified key is released during the current frame. 
 */
bool IsKeyReleased(Key key);

/**
 * @brief Checks whether the specified mouse button is not pressed/held down.
 * @param[in] mouseButton Mouse button to check
 * @return True if the specified mouse button is not pressed/held down. 
 */
bool IsMouseButtonUp(MouseButton mouseButton);

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

/**
 * @brief Checks whether the specified mouse button is released during the current frame.
 * @param[in] mouseButton Mouse button to check
 * @return True if the specified mouse button is released during the current frame.
 */
bool IsMouseButtonReleased(MouseButton mouseButton);

/**
 * @brief Gets the current position of the mouse cursor.
 * @return Current position of the mouse cursor
 */
Vector2 GetMousePosition();

// ----------------------------------------
// Drawing Functions
// ----------------------------------------

/**
 * @brief Sets the color to be used to clear the background.
 * @param[in] color Clear color
 */
void SetClearColor(Color color);

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
 * @brief Draws a bezier curve given the control points.
 * @param[in] controlPoints Control points
 * @param[in] numControlPoints Number of control points
 * @param[in] numSteps Number of segments to draw the curve
 * @param[in] color Color
 */
void DrawBezierCurve(Vector2* controlPoints, int numControlPoints, int numSteps, Color color);

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
// Overlap Checking Functions
// ----------------------------------------

/**
 * @brief Checks whether the point is inside the rectangle.
 * @param[in] point Point
 * @param[in] rect Rectangle
 * @return True if the point is inside the rectangle
 */
bool IsPointInsideRect(Vector2 point, Rect rect);

/**
 * @brief Checks whether the two specified rectangles are overlapping.
 * @param[in] rectA First rectangle
 * @param[in] rectB Second rectangle
 * @return True if the two rectangles are overlapping
 */
bool CheckRectsOverlap(Rect rectA, Rect rectB);

/**
 * @brief Checks whether the two specified circles are overlapping.
 * @param[in] centerA Center of the first circle
 * @param[in] radiusA Radius of the first circle
 * @param[in] centerB Center of the second circle
 * @param[in] radiusB Radius of the second circle
 * @return True if the two circles are overlapping
 */
bool CheckCirclesOverlap(Vector2 centerA, float radiusA, Vector2 centerB, float radiusB);

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
Font LoadFont(const char* fontFilePath);

/**
 * @brief Unloads the specified font data.
 * @param[in] font Font data 
 */
void UnloadFont(Font* font);


// ----------------------------------------
// Text Functions
// ----------------------------------------

/**
 * @brief Measures the specified text using the measurements of the specified font and font size.
 * @param[in] text Text to measure
 * @param[in] font Font
 * @param[in] fontSize Font size
 * @return Size of the specified text
 */
Vector2 MeasureTextSize(const char* text, Font font, int fontSize);

// extern "C"
#ifdef __cplusplus
}
#endif

#endif // IGNIS_H
