#include "ignis/ignis.h"

#include "dynamic_array.h"
#include "ignis/ignis_math.h"
#include "ignis_graphics.h"
#include "ignis_vulkan.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <vulkan/vulkan.h>

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_GLYPHS 128 // Up to what ASCII code will we prepare a glyph texture for
#define GLYPH_ATLAS_NUM_COLUMNS 10

#define MAX_KEYS 512
#define MAX_MOUSE_BUTTONS 3

// Window struct
typedef struct
{
    GLFWwindow *handle;
    int32_t width;
    int32_t height;
} Window;

// Context data
typedef struct
{
    bool isInitialized;
    Window window;

    int randomSeed;

    // --- Timing related variables ---
    int targetFramerate;
    float targetFrameTime;
    float previousFrameStartTime;
    float currentFrameStartTime;
} Context;

typedef struct
{
    int advance;

    int xOffset;
    int yOffset;
    int width;
    int height;
    
    Rect uvRect;
} GlyphData;

typedef struct
{
    int fontSize;

    int ascent;
    int descent;
    int lineGap;

    Texture texture;

    GlyphData glyphs[MAX_GLYPHS];
} FontInstanceData;

typedef struct
{
    int id;
    stbtt_fontinfo fontInfo;
    unsigned char *fontBuffer;

    DynamicArray fontInstances;
} FontData;

static Context context = { 0 };

static uint32_t MAX_VERTICES_PER_BATCH = 10000;

static Texture defaultTexture = { 0 };

static DynamicArray loadedFonts = { 0 };

typedef enum
{
    INPUT_STATE_RELEASED = 0,
    INPUT_STATE_PRESSED = 1,
    INPUT_STATE_HELD = 2
} InputState;

static InputState keyInputStates[MAX_KEYS] = { 0 };
static InputState mouseButtonInputStates[MAX_MOUSE_BUTTONS] = { 0 };

// ----------------------------------------
// Function declarations
// ----------------------------------------

void UnloadFontData(FontData* fontData);


// ----------------------------------------
// Input callback Functions
// ----------------------------------------

void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (context.isInitialized)
    {
        if (action == GLFW_PRESS)
        {
            if (keyInputStates[key] == INPUT_STATE_RELEASED)
            {
                keyInputStates[key] = INPUT_STATE_PRESSED;
            }
        }
        else if (action == GLFW_RELEASE)
        {
            if (keyInputStates[key] == INPUT_STATE_PRESSED)
            {
                keyInputStates[key] = INPUT_STATE_RELEASED;
            }
            else if (keyInputStates[key] == INPUT_STATE_HELD)
            {
                keyInputStates[key] = INPUT_STATE_RELEASED;
            }
        }
    }
}

void MouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        if (mouseButtonInputStates[button] == INPUT_STATE_RELEASED)
        {
            mouseButtonInputStates[button] = INPUT_STATE_PRESSED;
        }
    }
    else if (action == GLFW_RELEASE)
    {
        if (mouseButtonInputStates[button] == INPUT_STATE_PRESSED)
        {
            mouseButtonInputStates[button] = INPUT_STATE_RELEASED;
        }
        else if (mouseButtonInputStates[button] == INPUT_STATE_HELD)
        {
            mouseButtonInputStates[button] = INPUT_STATE_RELEASED;
        }
    }
}

/**
 * @brief Initializes the engine.
 */
bool IgnisInit(void)
{
    printf("[Engine] InitEngine\n");
    if (!context.isInitialized)
    {
        context.isInitialized = true;

        SetRandomSeed(time(NULL));

        if (glfwInit() != GLFW_TRUE)
        {
            printf("[Engine] Failed to initialize GLFW!\n");
            return false;
        }

        InitWindow(800, 600, "Test");

        glfwSetKeyCallback(context.window.handle, KeyCallback);
        glfwSetMouseButtonCallback(context.window.handle, MouseButtonCallback);

        if (!Ignis_Graphics_Init(context.window.width, context.window.height, context.window.handle))
        {
            IgnisCleanup();
            return false;
        }

        stbi_set_flip_vertically_on_load(1);
        
        // Set up default white texture
        {
            uint8_t pixels[4];
            pixels[0] = pixels[1] = pixels[2] = pixels[3] = 255;
            defaultTexture = LoadTextureFromData(pixels, 1, 1, 4);
        }

        // Initialize loaded fonts dynamic array
        DynamicArrayInit(&loadedFonts, sizeof(FontData));

        // Initialize bezier module
        Ignis_Bezier_Init();
    }
    else
    {
        printf("[Engine] Engine has already been initialized!\n");
    }

    return true;
}

