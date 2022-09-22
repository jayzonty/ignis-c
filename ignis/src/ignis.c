#include "ignis.h"

#include "ignis_math.h"
#include "ignis_vector.h"
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

#define MAX_NUM_TEXTURE_DESCRIPTORS 256

#define MAX_GLYPHS 128 // Up to what ASCII code will we prepare a glyph texture for
#define GLYPH_ATLAS_NUM_COLUMNS 10

#define MAX_KEYS 512

// Vertex struct
typedef struct 
{
    float x, y, z;
    float r, g, b;
    float u, v;
} Vertex;

// Window struct
typedef struct
{
    GLFWwindow *handle;
    int32_t width;
    int32_t height;
} Window;

// Vulkan image wrapper
typedef struct
{
    VkImage image;
    VkDeviceMemory memory;
} VulkanImage;

// Data for one frame
typedef struct
{
    VkImage image;
    VkImageView imageView;
    VkFramebuffer framebuffer;
    VkCommandBuffer commandBuffer;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderDoneSemaphore;
    VkFence renderDoneFence;
} FrameData;

// Vulkan-related data
typedef struct
{
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice logicalDevice;

    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamilyIndex;
    VkQueue presentQueue;
    uint32_t presentQueueFamilyIndex;

    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainImageExtent;

    uint32_t maxFramesInFlight;
    uint32_t currentFrameIndex;

    VkRenderPass renderPass;
    VkPipeline graphicsPipeline;
    VkPipelineLayout graphicsPipelineLayout;

    VkImage depthBufferImage;
    VkDeviceMemory depthBufferImageMemory;
    VkImageView depthBufferImageView;

    VkCommandPool commandPool;
    FrameData *framesData;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkSampler sampler; // TODO: Move this somewhere
    VkDescriptorSet textureDescriptorSets[MAX_NUM_TEXTURE_DESCRIPTORS];
} VulkanContext;

// Context data
typedef struct
{
    bool isInitialized;
    Window window;
    VulkanContext vulkan;
    FT_Library freetypeLibrary;

    int randomSeed;
} Context;

typedef struct
{
    uint32_t firstVertexOffset;
    uint32_t numVertices;
    uint32_t textureId;
} RenderBatchUnit;

typedef struct
{
    uint32_t numActiveVertices;
    Vertex *vertices;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    Vector renderBatchUnits;
} RenderBatch;

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
static RenderBatch defaultRenderBatch = { 0 };

static uint32_t MAX_VERTICES_PER_BATCH = 10000;

static uint32_t nextTextureId = 0;
static Texture defaultTexture = { 0 };

static Vector fontTextures = { 0 };

typedef enum
{
    INPUT_STATE_RELEASED = 0,
    INPUT_STATE_PRESSED = 1,
    INPUT_STATE_HELD = 2
} InputState;

static InputState keyInputStates[MAX_KEYS] = { 0 };

// ----------------------------------------
// Vulkan Functions
// ----------------------------------------

bool InitVulkan(void);
bool InitSwapchain(void);
bool InitRenderPass(void);
bool InitDepthStencilBuffer(void);
bool InitFramebuffers(void);
bool InitCommandBuffers(void);
bool InitDescriptors(void);
bool InitGraphicsPipeline(void);
bool InitSynchronizationTools(void);
bool InitSampler(void);
void ShutdownVulkan(void);

VkPhysicalDevice GetSuitablePhysicalDevice(VkInstance instance, uint32_t numRequiredExtensions, const char **requiredExtensionNames);
bool CheckDeviceExtensionSupport(VkPhysicalDevice device, uint32_t numRequiredExtensions, const char **requiredExtensionNames);
bool GetQueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface, uint32_t *graphicsQueueFamilyIndex, uint32_t *presentQueueFamilyIndex);

bool CreateShaderModule(const char *shaderFilePath, VkDevice device, VkShaderModule *outShaderModule);

// ----------------------------------------
// Input Functions
// ----------------------------------------

void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
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

        if (!InitVulkan())
        {
            IgnisCleanup();
            return false;
        }

        // Set up default render batch
        {
            defaultRenderBatch.numActiveVertices = 0;
            defaultRenderBatch.vertices = (Vertex*)malloc(sizeof(Vertex) * MAX_VERTICES_PER_BATCH);
            VectorInit(&defaultRenderBatch.renderBatchUnits, sizeof(RenderBatchUnit));

            bool result = CreateBuffer
            (
                context.vulkan.physicalDevice,
                context.vulkan.logicalDevice,
                sizeof(Vertex) * MAX_VERTICES_PER_BATCH,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &defaultRenderBatch.vertexBuffer,
                &defaultRenderBatch.vertexBufferMemory
            );
            if (!result)
            {
                printf("[Engine] Failed to create vertex buffer for the default render batch!\n");
                return false;
            }
        }

        stbi_set_flip_vertically_on_load(1);

        defaultTexture = LoadTexture("resources/textures/white.png");

        // Initialize freetype
        if (FT_Init_FreeType(&context.freetypeLibrary) != 0)
        {
            printf("[Engine] Failed to initialize Freetype library!\n");
        }
        VectorInit(&fontTextures, sizeof(FontTextureData));
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
        vkDeviceWaitIdle(context.vulkan.logicalDevice);

        size_t numfontTextures = VectorGetSize(&fontTextures);
        for (size_t i = 0; i < numfontTextures; ++i)
        {
            FontTextureData *textureData = (FontTextureData*)VectorGetElementAt(&fontTextures, i);
            UnloadTexture(textureData->texture);
        }
        VectorFree(&fontTextures);

        FT_Done_FreeType(context.freetypeLibrary);

        UnloadTexture(defaultTexture);

        VectorFree(&defaultRenderBatch.renderBatchUnits);

        if (defaultRenderBatch.vertexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(context.vulkan.logicalDevice, defaultRenderBatch.vertexBuffer, NULL);
            defaultRenderBatch.vertexBuffer = VK_NULL_HANDLE;
            vkFreeMemory(context.vulkan.logicalDevice, defaultRenderBatch.vertexBufferMemory, NULL);
            defaultRenderBatch.vertexBufferMemory = VK_NULL_HANDLE;
        }

        free(defaultRenderBatch.vertices);
        defaultRenderBatch.vertices = NULL;

        ShutdownVulkan();

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
 * @brief Processes input events.
 */
