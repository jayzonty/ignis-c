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
    FT_Library freetypeLibrary;

    int randomSeed;

    // --- Timing related variables ---
    int targetFramerate;
    float targetFrameTime;
    float previousFrameStartTime;
    float currentFrameStartTime;
} Context;

typedef struct
{
    Vector2 bearing;
    Vector2 size;
    float advance;
    Rect uvRect;
} GlyphData;

typedef struct
{
    FT_Face face;
    int fontSize;
    Texture texture;
    GlyphData glyphsData[MAX_GLYPHS];
} FontTextureData;

static Context context = { 0 };

static uint32_t MAX_VERTICES_PER_BATCH = 10000;

static Texture defaultTexture = { 0 };

static DynamicArray fontTextures = { 0 };

typedef enum
{
    INPUT_STATE_RELEASED = 0,
    INPUT_STATE_PRESSED = 1,
    INPUT_STATE_HELD = 2
} InputState;

static InputState keyInputStates[MAX_KEYS] = { 0 };
static InputState mouseButtonInputStates[MAX_MOUSE_BUTTONS] = { 0 };

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

        // Initialize freetype
        if (FT_Init_FreeType(&context.freetypeLibrary) != 0)
        {
            printf("[Engine] Failed to initialize Freetype library!\n");
        }
        DynamicArrayInit(&fontTextures, sizeof(FontTextureData));
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

        size_t numfontTextures = DynamicArrayGetSize(&fontTextures);
        for (size_t i = 0; i < numfontTextures; ++i)
        {
            FontTextureData *textureData = (FontTextureData*)DynamicArrayGetElementAt(&fontTextures, i);
            UnloadTexture(textureData->texture);
        }
        DynamicArrayFree(&fontTextures);

        FT_Done_FreeType(context.freetypeLibrary);

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
    FontTextureData *fontData = NULL;

    size_t numCachedFontTextures = DynamicArrayGetSize(&fontTextures);
    for (size_t i = 0; i < numCachedFontTextures; ++i)
    {
        FontTextureData *fontTextureData = (FontTextureData*)DynamicArrayGetElementAt(&fontTextures, i);
        if ((fontTextureData->face == font.face) && (fontTextureData->fontSize == fontSize))
        {
            fontData = fontTextureData;
            break;
        }
    }

    if (fontData == NULL)
    {
        printf("[Engine] No cached font data found. Generating new font data...\n");
        FontTextureData data = { 0 };
        data.face = font.face;
        data.fontSize = fontSize;

        FT_Set_Pixel_Sizes(font.face, 0, fontSize);
        int totalHeight = (font.face->size->metrics.height >> 6) * ceilf(sqrtf(MAX_GLYPHS));
        int glyphAtlasSize = 1;
        while (glyphAtlasSize < totalHeight) // Look for the nearest power of 2
        {
            glyphAtlasSize = glyphAtlasSize << 1;
        }
        printf("[Engine] Glyph atlas size: %d\n", glyphAtlasSize);

        uint8_t *pixels = (uint8_t*)malloc(sizeof(uint8_t) * glyphAtlasSize * glyphAtlasSize * 4);

        int x = 0, y = 0;
        for (unsigned char c = 0; c < MAX_GLYPHS; ++c)
        {
            if (FT_Load_Char(font.face, c, FT_LOAD_RENDER) != 0)
            {
                continue;
            }

            if ((x + font.face->glyph->bitmap.width) >= glyphAtlasSize)
            {
                x = 0;
                y += font.face->size->metrics.height >> 6;
            }

            data.glyphsData[c].size.x = font.face->glyph->bitmap.width;
            data.glyphsData[c].size.y = font.face->glyph->bitmap.rows;
            data.glyphsData[c].bearing.x = font.face->glyph->bitmap_left;
            data.glyphsData[c].bearing.y = font.face->glyph->bitmap_top;
            data.glyphsData[c].advance = (float)(font.face->glyph->advance.x >> 6);

            data.glyphsData[c].uvRect.x = (float)x / (float)glyphAtlasSize;
            data.glyphsData[c].uvRect.y = (float)y / (float)glyphAtlasSize;
            data.glyphsData[c].uvRect.width = data.glyphsData[c].size.x / (float)glyphAtlasSize;
            data.glyphsData[c].uvRect.height = data.glyphsData[c].size.y / (float)glyphAtlasSize;

            for (int row = 0; row < font.face->glyph->bitmap.rows; ++row)
            {
                for (int col = 0; col < font.face->glyph->bitmap.width; ++col)
                {
                    int atlasRow = font.face->glyph->bitmap.rows - row - 1;
                    int atlasIndex = ((y + atlasRow) * glyphAtlasSize + (x + col)) * 4;
                    //printf("row = %d, col = %d, atlasIndex = %d\n", row, col, atlasIndex);
                    int bufferIndex = row * font.face->glyph->bitmap.width + col;

                    pixels[atlasIndex + 0] = 255;
                    pixels[atlasIndex + 1] = 255;
                    pixels[atlasIndex + 2] = 255;
                    pixels[atlasIndex + 3] = font.face->glyph->bitmap.buffer[bufferIndex];
                }
            }

            x += font.face->glyph->bitmap.width;
        }

        data.texture = LoadTextureFromData(pixels, glyphAtlasSize, glyphAtlasSize, 4);
        DynamicArrayAdd(&fontTextures, &data);

        fontData = DynamicArrayGetElementAt(&fontTextures, DynamicArrayGetSize(&fontTextures) - 1);

        free(pixels);
    }

    if (fontData != NULL)
    {
        float x = position.x;

        unsigned long length = strlen(text);
        for (unsigned long i = 0; i < length; ++i)
        {
            unsigned char c = text[i];

            Vector2 pos = { 0 };
            pos.x = x + fontData->glyphsData[c].bearing.x;
            pos.y = position.y - (fontData->glyphsData[c].size.y - fontData->glyphsData[c].bearing.y);

            Vector2 origin = { 0 };

            DrawTexturedRectangle(pos, fontData->glyphsData[c].size, origin, fontData->texture, fontData->glyphsData[c].uvRect, color);

            x += fontData->glyphsData[c].advance;
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
bool CheckOverlap(Rect rectA, Rect rectB)
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
    if (FT_New_Face(context.freetypeLibrary, fontFilePath, 0, &ret.face) != 0)
    {
        printf("[Engine] Failed to load font at path %s\n", fontFilePath);
    }

    return ret;
}

/**
 * @brief Unloads the specified font data.
 * @param[in] font Font data 
 */
void UnloadFont(Font font)
{
    FT_Done_Face(font.face);
}

