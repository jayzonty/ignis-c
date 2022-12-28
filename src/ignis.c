#include "ignis/ignis.h"

#include "dynamic_array.h"
#include "ignis/ignis_math.h"
#include "ignis_bezier_internal.h"
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

// Up to what ASCII code will we prepare a glyph texture for
#define MAX_GLYPHS 128 

// Number of columns in a glyph atlas
#define GLYPH_ATLAS_NUM_COLUMNS 10

// Number of keycodes to support
#define MAX_KEYS 512

// Number of mouse button codes to support
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

// Glyph data
typedef struct
{
    int advance;

    int xOffset;
    int yOffset;
    int width;
    int height;
    
    Rect uvRect;
} GlyphData;

// Font instance data
typedef struct
{
    int fontSize;

    int ascent;
    int descent;
    int lineGap;

    Texture texture;

    GlyphData glyphs[MAX_GLYPHS];
} FontInstanceData;

// Font data
typedef struct
{
    int id;
    stbtt_fontinfo fontInfo;
    unsigned char *fontBuffer;

    DynamicArray fontInstances;
} FontData;

// Input states
typedef enum
{
    INPUT_STATE_UP = 0,
    INPUT_STATE_PRESSED = 1,
    INPUT_STATE_DOWN = 2,
    INPUT_STATE_RELEASED = 3,
} InputState;

// Context
static Context context = { 0 };

// Maximum number of vertices per render batch
static uint32_t MAX_VERTICES_PER_BATCH = 10000;

// Default texture
static Texture defaultTexture = { 0 };

// Loaded fonts
static DynamicArray loadedFonts = { 0 };

// Key input states
static InputState keyInputStates[MAX_KEYS] = { 0 };

// Mouse button states
static InputState mouseButtonInputStates[MAX_MOUSE_BUTTONS] = { 0 };

// ----------------------------------------
// Internal Function declarations
// ----------------------------------------

/**
 * @brief Gets the internal font data associated with the specified font.
 * @param[in] font Font
 * @return Font data
 */
FontData* GetFontData(Font font);

/**
 * @brief Unloads the specified internal font data.
 * @param[in] fontData Pointer to the font data to unload
 */
void UnloadFontData(FontData* fontData);

/**
 * @brief Gets the font instance data associated with the specified font data and font size.
 * @param[in] fontData Internal font data
 * @param[in] fontSize Font size
 * @return Pointer to the font instance data. If the font instance data is not yet loaded, returns NULL
 */
FontInstanceData* GetFontInstanceData(FontData* fontData, int fontSize);

/**
 * @brief Loads the font instance data associated with the specified font data and font size.
 * @param[in] fontData Internal font data
 * @param[in] fontSize Font size
 * @return Pointer to the loaded font instance data
 */
FontInstanceData* LoadFontInstanceData(FontData* fontData, int fontSize);


// ----------------------------------------
// Input callback Functions
// ----------------------------------------

void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (context.isInitialized)
    {
        if (action == GLFW_PRESS)
        {
            if (keyInputStates[key] == INPUT_STATE_UP)
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
            else if (keyInputStates[key] == INPUT_STATE_DOWN)
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
        if (mouseButtonInputStates[button] == INPUT_STATE_UP)
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
        else if (mouseButtonInputStates[button] == INPUT_STATE_DOWN)
        {
            mouseButtonInputStates[button] = INPUT_STATE_RELEASED;
        }
    }
}


// ----------------------------------------
// Window callback Functions
// ----------------------------------------

void FramebufferResizedCallback(GLFWwindow* window, int width, int height)
{
    Ignis_Graphics_HandleFramebufferResized(width, height);
}


// ----------------------------------------
// Core Functions
// ----------------------------------------

bool IgnisInit(const int windowWidth, const int windowHeight, const char* windowTitle)
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

        // --- Window creation ---
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        context.window.handle = glfwCreateWindow(windowWidth, windowHeight, windowTitle, NULL, NULL);
        context.window.width = windowWidth;
        context.window.height = windowHeight;

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

Vector2 GetWindowSize(void)
{
    Vector2 ret = { context.window.width, context.window.height };
    return ret;
}

void SetWindowSize(int width, int height)
{
    glfwSetWindowSize(context.window.handle, width, height);
}

void CloseWindow(void)
{
    if (context.isInitialized)
    {
        glfwSetWindowShouldClose(context.window.handle, GLFW_TRUE);
    }
}

bool IsWindowClosed(void)
{
    if (context.isInitialized)
    {
        return glfwWindowShouldClose(context.window.handle) == GLFW_TRUE;
    }

    return true;
}

float GetCurrentTime(void)
{
    return (float)glfwGetTime();
}

float GetDeltaTime(void)
{
    return context.currentFrameStartTime - context.previousFrameStartTime;
}

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