/**
 * @brief Shuts the engine down and releases any resources used.
 */
void IgnisCleanup(void)
{
    if (context.isInitialized)
    {
        Ignis_Graphics_WaitDeviceIdle();

        size_t numFonts = DynamicArrayGetSize(&loadedFonts);
        printf("[Engine] Cleaning up %d fonts\n", numFonts);
        for (size_t i = 0; i < numFonts; ++i)
        {
            FontData* fontData = (FontData*)DynamicArrayGetElementAt(&loadedFonts, i);
            UnloadFontData(fontData);
        }
        DynamicArrayFree(&loadedFonts);

        UnloadTexture(defaultTexture);

        Ignis_Graphics_Cleanup();

        glfwDestroyWindow(context.window.handle);
        glfwTerminate();
        context.isInitialized = false;
    }
}

/**
 * @brief Initializes the main window.
 * @param[in] width Window width
 * @param[in] height Window height
 * @param[in] title Window title
 */
void InitWindow(const int width, const int height, const char *title)
{
    if (context.isInitialized)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        context.window.handle = glfwCreateWindow(width, height, title, NULL, NULL);
        context.window.width = width;
        context.window.height = height;
    }
}

/**
 * @brief Closes the main window.
 */
void CloseWindow(void)
{
    if (context.isInitialized)
    {
        glfwSetWindowShouldClose(context.window.handle, GLFW_TRUE);
    }
}

/**
 * @brief Checks whether the main window is closed.
 * @return Flag indicating whether the main window is closed or not.
 */
bool IsWindowClosed(void)
{
    if (context.isInitialized)
    {
        return glfwWindowShouldClose(context.window.handle) == GLFW_TRUE;
    }

    return true;
}

/**
 * @brief Gets the time elapsed since the engine was initialized.
 * @return Time elapsed since the engine was initialized
 */
float GetCurrentTime(void)
{
    return (float)glfwGetTime();
}

/**
 * @brief Gets the time elapsed between the previous and the current frame.
 * @return Time elapsed between the previous and current frame
 */
float GetDeltaTime(void)
{
    return context.currentFrameStartTime - context.previousFrameStartTime;
}

/**
 * @brief Sets the target framerate of the application.
 * @param[in] targetFPS Target framerate
 */
void SetTargetFPS(int targetFPS)
{
    if (targetFPS <= 0)
    {
        context.targetFramerate = 0;
        context.targetFrameTime = 0.0f;
    }
    else
    {
        context.targetFramerate = targetFPS;
        context.targetFrameTime = 1.0f / targetFPS;
    }
}

/**
 * @brief Prepares the next frame.
 */
void BeginFrame()
{
    context.previousFrameStartTime = context.currentFrameStartTime;
    context.currentFrameStartTime = GetCurrentTime();
}

/**
 * @brief Ends the current frame.
 */