void ProcessInputEvents(void)
{
    if (context.isInitialized)
    {
        for (int key = 0; key < MAX_KEYS; ++key)
        {
            if ((glfwGetKey(context.window.handle, key) == GLFW_PRESS) && (keyInputStates[key] == INPUT_STATE_PRESSED))
            {
                keyInputStates[key] = INPUT_STATE_HELD;
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
 * @brief Prepares the renderer for accepting drawing commands.
 */
void BeginDrawing(void)
{
    defaultRenderBatch.numActiveVertices = 0;
    VectorClear(&defaultRenderBatch.renderBatchUnits);
}

int RenderBatchUnitCompFunc(const void *a, const void *b)
{
    RenderBatchUnit *first = (RenderBatchUnit*)a;
    RenderBatchUnit *second = (RenderBatchUnit*)b;
    return first->textureId - second->textureId;
}

/** 
 * @brief Signals the renderer to stop accepting drawing commands and be ready for rendering.
 */
void EndDrawing(void)
{
    qsort
    (
        VectorGetData(&defaultRenderBatch.renderBatchUnits),
        VectorGetSize(&defaultRenderBatch.renderBatchUnits),
        sizeof(RenderBatchUnit),
        RenderBatchUnitCompFunc
    );

    VkDeviceSize bufferSize = sizeof(Vertex) * defaultRenderBatch.numActiveVertices;

    void *mem;
    vkMapMemory(context.vulkan.logicalDevice, defaultRenderBatch.vertexBufferMemory, 0, bufferSize, 0, &mem);

    size_t offset = 0;
    size_t numRenderBatchUnits = VectorGetSize(&defaultRenderBatch.renderBatchUnits);
    for (size_t i = 0; i < numRenderBatchUnits; ++i)
    {
        RenderBatchUnit *unit = (RenderBatchUnit*)VectorGetElementAt(&defaultRenderBatch.renderBatchUnits, i);
        memcpy(mem + offset * sizeof(Vertex), defaultRenderBatch.vertices + unit->firstVertexOffset, sizeof(Vertex) * unit->numVertices);
        offset += unit->numVertices;
    }

    vkUnmapMemory(context.vulkan.logicalDevice, defaultRenderBatch.vertexBufferMemory);
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
    if ((defaultRenderBatch.vertices == NULL) || (defaultRenderBatch.vertexBuffer == VK_NULL_HANDLE))
    {
        printf("[Engine] Default render batch's vertex buffer is not initialized!\n");
        return;
    }

    if (defaultRenderBatch.numActiveVertices + 6 > MAX_VERTICES_PER_BATCH)
    {
        printf("[Engine] Not enough vertices in the default render batch to draw a new rectangle!\n");
        return;
    }

    float dx[6] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f };
    float dy[6] = { 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
    for (uint32_t i = 0; i < 6; ++i)
    {
        uint32_t index = defaultRenderBatch.numActiveVertices + i;

        Vector3 vertexPosition = { 0 };
        vertexPosition.x = ((position.x + size.x * (dx[i] - origin.x)) / context.window.width) * 2.0f - 1.0f;
        vertexPosition.y = ((position.y + size.y * (dy[i] - origin.y)) / context.window.height) * 2.0f - 1.0f;
        vertexPosition.z = 0.0f;

        defaultRenderBatch.vertices[index].x = vertexPosition.x;
        defaultRenderBatch.vertices[index].y = vertexPosition.y;
        defaultRenderBatch.vertices[index].z = vertexPosition.z;
        defaultRenderBatch.vertices[index].r = tint.r / 255.0f;
        defaultRenderBatch.vertices[index].g = tint.g / 255.0f;
        defaultRenderBatch.vertices[index].b = tint.b / 255.0f;
        defaultRenderBatch.vertices[index].u = uvRect.x + uvRect.width * dx[i];
        defaultRenderBatch.vertices[index].v = uvRect.y + uvRect.height * dy[i];
    }

    RenderBatchUnit unit = { 0 };
    unit.firstVertexOffset = defaultRenderBatch.numActiveVertices;
    unit.numVertices = 6;
    unit.textureId = texture.id;
    VectorPushBack(&defaultRenderBatch.renderBatchUnits, &unit);

    defaultRenderBatch.numActiveVertices += 6;
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

    size_t numCachedFontTextures = VectorGetSize(&fontTextures);
    for (size_t i = 0; i < numCachedFontTextures; ++i)
    {
        FontTextureData *fontTextureData = (FontTextureData*)VectorGetElementAt(&fontTextures, i);
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
        VectorPushBack(&fontTextures, &data);

        fontData = VectorGetElementAt(&fontTextures, VectorGetSize(&fontTextures) - 1);

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
    // Wait for the current frame to be done rendering
    vkWaitForFences
    (
        context.vulkan.logicalDevice,
        1,
        &context.vulkan.framesData[context.vulkan.currentFrameIndex].renderDoneFence,
        VK_TRUE,
        UINT64_MAX
    );
    vkResetFences
    (
        context.vulkan.logicalDevice,
        1, 
        &context.vulkan.framesData[context.vulkan.currentFrameIndex].renderDoneFence
    );

    uint32_t nextImageIndex;
    VkResult acquireImageResult = vkAcquireNextImageKHR
    (
        context.vulkan.logicalDevice, 
        context.vulkan.swapchain, 
        UINT64_MAX, 
        context.vulkan.framesData[context.vulkan.currentFrameIndex].imageAvailableSemaphore, 
        VK_NULL_HANDLE, 
        &nextImageIndex
    );
    if ((acquireImageResult != VK_SUCCESS) && (acquireImageResult != VK_SUBOPTIMAL_KHR))
    {
        printf("[Engine] Failed to acquire next image!\n");
        glfwSetWindowShouldClose(context.window.handle, GLFW_TRUE);
        return;
    }

    // --- Build render commands ---
        
    // Start command buffer recording
    VkCommandBuffer commandBuffer = context.vulkan.framesData[context.vulkan.currentFrameIndex].commandBuffer;
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo commandBufferBeginInfo = { 0 };
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
    {
        printf("[Engine] Failed to begin recording command buffer!\n");
        glfwSetWindowShouldClose(context.window.handle, GLFW_TRUE);
        return;
    }

    // Begin render pass
    VkRenderPassBeginInfo renderPassBeginInfo = { 0 };
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = context.vulkan.renderPass;
    renderPassBeginInfo.framebuffer = context.vulkan.framesData[context.vulkan.currentFrameIndex].framebuffer;
    VkRect2D renderArea = { .offset = { .x = 0, .y = 0 }, .extent = context.vulkan.swapchainImageExtent };
    renderPassBeginInfo.renderArea = renderArea;
    VkClearValue clearValues[2] = { 0 };
    clearValues[0].color.float32[0] = 0.0f;
    clearValues[0].color.float32[1] = 0.0f;
    clearValues[0].color.float32[2] = 0.0f;
    clearValues[0].color.float32[3] = 1.0f;
    clearValues[1].depthStencil.depth = 1.0f;
    clearValues[1].depthStencil.stencil = 0;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Viewport and scissors are dynamic, so we set here as a command
    float viewportHeight = context.vulkan.swapchainImageExtent.height;
    VkViewport viewport = { 0 };
    viewport.x = 0.0f;
    viewport.y = viewportHeight;
    viewport.width = context.vulkan.swapchainImageExtent.width;
    viewport.height = -viewportHeight; // Supported from Vulkan 1.1 to switch from Y=down convention
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = { 0 };
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = context.vulkan.swapchainImageExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.vulkan.graphicsPipeline);

    VkDeviceSize offset = 0;
    VkBuffer vertexBuffers[1] = { defaultRenderBatch.vertexBuffer };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, &offset);

    uint32_t vertexOffset = 0;
    uint32_t numVertices = 0;
    uint32_t prevTex = 0;
    size_t numRenderBatchUnits = VectorGetSize(&defaultRenderBatch.renderBatchUnits);
    for (size_t i = 0; i < numRenderBatchUnits; ++i)
    {
        RenderBatchUnit *unit = (RenderBatchUnit*)VectorGetElementAt(&defaultRenderBatch.renderBatchUnits, i);
        if (i == 0)
        {
            prevTex = unit->textureId;
        }

        if (unit->textureId != prevTex)
        {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.vulkan.graphicsPipelineLayout, 0, 1, &context.vulkan.textureDescriptorSets[prevTex], 0, NULL);
            vkCmdDraw(commandBuffer, numVertices, 1, vertexOffset, 0);

            vertexOffset += numVertices;
            numVertices = 0;
        }

        numVertices += unit->numVertices;
        prevTex = unit->textureId;
    }

    if (numVertices > 0)
    {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.vulkan.graphicsPipelineLayout, 0, 1, &context.vulkan.textureDescriptorSets[prevTex], 0, NULL);
        vkCmdDraw(commandBuffer, numVertices, 1, vertexOffset, 0);
    }

    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        printf("[Engine] Failed to end recording of command buffer!\n");
        glfwSetWindowShouldClose(context.window.handle, GLFW_TRUE);
        return;
    }

    // --- Submit ---
    VkSubmitInfo submitInfo = { 0 };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[1] = { context.vulkan.framesData[context.vulkan.currentFrameIndex].imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[1] = { context.vulkan.framesData[context.vulkan.currentFrameIndex].renderDoneSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult submitResult = vkQueueSubmit
    (
        context.vulkan.graphicsQueue,
        1,
        &submitInfo,
        context.vulkan.framesData[context.vulkan.currentFrameIndex].renderDoneFence
    );
    if (submitResult != VK_SUCCESS)
    {
        printf("[Engine] Failed to submit!\n");
        glfwSetWindowShouldClose(context.window.handle, GLFW_TRUE);
        return;
    }

    // --- Present ---
    VkPresentInfoKHR presentInfo = { 0 };
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[1] = { context.vulkan.swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &nextImageIndex;

    context.vulkan.currentFrameIndex = (context.vulkan.currentFrameIndex + 1) % context.vulkan.maxFramesInFlight;

    VkResult presentResult = vkQueuePresentKHR
    (
        context.vulkan.presentQueue,
        &presentInfo
    );
    if (presentResult != VK_SUCCESS)
    {
        printf("[Engine] Failed to present!\n");
        glfwSetWindowShouldClose(context.window.handle, GLFW_TRUE);
        return;
    }
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

VkCommandBuffer CreateCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool)
{
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo commandBufferAllocInfo = { 0 };
    commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocInfo.commandPool = commandPool;
    commandBufferAllocInfo.commandBufferCount = 1;
    commandBufferAllocInfo.pNext = NULL;
    vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocInfo, &commandBuffer);

    return commandBuffer;
}

void FreeCommandBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

void BeginOneTimeCommands(VkCommandBuffer commandBuffer)
{
    VkCommandBufferBeginInfo commandBufferBeginInfo = { 0 };
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    commandBufferBeginInfo.pInheritanceInfo = NULL;
    commandBufferBeginInfo.pNext = NULL;
    vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
}

void SubmitCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = { 0 };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
}

void AppendCopyBufferCommand(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkCommandBuffer commandBuffer)
{
    if (commandBuffer == VK_NULL_HANDLE)
    {
        return;
    }

    VkBufferCopy copyRegion = { 0 };
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;

    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
}

void AppendTransitionImageLayoutCommand(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer commandBuffer)
{
    if (commandBuffer == VK_NULL_HANDLE)
    {
        return;
    }

    VkPipelineStageFlags sourceStage = 0;
    VkPipelineStageFlags destinationStage = 0;

    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    if ((oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) && (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if ((oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) && (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier
    (
        commandBuffer,
        sourceStage,
        destinationStage,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &barrier
    );
}

void AppendCopyBufferToImageCommand(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkCommandBuffer commandBuffer)
{
    if (commandBuffer == VK_NULL_HANDLE)
    {
        return;
    }

    VkBufferImageCopy region = { 0 };
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

/**
 * @brief Loads the texture at the specified path.
 * @param[in] texturePath Path to the texture file
 * @return Loaded texture object
 */
Texture LoadTexture(const char *texturePath)
{
    Texture ret = { 0 };

    if (nextTextureId >= MAX_VERTICES_PER_BATCH)
    {
        return ret;
    }

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

    if (nextTextureId >= MAX_VERTICES_PER_BATCH)
    {
        return ret;
    }

    VkDeviceSize imageSize = width * height * numChannels;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer
    (
        context.vulkan.physicalDevice,
        context.vulkan.logicalDevice,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer,
        &stagingBufferMemory
    );

    void *data;
    vkMapMemory(context.vulkan.logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(context.vulkan.logicalDevice, stagingBufferMemory);

    VkImage image;
    VkDeviceMemory memory;
    bool result = CreateImage2D
    (
        context.vulkan.physicalDevice,
        context.vulkan.logicalDevice,
        width, 
        height, 
        VK_FORMAT_R8G8B8A8_SRGB, 
        VK_IMAGE_TILING_OPTIMAL, 
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &image,
        &memory
    );
    if (result)
    {
        // Create a one-time submit command buffer for the staging buffer transfer and for image layout transistion
        VkCommandBuffer commandBuffer = CreateCommandBuffer(context.vulkan.logicalDevice, context.vulkan.commandPool);
        BeginOneTimeCommands(commandBuffer);
        AppendTransitionImageLayoutCommand(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);
        AppendCopyBufferToImageCommand(stagingBuffer, image, width, height, commandBuffer);
        AppendTransitionImageLayoutCommand(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        SubmitCommandBuffer(commandBuffer, context.vulkan.graphicsQueue);
        FreeCommandBuffer(context.vulkan.logicalDevice, context.vulkan.commandPool, commandBuffer);

        ret.id = nextTextureId++;
        ret.image = image;
        ret.memory = memory;
        ret.width = width;
        ret.height = height;
        ret.numChannels = numChannels;
        CreateImageView(context.vulkan.logicalDevice, image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, &ret.imageView);

        // Update descriptor set
        VkDescriptorImageInfo info = { 0 };
        info.imageView = ret.imageView;
        info.sampler = context.vulkan.sampler;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write = { 0 };
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = context.vulkan.textureDescriptorSets[ret.id];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &info;

        vkUpdateDescriptorSets(context.vulkan.logicalDevice, 1, &write, 0, NULL);
    }

    vkDestroyBuffer(context.vulkan.logicalDevice, stagingBuffer, NULL);
    vkFreeMemory(context.vulkan.logicalDevice, stagingBufferMemory, NULL);

    return ret;
}

/**
 * @brief Unloads the specified texture.
 * @param[in] texture Texture to unload
 */
void UnloadTexture(Texture texture)
{
    if (texture.image != VK_NULL_HANDLE)
    {
        vkDestroyImageView(context.vulkan.logicalDevice, texture.imageView, NULL);
        vkDestroyImage(context.vulkan.logicalDevice, texture.image, NULL);
        vkFreeMemory(context.vulkan.logicalDevice, texture.memory, NULL);
    }
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

bool InitVulkan(void)
{
    if (context.vulkan.instance != VK_NULL_HANDLE)
    {
        return false;
    }

    { // Create Vulkan instance
        VkApplicationInfo appInfo = { 0 };
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Pong";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
        appInfo.pEngineName = NULL;
        appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
        appInfo.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo instanceCreateInfo = { 0 };
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;
    
        uint32_t extensionCount = 0;
        const char **extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
        instanceCreateInfo.enabledExtensionCount = extensionCount;
        instanceCreateInfo.ppEnabledExtensionNames = extensionNames;

        const char *layers[1];
        layers[0] = "VK_LAYER_KHRONOS_validation";
        instanceCreateInfo.enabledLayerCount = 1;
        instanceCreateInfo.ppEnabledLayerNames = layers;

        VkResult result = vkCreateInstance(&instanceCreateInfo, NULL, &context.vulkan.instance);
        if (result != VK_SUCCESS)
        {
            printf("[Engine] Failed to create a Vulkan instance!\n");
            return false;
        }
    }

    { // Create surface
        VkResult result = glfwCreateWindowSurface(context.vulkan.instance, context.window.handle, NULL, &context.vulkan.surface);
        if (result != VK_SUCCESS)
        {
            printf("[Engine] Failed to create a Vulkan surface!\n");
            return false;
        }
    }

    uint32_t numRequiredDeviceExtensions = 1;
    const char *requiredDeviceExtensions[1];
    requiredDeviceExtensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    { // Select physical device
        VkPhysicalDevice physicalDevice = GetSuitablePhysicalDevice(context.vulkan.instance, numRequiredDeviceExtensions, requiredDeviceExtensions);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            printf("[Engine] Failed to find a suitable GPU!\n");
            return false;
        }

        context.vulkan.physicalDevice = physicalDevice;

        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
        printf("[Engine] Selected GPU: %s\n", physicalDeviceProperties.deviceName);
    }

    { // Create logical device and queues
        if (!GetQueueFamilyIndices(
                context.vulkan.physicalDevice,
                context.vulkan.surface,
                &context.vulkan.graphicsQueueFamilyIndex,
                &context.vulkan.presentQueueFamilyIndex))
        {
            printf("[Engine] Failed to find queue family indices for graphics and presentation");
            return false;
        }

        uint32_t queueFamilyIndices[2];
        queueFamilyIndices[0] = context.vulkan.graphicsQueueFamilyIndex;
        queueFamilyIndices[1] = context.vulkan.presentQueueFamilyIndex;

        uint32_t numQueuesToCreate = 2;
        if (context.vulkan.graphicsQueueFamilyIndex == context.vulkan.presentQueueFamilyIndex)
        {
            numQueuesToCreate = 1;
        }

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo *queueCreateInfoStructs = (VkDeviceQueueCreateInfo*)malloc(sizeof(VkDeviceQueueCreateInfo) * numQueuesToCreate);
        for (uint32_t i = 0; i < numQueuesToCreate; ++i)
        {
            queueCreateInfoStructs[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfoStructs[i].queueFamilyIndex = queueFamilyIndices[i];
            queueCreateInfoStructs[i].queueCount = 1;
            queueCreateInfoStructs[i].flags = 0;
            queueCreateInfoStructs[i].pQueuePriorities = &queuePriority;
            queueCreateInfoStructs[i].pNext = NULL;
        }

        VkPhysicalDeviceFeatures physicalDeviceFeatures = { 0 };
        physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo logicalDeviceInfo = { 0 };
        logicalDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        logicalDeviceInfo.pEnabledFeatures = &physicalDeviceFeatures;
        logicalDeviceInfo.queueCreateInfoCount = numQueuesToCreate;
        logicalDeviceInfo.pQueueCreateInfos = queueCreateInfoStructs;
        logicalDeviceInfo.enabledExtensionCount = numRequiredDeviceExtensions;
        logicalDeviceInfo.ppEnabledExtensionNames = requiredDeviceExtensions;
        logicalDeviceInfo.enabledLayerCount = 0;
        if (vkCreateDevice(context.vulkan.physicalDevice, &logicalDeviceInfo, NULL, &context.vulkan.logicalDevice) != VK_SUCCESS)
        {
            free(queueCreateInfoStructs);
            printf("[Engine] Failed to create logical device!\n");
            return false;
        }

        free(queueCreateInfoStructs);

        // --- Get handles to the device queues that we just created ---
        vkGetDeviceQueue(context.vulkan.logicalDevice, context.vulkan.graphicsQueueFamilyIndex, 0, &context.vulkan.graphicsQueue);
        vkGetDeviceQueue(context.vulkan.logicalDevice, context.vulkan.presentQueueFamilyIndex, 0, &context.vulkan.presentQueue);
    }

    if (!InitSwapchain())
    {
        return false;
    }

    if (!InitRenderPass())
    {
        return false;
    }

    if (!InitDepthStencilBuffer())
    {
        return false;
    }

    if (!InitFramebuffers())
    {
        return false;
    }

    if (!InitCommandBuffers())
    {
        return false;
    }

    if (!InitDescriptors())
    {
        return false;
    }

    if (!InitGraphicsPipeline())
    {
        return false;
    }

    if (!InitSynchronizationTools())
    {
        return false;
    }

    // TODO: Move
    if (!InitSampler())
    {
        return false;
    }

    return true;
}

bool InitSwapchain(void)
{
    // --- Preparations for swapchain creation ---

    // Query surface capabilities (can also do this as a criteria when selecting a physical device)
    // For now, we'll just do a delayed check.
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.vulkan.physicalDevice, context.vulkan.surface, &surfaceCapabilities);

    // Query supported formats
    uint32_t numAvailableSurfaceFormats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context.vulkan.physicalDevice, context.vulkan.surface, &numAvailableSurfaceFormats, NULL);
    if (numAvailableSurfaceFormats == 0)
    {
        printf("[Engine] Selected physical device has no supported surface formats!\n");
        return false;
    }
    VkSurfaceFormatKHR *availableSurfaceFormats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * numAvailableSurfaceFormats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context.vulkan.physicalDevice, context.vulkan.surface, &numAvailableSurfaceFormats, availableSurfaceFormats);

    // Query presentation modes
    uint32_t numAvailablePresentModes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context.vulkan.physicalDevice, context.vulkan.surface, &numAvailablePresentModes, NULL);
    if (numAvailablePresentModes == 0)
    {
        free(availableSurfaceFormats);
        printf("[Engine] Selected physical device has no supported present modes!\n");
        return false;
    }
    VkPresentModeKHR *availablePresentModes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * numAvailablePresentModes);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context.vulkan.physicalDevice, context.vulkan.surface, &numAvailablePresentModes, availablePresentModes);

    // Select the preferred surface format.
    // Ideally, a format that supports BGRA with SRGB colorspace, but if we can't find such a format, just use the first one
    VkSurfaceFormatKHR selectedSurfaceFormat = availableSurfaceFormats[0];
    for (size_t i = 1; i < numAvailableSurfaceFormats; ++i)
    {
        if ((availableSurfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB) && (availableSurfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
        {
            selectedSurfaceFormat = availableSurfaceFormats[i];
            break;
        }
    }

    // Select preferred present mode.
    // By default, GPUs should support VK_PRESENT_MODE_FIFO_KHR at the bare minimum, but
    // if we can find VK_PRESENT_MODE_MAILBOX_KHR, then we use that
    VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (size_t i = 0; i < numAvailablePresentModes; ++i)
    {
        if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            selectedPresentMode = availablePresentModes[i];
            break;
        }
    }

    // Calculate swapchain extent
    VkExtent2D swapchainImageExtent;
    if (surfaceCapabilities.currentExtent.width != UINT32_MAX)
    {
        // Use the extent provided to us by Vulkan
        swapchainImageExtent = surfaceCapabilities.currentExtent;
    }
    else
    {
        // Use the framebuffer size provided by GLFW as the extent.
        // However, we make sure that the extent is within the capabilities of the surface
        int width, height;
        glfwGetFramebufferSize(context.window.handle, &width, &height);

        swapchainImageExtent.width = Clamp(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        swapchainImageExtent.height = Clamp(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
    }

    // --- Create swapchain ---

    // Prepare swapchain create struct
    uint32_t numSwapchainImages = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0)
    {
        numSwapchainImages = Min(numSwapchainImages, surfaceCapabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = context.vulkan.surface;
    swapchainCreateInfo.minImageCount = numSwapchainImages;
    swapchainCreateInfo.imageFormat = selectedSurfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = selectedSurfaceFormat.colorSpace;
    swapchainCreateInfo.presentMode = selectedPresentMode;
    swapchainCreateInfo.clipped = VK_TRUE; // Clip pixels that are obscured, e.g., by other windows
    swapchainCreateInfo.imageExtent = swapchainImageExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Can use VK_IMAGE_USAGE_TRANSFER_DST_BIT for rendering to an offscreen buffer (e.g., post-processing)
    if (context.vulkan.graphicsQueueFamilyIndex != context.vulkan.presentQueueFamilyIndex)
    {
        // Graphics queue and present queue are different (in some GPUs, they can be the same).
        // In this case, we use VK_SHARING_MODE_CONCURRENT, which means swapchain images can be owned by
        // multiple queue famillies without the need for transfer of ownership.
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;

        // We also need to specify which queue families concurrently own a swapchain image 
        uint32_t indices[] = { context.vulkan.graphicsQueueFamilyIndex, context.vulkan.presentQueueFamilyIndex };
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = indices;
    }
    else
    {
        // If graphics queue and present queue are the same, set to exclusive mode since
        // we don't need to transfer ownership normally. This also offers the best performance.
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    // In case we want to apply a transformation to be applied to all swapchain images.
    // If not, we just use the current transformation of the surface capabilities struct.
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    // Blending with other windows. (We almost always ignore)
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // In case the previous swap chain becomes invalid. In that case, we need to supply the old swapchain
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    // Actually create the swapchain
    if (vkCreateSwapchainKHR(context.vulkan.logicalDevice, &swapchainCreateInfo, NULL, &context.vulkan.swapchain) != VK_SUCCESS)
    {
        free(availableSurfaceFormats);
        free(availablePresentModes);
        printf("[Engine] Failed to create swapchain!\n");
        return false;
    }

    // Retrieve swapchain images, and store the image format and extent
    vkGetSwapchainImagesKHR(context.vulkan.logicalDevice, context.vulkan.swapchain, &numSwapchainImages, NULL);
    VkImage *swapchainImages = (VkImage*)malloc(sizeof(VkImage) * numSwapchainImages);
    vkGetSwapchainImagesKHR(context.vulkan.logicalDevice, context.vulkan.swapchain, &numSwapchainImages, swapchainImages);
    context.vulkan.swapchainImageFormat = selectedSurfaceFormat.format;
    context.vulkan.swapchainImageExtent = swapchainImageExtent;

    VkImageView *swapchainImageViews = (VkImageView*)malloc(sizeof(VkImage) * numSwapchainImages);
    context.vulkan.framesData = (FrameData*)malloc(sizeof(FrameData) * numSwapchainImages);
    for (size_t i = 0; i < numSwapchainImages; ++i)
    {
        context.vulkan.framesData[i].image = swapchainImages[i];
        CreateImageView(context.vulkan.logicalDevice, context.vulkan.framesData[i].image, context.vulkan.swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, &context.vulkan.framesData[i].imageView);
        if (context.vulkan.framesData[i].imageView == VK_NULL_HANDLE)
        {
            free(swapchainImages);
            free(availableSurfaceFormats);
            free(availablePresentModes);
            printf("[Engine] Failed to create swapchain image views!\n");
            return false;
        }
    }

    context.vulkan.maxFramesInFlight = numSwapchainImages;

    free(swapchainImages);
    free(availableSurfaceFormats);
    free(availablePresentModes);

    return true;
}

bool InitRenderPass(void)
{
    // Setup color attachment for the framebuffer
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = context.vulkan.swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear framebuffer upon being loaded
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Keep rendered contents in memory after
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // We don't use stencil testing, so we don't care for now
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // We don't use stencil testing, so we don't care for now
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentReference = {};
    colorAttachmentReference.attachment = 0; // This is reflected in the "layout(location = 0) out color" in the fragment shader
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Setup depth attachment for the framebuffer
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT; // TODO: Create a routine for finding the suitable depth format
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentReference = {};
    depthAttachmentReference.attachment = 1;
    depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Rendering subpass (not compute subpass)
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    VkSubpassDependency subpassDependency = {};
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstSubpass = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDependency depthDependency = {};
    depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.srcAccessMask = 0;
    depthDependency.dstSubpass = 0;
    depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[2] = { 0 };
    attachments[0] = colorAttachment;
    attachments[1] = depthAttachment;

    VkSubpassDependency dependencies[2] = { 0 };
    dependencies[0] = subpassDependency;
    dependencies[1] = depthDependency;

    VkRenderPassCreateInfo renderPassCreateInfo = {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 2;
    renderPassCreateInfo.pAttachments = attachments;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 2; 
    renderPassCreateInfo.pDependencies = dependencies;

    if (vkCreateRenderPass(context.vulkan.logicalDevice, &renderPassCreateInfo, NULL, &context.vulkan.renderPass) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create render pass!\n");
        return false;
    }

    return true;
}

bool InitDepthStencilBuffer(void)
{
    // TODO: We can use the VK_FORMAT_D32_SFLOAT format for the depth buffer, but we can add more
    // flexibility by querying which format is supported.
    // If somehow we will need the stencil buffer in the future, we'll have to use another format
    // VK_FORMAT_D32_SFLOAT_S8_UINT or VK_FORMAT_D24_UNORM_S8_UINT.
    bool result = CreateImage2D
    (
        context.vulkan.physicalDevice,
        context.vulkan.logicalDevice,
        context.vulkan.swapchainImageExtent.width, 
        context.vulkan.swapchainImageExtent.height, 
        VK_FORMAT_D32_SFLOAT, 
        VK_IMAGE_TILING_OPTIMAL, 
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &context.vulkan.depthBufferImage,
        &context.vulkan.depthBufferImageMemory
    );
    if (!result)
    {
        printf("[Engine] Failed to create Vulkan image for the depth buffer!\n");
        return false;
    }

    CreateImageView(context.vulkan.logicalDevice, context.vulkan.depthBufferImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, &context.vulkan.depthBufferImageView);

    // TODO: Can transition image layout of the depth buffer image if we so choose,
    // but we'll already handle this in the render pass.

    return true;
}

bool InitFramebuffers(void)
{
    for (size_t i = 0; i < context.vulkan.maxFramesInFlight; ++i)
    {
        VkImageView attachments[2] = { 0 };
        attachments[0] = context.vulkan.framesData[i].imageView;
        attachments[1] = context.vulkan.depthBufferImageView;

        VkFramebufferCreateInfo framebufferCreateInfo = {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = context.vulkan.renderPass;
        framebufferCreateInfo.attachmentCount = 2;
        framebufferCreateInfo.pAttachments = attachments;
        framebufferCreateInfo.width = context.vulkan.swapchainImageExtent.width;
        framebufferCreateInfo.height = context.vulkan.swapchainImageExtent.height;
        framebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(context.vulkan.logicalDevice, &framebufferCreateInfo, NULL, &context.vulkan.framesData[i].framebuffer) != VK_SUCCESS)
        {
            printf("[Engine] Failed to create a framebuffer!\n");
            return false;
        }
    }
    return true;
}

bool InitCommandBuffers(void)
{
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = context.vulkan.graphicsQueueFamilyIndex;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(context.vulkan.logicalDevice, &commandPoolInfo, NULL, &context.vulkan.commandPool) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create command pool!\n");
        return false;
    }

    VkCommandBufferAllocateInfo commandBufferInfo = {};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.commandPool = context.vulkan.commandPool;
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferInfo.commandBufferCount = context.vulkan.maxFramesInFlight;

    VkCommandBuffer *commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * context.vulkan.maxFramesInFlight);
    if (vkAllocateCommandBuffers(context.vulkan.logicalDevice, &commandBufferInfo, commandBuffers) != VK_SUCCESS)
    {
        free(commandBuffers);
        printf("[Engine] Failed to create command buffers!\n");
        return false;
    }

    for (size_t i = 0; i < context.vulkan.maxFramesInFlight; ++i)
    {
        context.vulkan.framesData[i].commandBuffer = commandBuffers[i];
    }

    free(commandBuffers);

    return true;
}

bool InitDescriptors(void)
{
    VkDescriptorSetLayoutBinding texBinding = { 0 };
    texBinding.binding = 0;
    texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texBinding.descriptorCount = 1;
    texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo = { 0 };
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &texBinding;
    if (vkCreateDescriptorSetLayout(context.vulkan.logicalDevice, &layoutInfo, NULL, &context.vulkan.descriptorSetLayout) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create descriptor set layout!\n");
        return false;
    }

    VkDescriptorPoolSize poolSizes[1];
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = MAX_NUM_TEXTURE_DESCRIPTORS;
    VkDescriptorPoolCreateInfo descriptorPoolInfo = { 0 };
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.maxSets = MAX_NUM_TEXTURE_DESCRIPTORS;
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(context.vulkan.logicalDevice, &descriptorPoolInfo, NULL, &context.vulkan.descriptorPool) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create descriptor pool!\n");
        return false;
    }

    VkDescriptorSetLayout layouts[MAX_NUM_TEXTURE_DESCRIPTORS];
    for (uint32_t i = 0; i < MAX_NUM_TEXTURE_DESCRIPTORS; ++i)
    {
        layouts[i] = context.vulkan.descriptorSetLayout;
    }
    VkDescriptorSetAllocateInfo allocInfo = { 0 };
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = context.vulkan.descriptorPool;
    allocInfo.descriptorSetCount = MAX_NUM_TEXTURE_DESCRIPTORS;
    allocInfo.pSetLayouts = layouts;
    if (vkAllocateDescriptorSets(context.vulkan.logicalDevice, &allocInfo, context.vulkan.textureDescriptorSets) != VK_SUCCESS)
    {
        printf("[Engine] Failed to allocate descriptor sets!\n");
        return false;
    }

    return true;
}

bool InitGraphicsPipeline(void)
{
    VkVertexInputBindingDescription vertexInputBindingDescription = { 0 };
    vertexInputBindingDescription.binding = 0;
    vertexInputBindingDescription.stride = sizeof(Vertex);
    vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // Position
    VkVertexInputAttributeDescription positionAttributeDescription = { 0 };
    positionAttributeDescription.binding = 0;
    positionAttributeDescription.location = 0;
    positionAttributeDescription.offset = offsetof(Vertex, x);
    positionAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;

    // Color
    VkVertexInputAttributeDescription colorAttributeDescription = { 0 };
    colorAttributeDescription.binding = 0;
    colorAttributeDescription.location = 1;
    colorAttributeDescription.offset = offsetof(Vertex, r);
    colorAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;

    // UV
    VkVertexInputAttributeDescription uvAttributeDescription = { 0 };
    uvAttributeDescription.binding = 0;
    uvAttributeDescription.location = 2;
    uvAttributeDescription.offset = offsetof(Vertex, u);
    uvAttributeDescription.format = VK_FORMAT_R32G32_SFLOAT;

    VkVertexInputAttributeDescription vertexInputAttributes[3] = { positionAttributeDescription, colorAttributeDescription, uvAttributeDescription };
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = { 0 };
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
    vertexInputCreateInfo.vertexAttributeDescriptionCount = 3;
    vertexInputCreateInfo.pVertexAttributeDescriptions = vertexInputAttributes;
    vertexInputCreateInfo.flags = 0;
    vertexInputCreateInfo.pNext = NULL;

    // --- Input assembly ---
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = { 0 };
    inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;
    inputAssemblyCreateInfo.flags = 0;
    inputAssemblyCreateInfo.pNext = NULL;

    // --- Viewports & scisscors ---
    VkPipelineViewportStateCreateInfo viewportCreateInfo = { 0 };
    viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportCreateInfo.viewportCount = 1;
    viewportCreateInfo.pViewports = NULL;
    viewportCreateInfo.scissorCount = 1;
    viewportCreateInfo.pScissors = NULL;
    viewportCreateInfo.flags = 0;
    viewportCreateInfo.pNext = NULL;

    // --- Rasterization ---
    VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo = { 0 };
    rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationCreateInfo.lineWidth = 1.0f;
    rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationCreateInfo.depthBiasConstantFactor = 0.0f; // Optional
    rasterizationCreateInfo.depthBiasClamp = 0.0f; // Optional
    rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f; // Optional

    // --- Multisampling ---
    VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = { 0 };
    multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleCreateInfo.minSampleShading = 1.0f; // Optional
    multisampleCreateInfo.pSampleMask = NULL; // Optional
    multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampleCreateInfo.alphaToOneEnable = VK_FALSE; // Optional

    // --- Depth & stencil ---
    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = { 0 };
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilCreateInfo.minDepthBounds = 0.0f;
    depthStencilCreateInfo.maxDepthBounds = 1.0f;
    depthStencilCreateInfo.stencilTestEnable = VK_FALSE; 
    //depthStencilCreateInfo.front = { 0 };
    //depthStencilCreateInfo.back = { 0 };

    // --- Color blending ---
    VkPipelineColorBlendAttachmentState colorBlendAttachment = { 0 };
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = { 0 };
    colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlendCreateInfo.attachmentCount = 1;
    colorBlendCreateInfo.pAttachments = &colorBlendAttachment;
    colorBlendCreateInfo.blendConstants[0] = 0.0f; // Optional
    colorBlendCreateInfo.blendConstants[1] = 0.0f; // Optional
    colorBlendCreateInfo.blendConstants[2] = 0.0f; // Optional
    colorBlendCreateInfo.blendConstants[3] = 0.0f; // Optional

    // --- Dynamic state ---
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = { 0 };
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = 2;
    VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;

    // --- Pipeline layout ---
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = { 0 };
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &context.vulkan.descriptorSetLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutCreateInfo.pPushConstantRanges = NULL; // Optional

    // --- Shaders ---
    VkPipelineShaderStageCreateInfo vertexShaderCreateInfo = { 0 };
    vertexShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderCreateInfo.module = VK_NULL_HANDLE;
    vertexShaderCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderCreateInfo = { 0 };
    fragmentShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderCreateInfo.module = VK_NULL_HANDLE;
    fragmentShaderCreateInfo.pName = "main";

    // Create shader modules for the vertex and fragment shaders
    VkShaderModule vertexShaderModule;
    if (!CreateShaderModule("resources/shaders/main_vs.spv", context.vulkan.logicalDevice, &vertexShaderModule))
    {
        return false;
    }

    VkShaderModule fragmentShaderModule;
    if (!CreateShaderModule("resources/shaders/main_fs.spv", context.vulkan.logicalDevice, &fragmentShaderModule))
    {
        vkDestroyShaderModule(context.vulkan.logicalDevice, vertexShaderModule, NULL);
        return false;
    }

    vertexShaderCreateInfo.module = vertexShaderModule;
    fragmentShaderCreateInfo.module = fragmentShaderModule;

    VkPipelineShaderStageCreateInfo shaderStages[2] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };
    if (vkCreatePipelineLayout(context.vulkan.logicalDevice, &pipelineLayoutCreateInfo, NULL, &context.vulkan.graphicsPipelineLayout) != VK_SUCCESS)
    {
        vkDestroyShaderModule(context.vulkan.logicalDevice, vertexShaderModule, NULL);
        vkDestroyShaderModule(context.vulkan.logicalDevice, fragmentShaderModule, NULL);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
    pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    pipelineCreateInfo.layout = context.vulkan.graphicsPipelineLayout;
    pipelineCreateInfo.renderPass = context.vulkan.renderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // In case we inherit from an old pipeline
    pipelineCreateInfo.basePipelineIndex = -1; // Optional

    if (vkCreateGraphicsPipelines(context.vulkan.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &context.vulkan.graphicsPipeline) != VK_SUCCESS)
    {
        vkDestroyShaderModule(context.vulkan.logicalDevice, vertexShaderModule, NULL);
        vkDestroyShaderModule(context.vulkan.logicalDevice, fragmentShaderModule, NULL);
        vkDestroyPipelineLayout(context.vulkan.logicalDevice, context.vulkan.graphicsPipelineLayout, NULL);
        return false;
    }

    vkDestroyShaderModule(context.vulkan.logicalDevice, vertexShaderModule, NULL);
    vkDestroyShaderModule(context.vulkan.logicalDevice, fragmentShaderModule, NULL);

    return true;
}

void ShutdownVulkan(void)
{
    if (context.vulkan.descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(context.vulkan.logicalDevice, context.vulkan.descriptorPool, NULL);
        context.vulkan.descriptorPool = VK_NULL_HANDLE;
    }
    if (context.vulkan.descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(context.vulkan.logicalDevice, context.vulkan.descriptorSetLayout, NULL);
        context.vulkan.descriptorSetLayout = VK_NULL_HANDLE;
    }
    // TODO: Move
    if (context.vulkan.sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(context.vulkan.logicalDevice, context.vulkan.sampler, NULL);
        context.vulkan.sampler = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < context.vulkan.maxFramesInFlight; ++i)
    {
        vkDestroySemaphore(context.vulkan.logicalDevice, context.vulkan.framesData[i].imageAvailableSemaphore, NULL);
        vkDestroySemaphore(context.vulkan.logicalDevice, context.vulkan.framesData[i].renderDoneSemaphore, NULL);
        vkDestroyFence(context.vulkan.logicalDevice, context.vulkan.framesData[i].renderDoneFence, NULL);
    }

    if (context.vulkan.graphicsPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(context.vulkan.logicalDevice, context.vulkan.graphicsPipelineLayout, NULL);
        context.vulkan.graphicsPipelineLayout = VK_NULL_HANDLE;
        vkDestroyPipeline(context.vulkan.logicalDevice, context.vulkan.graphicsPipeline, NULL);
        context.vulkan.graphicsPipeline = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < context.vulkan.maxFramesInFlight; ++i)
    {
        vkDestroyFramebuffer(context.vulkan.logicalDevice, context.vulkan.framesData[i].framebuffer, NULL);
    }

    if (context.vulkan.commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(context.vulkan.logicalDevice, context.vulkan.commandPool, NULL);
    }

    if (context.vulkan.depthBufferImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(context.vulkan.logicalDevice, context.vulkan.depthBufferImageView, NULL);
        context.vulkan.depthBufferImageView = VK_NULL_HANDLE;
        vkDestroyImage(context.vulkan.logicalDevice, context.vulkan.depthBufferImage, NULL);
        context.vulkan.depthBufferImage = VK_NULL_HANDLE;
        vkFreeMemory(context.vulkan.logicalDevice, context.vulkan.depthBufferImageMemory, NULL);
        context.vulkan.depthBufferImageMemory = VK_NULL_HANDLE;
    }

    if (context.vulkan.renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(context.vulkan.logicalDevice, context.vulkan.renderPass, NULL);
        context.vulkan.renderPass = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < context.vulkan.maxFramesInFlight; ++i)
    {
        vkDestroyImageView(context.vulkan.logicalDevice, context.vulkan.framesData[i].imageView, NULL);
    }

    // Destroy swapchain
    if (context.vulkan.swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(context.vulkan.logicalDevice, context.vulkan.swapchain, NULL);
    }

    free(context.vulkan.framesData);
    context.vulkan.framesData = NULL;

    // Destroy logical device
    if (context.vulkan.logicalDevice != VK_NULL_HANDLE)
    {
        vkDestroyDevice(context.vulkan.logicalDevice, NULL);
        context.vulkan.logicalDevice = VK_NULL_HANDLE;
    }

    // Destroy surface
    if (context.vulkan.surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(context.vulkan.instance, context.vulkan.surface, NULL);
        context.vulkan.surface = VK_NULL_HANDLE;
    }

    // Destroy Vulkan instance
    if (context.vulkan.instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(context.vulkan.instance, NULL);
        context.vulkan.instance = VK_NULL_HANDLE;
    }
}

bool InitSynchronizationTools(void)
{
    // --- Create semaphores that we use for rendering ---
    for (uint32_t i = 0; i < context.vulkan.maxFramesInFlight; ++i)
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Set the flag to be already signalled at the start

        if ((vkCreateSemaphore(context.vulkan.logicalDevice, &semaphoreCreateInfo, NULL, &context.vulkan.framesData[i].imageAvailableSemaphore) != VK_SUCCESS) 
                || (vkCreateSemaphore(context.vulkan.logicalDevice, &semaphoreCreateInfo, NULL, &context.vulkan.framesData[i].renderDoneSemaphore) != VK_SUCCESS)
                || (vkCreateFence(context.vulkan.logicalDevice, &fenceCreateInfo, NULL, &context.vulkan.framesData[i].renderDoneFence) != VK_SUCCESS))
        {
            printf("[Engine] Failed to create synchronization tools!\n");
            return false;
        }
    }

    return true;
}

bool InitSampler(void)
{
    VkSamplerCreateInfo info = { 0 };
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.minFilter = VK_FILTER_LINEAR;
    info.magFilter = VK_FILTER_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(context.vulkan.physicalDevice, &physicalDeviceProperties);
    info.anisotropyEnable = VK_TRUE;
    info.maxAnisotropy = physicalDeviceProperties.limits.maxSamplerAnisotropy;

    info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    info.unnormalizedCoordinates = VK_FALSE;

    info.compareEnable = VK_FALSE;
    info.compareOp = VK_COMPARE_OP_ALWAYS;

    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.mipLodBias = 0.0f;
    info.minLod = 0.0f;
    info.maxLod = 0.0f;

    if (vkCreateSampler(context.vulkan.logicalDevice, &info, NULL, &context.vulkan.sampler) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create sampler!\n");
        return false;
    }

    return true;
}

VkPhysicalDevice GetSuitablePhysicalDevice(VkInstance instance, uint32_t numRequiredExtensions, const char **requiredExtensionNames)
{
    VkPhysicalDevice ret = VK_NULL_HANDLE;

    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(context.vulkan.instance, &physicalDeviceCount, NULL);
    if (physicalDeviceCount > 0)
    {
        VkPhysicalDevice *physicalDevices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
        vkEnumeratePhysicalDevices(context.vulkan.instance, &physicalDeviceCount, physicalDevices);
        for (uint32_t i = 0; i < physicalDeviceCount; ++i)
        {
            VkPhysicalDevice device = physicalDevices[i];

            // Query physical device properties
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);

            // For now, only consider discrete GPUs
            if (deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                continue;
            }

            // Query physical device features
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

            // Check for anisotropic filtering capability (most GPUs nowadays should support this)
            if (!deviceFeatures.samplerAnisotropy)
            {
                continue;
            }

            // Check if our required device extensions are supported
            if (!CheckDeviceExtensionSupport(device, numRequiredExtensions, requiredExtensionNames))
            {
                continue;
            }

            ret = device;
            break;
        }

        free(physicalDevices);
    }

    return ret;
}

bool CheckDeviceExtensionSupport(VkPhysicalDevice device, uint32_t numRequiredExtensions, const char **requiredExtensionNames)
{
    uint32_t numSupportedExtensions;
    vkEnumerateDeviceExtensionProperties(device, NULL, &numSupportedExtensions, NULL);
    if (numSupportedExtensions > 0)
    {
        VkExtensionProperties *supportedExtensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * numSupportedExtensions);
        vkEnumerateDeviceExtensionProperties(device, NULL, &numSupportedExtensions, supportedExtensions);
        for (uint32_t i = 0; i < numRequiredExtensions; ++i)
        {
            bool isFound = false;

            for (uint32_t j = 0; j < numSupportedExtensions; ++j)
            {
                if (strcmp(requiredExtensionNames[i], supportedExtensions[j].extensionName) == 0)
                {
                    isFound = true;
                    break;
                }
            }

            if (!isFound)
            {
                return false;
            }
        }
        free(supportedExtensions);
    }

    return true;
}

bool GetQueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface, uint32_t *graphicsQueueFamilyIndex, uint32_t *presentQueueFamilyIndex)
{
    uint32_t numQueueFamilies = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, NULL);

    VkQueueFamilyProperties *queueFamilyProperties = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * numQueueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &numQueueFamilies, queueFamilyProperties);

    bool graphicsQueueFound = false, presentQueueFound = false;
    for (uint32_t i = 0; i < numQueueFamilies; ++i)
    {
        // Check if the queue family supports graphics capabilities
        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            graphicsQueueFound = true;
            *graphicsQueueFamilyIndex = i;
        }

        // Check if the queue family supports presentation capabilities
        VkBool32 presentationSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);
        if (presentationSupport)
        {
            presentQueueFound = true;
            *presentQueueFamilyIndex = i;
        }

        if (graphicsQueueFound && presentQueueFound)
        {
            return true;
        }
    }

    return false;
}