void BeginFrame()
{
    context.previousFrameStartTime = context.currentFrameStartTime;
    context.currentFrameStartTime = GetCurrentTime();
}

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
                keyInputStates[key] = INPUT_STATE_DOWN;
            }
            else if ((glfwGetKey(context.window.handle, key) == GLFW_RELEASE) && (keyInputStates[key] == INPUT_STATE_RELEASED))
            {
                keyInputStates[key] = INPUT_STATE_UP;
            }
        }
        for (int button = 0; button < MAX_MOUSE_BUTTONS; ++button)
        {
            if ((glfwGetMouseButton(context.window.handle, button) == GLFW_PRESS) && (mouseButtonInputStates[button] == INPUT_STATE_PRESSED))
            {
                mouseButtonInputStates[button] = INPUT_STATE_DOWN;
            }
            else if ((glfwGetMouseButton(context.window.handle, button) == GLFW_RELEASE) && (mouseButtonInputStates[button] == INPUT_STATE_RELEASED))
            {
                mouseButtonInputStates[button] = INPUT_STATE_UP;
            }
        }
        glfwPollEvents();
    }
}


// ----------------------------------------
// Input Functions
// ----------------------------------------

bool IsKeyUp(Key key)
{
    return (keyInputStates[key] == INPUT_STATE_UP);
}

bool IsKeyPressed(Key key)
{
    return (keyInputStates[key] == INPUT_STATE_PRESSED);
}

bool IsKeyDown(Key key)
{
    return (keyInputStates[key] == INPUT_STATE_DOWN);
}

bool IsKeyReleased(Key key)
{
    return (keyInputStates[key] == INPUT_STATE_RELEASED);
}

bool IsMouseButtonUp(MouseButton mouseButton)
{
    return (mouseButtonInputStates[mouseButton] == INPUT_STATE_UP);
}

bool IsMouseButtonPressed(MouseButton mouseButton)
{
    return (mouseButtonInputStates[mouseButton] == INPUT_STATE_PRESSED);
}

bool IsMouseButtonDown(MouseButton mouseButton)
{
    return (mouseButtonInputStates[mouseButton] == INPUT_STATE_DOWN);
}

bool IsMouseButtonReleased(MouseButton mouseButton)
{
    return (mouseButtonInputStates[mouseButton] == INPUT_STATE_RELEASED);
}

Vector2 GetMousePosition()
{
    double x, y;
    glfwGetCursorPos(context.window.handle, &x, &y);

    Vector2 ret = { x, y };
    ret.y = context.window.height - ret.y;
    return ret;
}


// ----------------------------------------
// Drawing Functions
// ----------------------------------------

void SetClearColor(Color color)
{
    Ignis_Graphics_SetClearColor(color);
}

void BeginDrawing(void)
{
    Ignis_Graphics_BeginDrawing();
}

void EndDrawing(void)
{
    Ignis_Graphics_EndDrawing();
}

void DrawRectangle(Vector2 position, Vector2 size, Vector2 origin, Color color, float depth)
{
    Rect uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
    DrawTexturedRectangle(position, size, origin, defaultTexture, uvRect, color, depth);
}

void DrawCircle(Vector2 position, float radius, Vector2 origin, Color color, float depth)
{
    Ignis_Graphics_DrawCircle(position, radius, origin, color, depth);
}

void DrawLine(Vector2 start, Vector2 end, float width, Color color, float depth)
{
    Ignis_Graphics_DrawLine(start, end, width, color, depth);
}

void DrawBezierCurve(Vector2* controlPoints, int numControlPoints, int numSteps, Color color, float depth)
{
    Ignis_Graphics_DrawBezierCurve(controlPoints, numControlPoints, numSteps, color, depth);
}

void DrawTexturedRectangle(Vector2 position, Vector2 size, Vector2 origin, Texture texture, Rect uvRect, Color tint, float depth)
{
    Ignis_Graphics_DrawTexture(position, size, origin, texture, uvRect, tint, depth);
}

void DrawTextSubstr(Vector2 position, const char *text, size_t offset, size_t length, Font font, int fontSize, Color color, float depth)
{
    FontData* fontData = GetFontData(font);
    if (fontData == NULL)
    {
        return;
    }

    FontInstanceData* fontInstanceData = GetFontInstanceData(fontData, fontSize);
    if (fontInstanceData == NULL)
    {
        printf("[Engine] No cached font data found. Generating new font data...\n");
        fontInstanceData = LoadFontInstanceData(fontData, fontSize);
    }

    if (fontInstanceData != NULL)
    {
        float x = position.x;

        for (size_t i = 0; i < length; ++i)
        {
            unsigned char c = text[offset + i];
            GlyphData* glyphData = &fontInstanceData->glyphs[c];

            Vector2 pos = { 0 };
            pos.x = x + glyphData->xOffset;
            pos.y = position.y - (glyphData->yOffset + glyphData->height); // offsetY is negative up

            Vector2 origin = { 0 };
            Vector2 size = { glyphData->width, glyphData->height };

            DrawTexturedRectangle(pos, size, origin, fontInstanceData->texture, glyphData->uvRect, color, depth);

            x += glyphData->advance;
        }
    }
}