void EndFrame()
{
    // Wait for the extra time
    if (context.targetFramerate > 0)
    {
        float frameEndTime = GetCurrentTime();
        float frameTime = frameEndTime - context.currentFrameStartTime;

        double sleepTime = context.targetFrameTime - frameTime;
        if (sleepTime > 0.0f)
        {
            time_t seconds = sleepTime;
            long nanoseconds = (sleepTime - seconds) * 1000000000L;

            struct timespec request = { 0 };
            request.tv_sec = seconds;
            request.tv_nsec = nanoseconds;

            // Make sure we sleep again when the sleep
            // gets interrupted
            while (nanosleep(&request, &request) == -1)
            {
                continue;
            }
        }
    }

    // Process input events
    if (context.isInitialized)
    {
        for (int key = 0; key < MAX_KEYS; ++key)
        {
            if ((glfwGetKey(context.window.handle, key) == GLFW_PRESS) && (keyInputStates[key] == INPUT_STATE_PRESSED))
            {
                keyInputStates[key] = INPUT_STATE_HELD;
            }
        }
        for (int button = 0; button < MAX_MOUSE_BUTTONS; ++button)
        {
            if ((glfwGetMouseButton(context.window.handle, button) == GLFW_PRESS) && (mouseButtonInputStates[button] == INPUT_STATE_PRESSED))
            {
                mouseButtonInputStates[button] = INPUT_STATE_HELD;
            }
        }
        glfwPollEvents();
    }
}

/**
 * @brief Checks whether the specified key is pressed during the current frame.
 * @param[in] key Key to check
 * @return True if the specified key is pressed during the current frame. 
 */
bool IsKeyPressed(Key key)
{
    return (keyInputStates[key] == INPUT_STATE_PRESSED);
}

/**
 * @brief Checks whether the specified key is currently being held down or not
 * @param[in] key Key to check
 * @return True if the specified key is currently being held down.
 */
bool IsKeyDown(Key key)
{
    return (keyInputStates[key] == INPUT_STATE_HELD);
}

/**
 * @brief Checks whether the specified mouse button is pressed during the current frame.
 * @param[in] mouseButton Mouse button to check
 * @return True if the specified mouse button is pressed during the current frame. 
 */
bool IsMouseButtonPressed(MouseButton mouseButton)
{
    return (mouseButtonInputStates[mouseButton] == INPUT_STATE_PRESSED);
}

/**
 * @brief Checks whether the specified mouse button is currently being held down or not.
 * @param[in] mouseButton Mouse button to check
 * @return True if the specified mouse button is currently being held down. 
 */
bool IsMouseButtonDown(MouseButton mouseButton)
{
    return (mouseButtonInputStates[mouseButton] == INPUT_STATE_HELD);
}

/**
 * @brief Gets the current position of the mouse cursor.
 * @return Current position of the mouse cursor
 */
Vector2 GetMousePosition()
{
    double x, y;
    glfwGetCursorPos(context.window.handle, &x, &y);

    Vector2 ret = { x, y };
    ret.y = context.window.height - ret.y;
    return ret;
}

/**
 * @brief Sets the color to be used to clear the background.
 * @param[in] color Clear color
 */
void SetClearColor(Color color)
{
    Ignis_Graphics_SetClearColor(color);
}

/** 
 * @brief Prepares the renderer for accepting drawing commands.
 */
void BeginDrawing(void)
{
    Ignis_Graphics_BeginDrawing();
}

/** 
 * @brief Signals the renderer to stop accepting drawing commands and be ready for rendering.
 */
void EndDrawing(void)
{
    Ignis_Graphics_EndDrawing();
}

/** 
 * @brief Adds a rectangle to the list of things to be drawn by the renderer.
 * @param[in] position Rectangle position
 * @param[in] size Rectangle size
 * @param[in] origin Rectangle origin ([0, 0] for lower-left, [1, 1] for upper-right)
 * @param[in] color Rectangle color
 */
void DrawRectangle(Vector2 position, Vector2 size, Vector2 origin, Color color)
{
    Rect uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
    DrawTexturedRectangle(position, size, origin, defaultTexture, uvRect, color);
}

/**
 * @brief Adds a circle to the list of things to be drawn by the renderer.
 * @param[in] position Position
 * @param[in] radius Radius
 * @param[in] origin Origin
 * @param[in] color Color
 */
void DrawCircle(Vector2 position, float radius, Vector2 origin, Color color)
{
    Ignis_Graphics_DrawCircle(position, radius, origin, color);
}

/**
 * @brief Draws a line between the starting and end point.
 * @param[in] start Start point
 * @param[in] end End point
 * @param[in] width Width
 * @param[in] color Color
 */
void DrawLine(Vector2 start, Vector2 end, float width, Color color)
{
    Ignis_Graphics_DrawLine(start, end, width, color);
}