VulkanImage CreateImage(const uint32_t width, const uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryProperties)
{
    VulkanImage ret = { .image = VK_NULL_HANDLE, .memory = VK_NULL_HANDLE };

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usageFlags;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(context.vulkan.logicalDevice, &imageInfo, NULL, &image) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create image!\n");
        return ret;
    }

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(context.vulkan.logicalDevice, image, &memoryRequirements);

    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(context.vulkan.physicalDevice, &deviceMemoryProperties);

    uint32_t memoryTypeBits = memoryRequirements.memoryTypeBits;
    uint32_t memoryTypeIndex;
    bool memoryTypeIndexFound = false;
    // Go through each bit in the memoryTypeBits and check if the bit is set
    // and the memory type at that particular index supports the required properties.
    // For example, we check if bit 5 in the memoryTypeBits is set (meaning it is supported) and
    // if the memory type at index 5 supports all the flags in the requiredProperties flag
    for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; ++i)
    {
        if ((memoryTypeBits & (1 << i)) 
                && ((deviceMemoryProperties.memoryTypes[i].propertyFlags & memoryProperties) == memoryProperties))
        {
            memoryTypeIndex = i;
            memoryTypeIndexFound = true;
            break;
        }
    }

    if (!memoryTypeIndexFound)
    {
        vkDestroyImage(context.vulkan.logicalDevice, image, NULL);
        return ret;
    }

    VkDeviceMemory memory;
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(context.vulkan.logicalDevice, &allocInfo, NULL, &memory) != VK_SUCCESS)
    {
        printf("[Engine] Failed to allocate memory for the image!\n");
        vkDestroyImage(context.vulkan.logicalDevice, image, NULL);
        return ret;
    }

    vkBindImageMemory(context.vulkan.logicalDevice, image, memory, 0);

    ret.image = image;
    ret.memory = memory;
    return ret;
}

bool CreateShaderModule(const char *shaderFilePath, VkDevice device, VkShaderModule *outShaderModule)
{
    FILE *file = fopen(shaderFilePath, "rb");
    if (!file)
    {
        printf("[Engine] Cannot open shader file %s\n", shaderFilePath);
        return false;
    }

    uint32_t fileSize;
    fseek(file, 0L, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0L, SEEK_SET);

    char *shaderData = (char*)malloc(sizeof(char) * fileSize);
    fread(shaderData, sizeof(char), fileSize, file);
    fclose(file);


    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = (uint32_t*)shaderData;

    if (vkCreateShaderModule(device, &createInfo, NULL, outShaderModule) != VK_SUCCESS)
    {
        free(shaderData);
        return false;
    }

    free(shaderData);
    return true;

}