void DrawText(Vector2 position, const char *text, Font font, int fontSize, Color color, float depth)
{
    size_t length = strlen(text);
    DrawTextSubstr(position, text, 0, length, font, fontSize, color, depth);
}

void Draw(void)
{
    Ignis_Graphics_Draw();
}


// ----------------------------------------
// Overlap Checking Functions
// ----------------------------------------

bool IsPointInsideRect(Vector2 point, Rect rect)
{
    return (rect.x < point.x) 
            && (point.x < rect.x + rect.width)
            && (rect.y < point.y)
            && (point.y < rect.y + rect.height);
}

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

bool CheckCirclesOverlap(Vector2 centerA, float radiusA, Vector2 centerB, float radiusB)
{
    Vector2 D = Vector2Subtract(centerA, centerB);
    float sqrDist = Vector2DotProduct(D, D);
    float radiusSqr = (radiusA + radiusB) * (radiusA + radiusB);
    return sqrDist < radiusSqr;
}

bool CheckCircleRectOverlap(Vector2 circleCenter, float circleRadius, Rect rect)
{
    Vector2 closestPoint = GetClosestRectPointFromPoint(circleCenter, rect);

    float sqrDist = Vector2SquaredDistance(circleCenter, closestPoint);
    float sqrRadius = circleRadius * circleRadius;

    return sqrDist <= sqrRadius;
}


// ----------------------------------------
// Computational Geometry Functions
// ----------------------------------------

Vector2 GetClosestRectPointFromPoint(Vector2 point, Rect rect)
{
    Vector2 ret = { 0.0f, 0.0f };
    ret.x = Clamp_Float(point.x, rect.x, rect.x + rect.width);
    ret.y = Clamp_Float(point.y, rect.y, rect.y + rect.height);
    return ret;
}


// ----------------------------------------
// Random Functions
// ----------------------------------------

void SetRandomSeed(unsigned int seed)
{
    srand(seed);
    context.randomSeed = seed;
}

unsigned int GetRandomSeed()
{
    return context.randomSeed;
}

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

float GetRandomFloat(float min, float max)
{
    float randMax = (float)RAND_MAX;
    float r = rand() / randMax;
    return min + (max - min) * r;
}


// ----------------------------------------
// Texture Functions
// ----------------------------------------

Texture LoadTexture(const char *texturePath)
{
    Texture ret = { 0 };

    int width, height, numChannels = 4; // For now, force RGBA
    stbi_uc *pixels = stbi_load(texturePath, &width, &height, NULL, STBI_rgb_alpha);
    ret = LoadTextureFromData(pixels, width, height, numChannels);
    stbi_image_free(pixels);

    return ret;
}

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

void UnloadTexture(Texture texture)
{
    Ignis_Graphics_UnloadTexture(texture.id);
}


// ----------------------------------------
// Font Functions
// ----------------------------------------

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


// ----------------------------------------
// Text Functions
// ----------------------------------------

Vector2 MeasureTextSubstrSize(const char* text, size_t offset, size_t length, Font font, int fontSize)
{
    Vector2 ret = { 0 };

    FontData* fontData = GetFontData(font);
    if (fontData != NULL)
    {
        FontInstanceData* fontInstanceData = GetFontInstanceData(fontData, fontSize);
        if (fontInstanceData == NULL)
        {
            fontInstanceData = LoadFontInstanceData(fontData, fontSize);
        }

        for (size_t i = 0; i < length; ++i)
        {
            unsigned char c = text[offset + i];
            GlyphData* glyphData = &fontInstanceData->glyphs[c];

            ret.x += glyphData->advance;
            if (glyphData->height > ret.y)
            {
                ret.y = glyphData->height;
            }
        }
    }

    return ret;
}

Vector2 MeasureTextSize(const char* text, Font font, int fontSize)
{
    size_t length = strlen(text);
    return MeasureTextSubstrSize(text, 0, length, font, fontSize);
}


// ----------------------------------------
// Internal Functions
// ----------------------------------------

FontData* GetFontData(Font font)
{
    size_t numFonts = DynamicArrayGetSize(&loadedFonts);
    if (font.id < 0 || font.id >= numFonts)
    {
        return NULL;
    }

    return (FontData*)DynamicArrayGetElementAt(&loadedFonts, font.id);
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

FontInstanceData* GetFontInstanceData(FontData* fontData, int fontSize)
{
    size_t numCachedFontTextures = DynamicArrayGetSize(&fontData->fontInstances);
    for (size_t i = 0; i < numCachedFontTextures; ++i)
    {
        FontInstanceData *instanceData = (FontInstanceData*)DynamicArrayGetElementAt(&fontData->fontInstances, i);
        if (instanceData->fontSize == fontSize)
        {
            return instanceData;
        }
    }

    return NULL;
}

FontInstanceData* LoadFontInstanceData(FontData* fontData, int fontSize)
{
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

    FontInstanceData* ret = DynamicArrayGetElementAt(&fontData->fontInstances, DynamicArrayGetSize(&fontData->fontInstances) - 1);
    return ret;
}