/**
 * @brief Draws a bezier curve given the control points.
 * @param[in] controlPoints Control points
 * @param[in] numControlPoints Number of control points
 * @param[in] numSteps Number of segments to draw the curve
 * @param[in] color Color
 */
void DrawBezierCurve(Vector2* controlPoints, int numControlPoints, int numSteps, Color color)
{
    Ignis_Graphics_DrawBezierCurve(controlPoints, numControlPoints, numSteps, color);
}

/**
 * @brief Adds a textured rectangle to the list of things to be drawn by the renderer.
 * @param[in] position Rectangle position
 * @param[in] size Rectangle size
 * @param[in] origin Rectangle origin ([0, 0] for lower-left, [1, 1] for upper-right)
 * @param[in] texture Texture
 * @param[in] uvRect Rect representing the area within the texture to use
 * @param[in] tint Tint color
 */
void DrawTexturedRectangle(Vector2 position, Vector2 size, Vector2 origin, Texture texture, Rect uvRect, Color tint)
{
    Ignis_Graphics_DrawTexture(position, size, origin, texture, uvRect, tint);
}

/**
 * @brief Adds a text to the list of things to be drawn by the renderer.
 * @param[in] position Text position
 * @param[in] text Text to render
 * @param[in] font Text font
 * @param[in] fontSize Text font size
 * @param[in] color Text color
 */
void DrawText(Vector2 position, const char *text, Font font, int fontSize, Color color)
{
    FontData* fontData = (FontData*)DynamicArrayGetElementAt(&loadedFonts, font.id);
    if (fontData == NULL)
    {
        return;
    }

    FontInstanceData* fontInstanceData = NULL;

    size_t numCachedFontTextures = DynamicArrayGetSize(&fontData->fontInstances);
    for (size_t i = 0; i < numCachedFontTextures; ++i)
    {
        FontInstanceData *instanceData = (FontInstanceData*)DynamicArrayGetElementAt(&fontData->fontInstances, i);
        if (instanceData->fontSize == fontSize)
        {
            fontInstanceData = instanceData;
            break;
        }
    }

    if (fontInstanceData == NULL)
    {
        printf("[Engine] No cached font data found. Generating new font data...\n");

        FontInstanceData data = { 0 };
        data.fontSize = fontSize;

        float scale = stbtt_ScaleForPixelHeight(&fontData->fontInfo, fontSize);

        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&fontData->fontInfo, &ascent, &descent, &lineGap);
        ascent = roundf(ascent * scale);
        descent = roundf(descent * scale);
        lineGap = roundf(lineGap * scale);
        data.ascent = ascent;
        data.descent = descent;
        data.lineGap = lineGap;

        int height = ascent - descent;
        int totalHeight = height * ceilf(sqrtf(MAX_GLYPHS));
        int glyphAtlasSize = 1;
        while (glyphAtlasSize < totalHeight) // Look for the nearest power of 2
        {
            glyphAtlasSize = glyphAtlasSize << 1;
        }
        printf("[Engine] Font texture size: %d\n", glyphAtlasSize);

        uint8_t *pixels = (uint8_t*)malloc(sizeof(uint8_t) * glyphAtlasSize * glyphAtlasSize * 4); // TODO: Grayscale to save memory

        int x = 0, y = 0;
        for (unsigned char c = 0; c < MAX_GLYPHS; ++c)
        {
            GlyphData* glyph = &data.glyphs[c];

            int advance;
            stbtt_GetCodepointHMetrics(&fontData->fontInfo, c, &advance, NULL);
            glyph->advance = roundf(advance * scale);

            int xOffset, yOffset;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&fontData->fontInfo, scale, scale, c, &glyph->width, &glyph->height, &glyph->xOffset, &glyph->yOffset);

            if ((x + glyph->width) >= glyphAtlasSize)
            {
                x = 0;
                y += height;
            }

            glyph->uvRect.x = (float)x / (float)glyphAtlasSize;
            glyph->uvRect.y = (float)y / (float)glyphAtlasSize;
            glyph->uvRect.width = glyph->width / (float)glyphAtlasSize;
            glyph->uvRect.height = glyph->height / (float)glyphAtlasSize;

            for (int row = 0; row < glyph->height; ++row)
            {
                for (int col = 0; col < glyph->width; ++col)
                {
                    int atlasRow = glyph->height - row - 1;
                    int atlasIndex = ((y + atlasRow) * glyphAtlasSize + (x + col)) * 4;
                    int bitmapIndex = row * glyph->width + col;

                    pixels[atlasIndex + 0] = 255;
                    pixels[atlasIndex + 1] = 255;
                    pixels[atlasIndex + 2] = 255;
                    pixels[atlasIndex + 3] = bitmap[bitmapIndex];
                }
            }

            x += glyph->width;

            stbtt_FreeBitmap(bitmap, NULL);
        }

        data.texture = LoadTextureFromData(pixels, glyphAtlasSize, glyphAtlasSize, 4);
        DynamicArrayAdd(&fontData->fontInstances, &data);

        fontInstanceData = DynamicArrayGetElementAt(&fontData->fontInstances, DynamicArrayGetSize(&fontData->fontInstances) - 1);
    }

    if (fontInstanceData != NULL)
    {
        float x = position.x;

        unsigned long length = strlen(text);
        for (unsigned long i = 0; i < length; ++i)
        {
            unsigned char c = text[i];
            GlyphData* glyphData = &fontInstanceData->glyphs[c];

            Vector2 pos = { 0 };
            pos.x = x + glyphData->xOffset;
            pos.y = position.y - (glyphData->yOffset + glyphData->height); // offsetY is negative up

            Vector2 origin = { 0 };
            Vector2 size = { glyphData->width, glyphData->height };

            DrawTexturedRectangle(pos, size, origin, fontInstanceData->texture, glyphData->uvRect, color);

            x += glyphData->advance;
        }
    }
}

/**
 * @brief Draws all objects to the framebuffer.
 */
void Draw(void)
{
    Ignis_Graphics_Draw();
}

/**
 * @brief Checks whether the two specified rectangles are overlapping.
 * @param[in] rectA First rectangle
 * @param[in] rectB Second rectangle
 * @return True if the two rectangles are overlapping
 */
bool CheckRectsOverlap(Rect rectA, Rect rectB)
{
    Vector2 minA = { rectA.x, rectA.y };
    Vector2 maxA = { rectA.x + rectA.width, rectA.y + rectA.height };
    Vector2 minB = { rectB.x, rectB.y };
    Vector2 maxB = { rectB.x + rectB.width, rectB.y + rectB.height };

    if ((minA.x > maxB.x) || (minB.x > maxA.x) || (minA.y > maxB.y) || (minB.y > maxA.y))
    {
        return false;
    }

    return true;
}

/**
 * @brief Checks whether the two specified circles are overlapping.
 * @param[in] centerA Center of the first circle
 * @param[in] radiusA Radius of the first circle
 * @param[in] centerB Center of the second circle
 * @param[in] radiusB Radius of the second circle
 * @return True if the two circles are overlapping
 */
bool CheckCirclesOverlap(Vector2 centerA, float radiusA, Vector2 centerB, float radiusB)
{
    Vector2 D = Vector2Subtract(centerA, centerB);
    float sqrDist = Vector2DotProduct(D, D);
    float radiusSqr = (radiusA + radiusB) * (radiusA + radiusB);
    return sqrDist < radiusSqr;
}

/**
 * @brief Sets the seed for the random number generator.
 * @param[in] seed Seed
 */
void SetRandomSeed(unsigned int seed)
{
    srand(seed);
    context.randomSeed = seed;
}

/**
 * @brief Gets the seed for the random number generator.
 * @return Current seed
 */
unsigned int GetRandomSeed()
{
    return context.randomSeed;
}

/**
 * @brief Gets a random integer within [min, max].
 * @param[in] min Minimum
 * @param[in] max Maximum
 * @return Random integer within [min, max]
 */
int GetRandomInt(int min, int max)
{
    if (min > max)
    {
        int temp = min;
        min = max;
        max = temp;
    }

    int numPossibilities = max - min + 1;
    int randMax = RAND_MAX / numPossibilities;
    randMax *= numPossibilities;

    int ret;
    do
    {
        ret = rand();
    } while (ret > randMax);

    ret = ret % numPossibilities;
    ret += min;

    return ret;
}

/**
 * @brief Gets a random float within [min, max].
 * @param[in] min Minimum
 * @param[in] max Maximum
 * @return Random float within [min, max]
 */
float GetRandomFloat(float min, float max)
{
    float randMax = (float)RAND_MAX;
    float r = rand() / randMax;
    return min + (max - min) * r;
}

/**
 * @brief Loads the texture at the specified path.
 * @param[in] texturePath Path to the texture file
 * @return Loaded texture object
 */
Texture LoadTexture(const char *texturePath)
{
    Texture ret = { 0 };

    int width, height, numChannels = 4; // For now, force RGBA
    stbi_uc *pixels = stbi_load(texturePath, &width, &height, NULL, STBI_rgb_alpha);
    ret = LoadTextureFromData(pixels, width, height, numChannels);
    stbi_image_free(pixels);

    return ret;
}

/**
 * @brief Loads a texture from raw bytes data.
 * @param[in] pixels Raw pixel data
 * @param[in] width Image width
 * @param[in] height Image height
 * @param[in] numChannels Number of channels in the image
 */
Texture LoadTextureFromData(const uint8_t *pixels, int width, int height, int numChannels)
{
    Texture ret = { 0 };

    int id = Ignis_Graphics_LoadTextureFromData(pixels, width, height, numChannels);
    if (id != -1)
    {
        ret.id = id;
        ret.width = width;
        ret.height = height;
        ret.numChannels = numChannels;
    }

    return ret;
}

/**
 * @brief Unloads the specified texture.
 * @param[in] texture Texture to unload
 */
void UnloadTexture(Texture texture)
{
    Ignis_Graphics_UnloadTexture(texture.id);
}

/**
 * @brief Loads font data from the specified font file path.
 * @param[in] fontFilePath Font file path
 * @return Font data
 */
Font LoadFont(const char *fontFilePath)
{
    Font ret = { 0 };
    ret.id = -1;

    FontData fontData = { 0 };

    FILE *fontFile = fopen(fontFilePath, "rb");
    fseek(fontFile, 0, SEEK_END);
    long size = ftell(fontFile);
    fseek(fontFile, 0, SEEK_SET);

    // TODO: Add file IO error handling

    fontData.fontBuffer = malloc(size);
    fread(fontData.fontBuffer, size, 1, fontFile);
    fclose(fontFile);

    if (stbtt_InitFont(&fontData.fontInfo, fontData.fontBuffer, 0))
    {
        printf("[Engine] Loading font %s\n", fontFilePath);
        DynamicArrayInit(&fontData.fontInstances, sizeof(FontInstanceData));

        fontData.id = DynamicArrayGetSize(&loadedFonts);
        ret.id = fontData.id;
        DynamicArrayAdd(&loadedFonts, &fontData);
    }
    else
    {
        printf("[Engine] Failed to load font at path %s\n", fontFilePath);
    }

    return ret;
}

/**
 * @brief Unloads the specified font data.
 * @param[in] font Font data 
 */
void UnloadFont(Font* font)
{
    if (font->id < 0)
    {
        return;
    }

    FontData* fontData = DynamicArrayGetElementAt(&loadedFonts, font->id);
    UnloadFontData(fontData);

    font->id = -1;
}

void UnloadFontData(FontData* fontData)
{
    Ignis_Graphics_WaitDeviceIdle();

    size_t numInstances = DynamicArrayGetSize(&fontData->fontInstances);
    for (size_t i = 0; i < numInstances; ++i)
    {
        FontInstanceData* fontInstanceData = DynamicArrayGetElementAt(&fontData->fontInstances, i);
        UnloadTexture(fontInstanceData->texture);
    }
    DynamicArrayFree(&fontData->fontInstances);

    free(fontData->fontBuffer);
    fontData->fontBuffer = NULL;
}
