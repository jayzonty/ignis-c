#include "ignis_graphics.h"

#include "dynamic_array.h"
#include "ignis/ignis_math.h"
#include "ignis_vulkan.h"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

#include <math.h>

#define MAX_NUM_TEXTURE_DESCRIPTORS 256
#define MAX_VERTICES_PER_BATCH 10000

typedef struct
{
    int width;
    int height;
    int numChannels;

    VkImage image;
    VkDeviceMemory memory;
    VkImageView imageView;
} TextureData;

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

// Vertex struct
typedef struct 
{
    float x, y, z;
    float r, g, b;
    float u, v;
} Vertex;

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

    DynamicArray renderBatchUnits;
} RenderBatch;

static VulkanContext vulkanContext = { 0 };

static RenderBatch defaultRenderBatch = { 0 };

static uint32_t nextTextureId = 0;

static TextureData textureDataList[MAX_NUM_TEXTURE_DESCRIPTORS] = { 0 };

static int _windowWidth = 0;
static int _windowHeight = 0;
static GLFWwindow* _windowHandle = NULL;

// ----------------------------------------
// Vulkan Function Declarations
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

bool Ignis_Graphics_Init(int windowWidth, int windowHeight, GLFWwindow *windowHandle)
{
    _windowWidth = windowWidth;
    _windowHeight = windowHeight;
    _windowHandle = windowHandle;

    if (!InitVulkan())
    {
        Ignis_Graphics_Cleanup();
        return false;
    }

    // Set up default render batch
    {
        defaultRenderBatch.numActiveVertices = 0;
        defaultRenderBatch.vertices = (Vertex*)malloc(sizeof(Vertex) * MAX_VERTICES_PER_BATCH);
        DynamicArrayInit(&defaultRenderBatch.renderBatchUnits, sizeof(RenderBatchUnit));

        bool result = CreateBuffer
        (
            vulkanContext.physicalDevice,
            vulkanContext.logicalDevice,
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

    return true;
}

void Ignis_Graphics_WaitDeviceIdle(void)
{
    vkDeviceWaitIdle(vulkanContext.logicalDevice);
}

void Ignis_Graphics_Cleanup(void)
{
    vkDeviceWaitIdle(vulkanContext.logicalDevice);

    DynamicArrayFree(&defaultRenderBatch.renderBatchUnits);

    if (defaultRenderBatch.vertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(vulkanContext.logicalDevice, defaultRenderBatch.vertexBuffer, NULL);
        defaultRenderBatch.vertexBuffer = VK_NULL_HANDLE;
        vkFreeMemory(vulkanContext.logicalDevice, defaultRenderBatch.vertexBufferMemory, NULL);
        defaultRenderBatch.vertexBufferMemory = VK_NULL_HANDLE;
    }

    free(defaultRenderBatch.vertices);
    defaultRenderBatch.vertices = NULL;

    ShutdownVulkan();
}

void Ignis_Graphics_BeginDrawing()
{
    defaultRenderBatch.numActiveVertices = 0;
    DynamicArrayClear(&defaultRenderBatch.renderBatchUnits);
}

int RenderBatchUnitCompFunc(const void *a, const void *b)
{
    RenderBatchUnit *first = (RenderBatchUnit*)a;
    RenderBatchUnit *second = (RenderBatchUnit*)b;
    return first->textureId - second->textureId;
}

void Ignis_Graphics_EndDrawing()
{
    // Transform vertices to NDC
    // TODO: Transform via a projection matrix instead
    for (size_t i = 0; i < defaultRenderBatch.numActiveVertices; ++i)
    {
        defaultRenderBatch.vertices[i].x = (defaultRenderBatch.vertices[i].x / _windowWidth) * 2.0f - 1.0f;
        defaultRenderBatch.vertices[i].y = (defaultRenderBatch.vertices[i].y / _windowHeight) * 2.0f - 1.0f;
    }

    qsort
    (
        DynamicArrayGetData(&defaultRenderBatch.renderBatchUnits),
        DynamicArrayGetSize(&defaultRenderBatch.renderBatchUnits),
        sizeof(RenderBatchUnit),
        RenderBatchUnitCompFunc
    );

    VkDeviceSize bufferSize = sizeof(Vertex) * defaultRenderBatch.numActiveVertices;

    void *mem;
    vkMapMemory(vulkanContext.logicalDevice, defaultRenderBatch.vertexBufferMemory, 0, bufferSize, 0, &mem);

    size_t offset = 0;
    size_t numRenderBatchUnits = DynamicArrayGetSize(&defaultRenderBatch.renderBatchUnits);
    for (size_t i = 0; i < numRenderBatchUnits; ++i)
    {
        RenderBatchUnit *unit = (RenderBatchUnit*)DynamicArrayGetElementAt(&defaultRenderBatch.renderBatchUnits, i);
        memcpy(mem + offset * sizeof(Vertex), defaultRenderBatch.vertices + unit->firstVertexOffset, sizeof(Vertex) * unit->numVertices);
        offset += unit->numVertices;
    }

    vkUnmapMemory(vulkanContext.logicalDevice, defaultRenderBatch.vertexBufferMemory);
}

void Ignis_Graphics_Draw(void)
{
    // Wait for the current frame to be done rendering
    vkWaitForFences
    (
        vulkanContext.logicalDevice,
        1,
        &vulkanContext.framesData[vulkanContext.currentFrameIndex].renderDoneFence,
        VK_TRUE,
        UINT64_MAX
    );
    vkResetFences
    (
        vulkanContext.logicalDevice,
        1, 
        &vulkanContext.framesData[vulkanContext.currentFrameIndex].renderDoneFence
    );

    uint32_t nextImageIndex;
    VkResult acquireImageResult = vkAcquireNextImageKHR
    (
        vulkanContext.logicalDevice, 
        vulkanContext.swapchain, 
        UINT64_MAX, 
        vulkanContext.framesData[vulkanContext.currentFrameIndex].imageAvailableSemaphore, 
        VK_NULL_HANDLE, 
        &nextImageIndex
    );
    if ((acquireImageResult != VK_SUCCESS) && (acquireImageResult != VK_SUBOPTIMAL_KHR))
    {
        printf("[Engine] Failed to acquire next image!\n");
        glfwSetWindowShouldClose(_windowHandle, GLFW_TRUE);
        return;
    }

    // --- Build render commands ---
        
    // Start command buffer recording
    VkCommandBuffer commandBuffer = vulkanContext.framesData[vulkanContext.currentFrameIndex].commandBuffer;
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo commandBufferBeginInfo = { 0 };
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
    {
        printf("[Engine] Failed to begin recording command buffer!\n");
        glfwSetWindowShouldClose(_windowHandle, GLFW_TRUE);
        return;
    }

    // Begin render pass
    VkRenderPassBeginInfo renderPassBeginInfo = { 0 };
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = vulkanContext.renderPass;
    renderPassBeginInfo.framebuffer = vulkanContext.framesData[nextImageIndex].framebuffer;
    VkRect2D renderArea = { .offset = { .x = 0, .y = 0 }, .extent = vulkanContext.swapchainImageExtent };
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
    float viewportHeight = vulkanContext.swapchainImageExtent.height;
    VkViewport viewport = { 0 };
    viewport.x = 0.0f;
    viewport.y = viewportHeight;
    viewport.width = vulkanContext.swapchainImageExtent.width;
    viewport.height = -viewportHeight; // Supported from Vulkan 1.1 to switch from Y=down convention
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = { 0 };
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = vulkanContext.swapchainImageExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanContext.graphicsPipeline);

    VkDeviceSize offset = 0;
    VkBuffer vertexBuffers[1] = { defaultRenderBatch.vertexBuffer };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, &offset);

    uint32_t vertexOffset = 0;
    uint32_t numVertices = 0;
    uint32_t prevTex = 0;
    size_t numRenderBatchUnits = DynamicArrayGetSize(&defaultRenderBatch.renderBatchUnits);
    for (size_t i = 0; i < numRenderBatchUnits; ++i)
    {
        RenderBatchUnit *unit = (RenderBatchUnit*)DynamicArrayGetElementAt(&defaultRenderBatch.renderBatchUnits, i);
        if (i == 0)
        {
            prevTex = unit->textureId;
        }

        if (unit->textureId != prevTex)
        {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanContext.graphicsPipelineLayout, 0, 1, &vulkanContext.textureDescriptorSets[prevTex], 0, NULL);
            vkCmdDraw(commandBuffer, numVertices, 1, vertexOffset, 0);

            vertexOffset += numVertices;
            numVertices = 0;
        }

        numVertices += unit->numVertices;
        prevTex = unit->textureId;
    }

    if (numVertices > 0)
    {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanContext.graphicsPipelineLayout, 0, 1, &vulkanContext.textureDescriptorSets[prevTex], 0, NULL);
        vkCmdDraw(commandBuffer, numVertices, 1, vertexOffset, 0);
    }

    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        printf("[Engine] Failed to end recording of command buffer!\n");
        glfwSetWindowShouldClose(_windowHandle, GLFW_TRUE);
        return;
    }

    // --- Submit ---
    VkSubmitInfo submitInfo = { 0 };
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[1] = { vulkanContext.framesData[vulkanContext.currentFrameIndex].imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[1] = { vulkanContext.framesData[vulkanContext.currentFrameIndex].renderDoneSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult submitResult = vkQueueSubmit
    (
        vulkanContext.graphicsQueue,
        1,
        &submitInfo,
        vulkanContext.framesData[vulkanContext.currentFrameIndex].renderDoneFence
    );
    if (submitResult != VK_SUCCESS)
    {
        printf("[Engine] Failed to submit!\n");
        glfwSetWindowShouldClose(_windowHandle, GLFW_TRUE);
        return;
    }

    // --- Present ---
    VkPresentInfoKHR presentInfo = { 0 };
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[1] = { vulkanContext.swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &nextImageIndex;

    vulkanContext.currentFrameIndex = (vulkanContext.currentFrameIndex + 1) % vulkanContext.maxFramesInFlight;

    VkResult presentResult = vkQueuePresentKHR
    (
        vulkanContext.presentQueue,
        &presentInfo
    );
    if (presentResult != VK_SUCCESS)
    {
        printf("[Engine] Failed to present!\n");
        glfwSetWindowShouldClose(_windowHandle, GLFW_TRUE);
        return;
    }
}

void Ignis_Graphics_DrawCircle(Vector2 position, float radius, Vector2 origin, Color color)
{
    RenderBatch *renderBatch = &defaultRenderBatch;

    const uint32_t NUM_SEGMENTS = 90;
    const uint32_t CIRCLE_VERTEX_COUNT = NUM_SEGMENTS * 3;

    if ((renderBatch->vertices == NULL) || (renderBatch->vertexBuffer == VK_NULL_HANDLE))
    {
        printf("[Engine] Default render batch's vertex buffer is not initialized!\n");
        return;
    }

    if (renderBatch->numActiveVertices + CIRCLE_VERTEX_COUNT > MAX_VERTICES_PER_BATCH)
    {
        printf("[Engine] Not enough vertices in the default render batch to draw a new circle!\n");
        return;
    }

    float segmentAngle = (M_PI * 2.0f) / NUM_SEGMENTS;
    uint32_t index = renderBatch->numActiveVertices;
    for (uint32_t i = 0; i < NUM_SEGMENTS; ++i)
    {
        // Circle center
        float x = position.x - radius * (origin.x - 0.5f) * 2.0f;
        float y = position.y - radius * (origin.y - 0.5f) * 2.0f;
        renderBatch->vertices[index].x = x;
        renderBatch->vertices[index].y = y;
        renderBatch->vertices[index].z = 0.0f;
        renderBatch->vertices[index].r = color.r / 255.0f;
        renderBatch->vertices[index].g = color.g / 255.0f;
        renderBatch->vertices[index].b = color.b / 255.0f;
        renderBatch->vertices[index].u = 0.0f;
        renderBatch->vertices[index].v = 0.0f;
        ++index;

        // Segment vertex 1
        x = position.x + radius * cosf(i * segmentAngle) - radius * (origin.x - 0.5f) * 2.0f;
        y = position.y + radius * sinf(i * segmentAngle) - radius * (origin.y - 0.5f) * 2.0f;
        renderBatch->vertices[index].x = x;
        renderBatch->vertices[index].y = y;
        renderBatch->vertices[index].z = 0.0f;
        renderBatch->vertices[index].r = color.r / 255.0f;
        renderBatch->vertices[index].g = color.g / 255.0f;
        renderBatch->vertices[index].b = color.b / 255.0f;
        renderBatch->vertices[index].u = 1.0f;
        renderBatch->vertices[index].v = 1.0f;
        ++index;

        // Segment vertex 2
        x = position.x + radius * cosf((i + 1) * segmentAngle) - radius * (origin.x - 0.5f) * 2.0f;
        y = position.y + radius * sinf((i + 1) * segmentAngle) - radius * (origin.y - 0.5f) * 2.0f;
        renderBatch->vertices[index].x = x;
        renderBatch->vertices[index].y = y;
        renderBatch->vertices[index].z = 0.0f;
        renderBatch->vertices[index].r = color.r / 255.0f;
        renderBatch->vertices[index].g = color.g / 255.0f;
        renderBatch->vertices[index].b = color.b / 255.0f;
        renderBatch->vertices[index].u = 0.0f;
        renderBatch->vertices[index].v = 1.0f;
        ++index;
    }

    RenderBatchUnit unit = { 0 };
    unit.firstVertexOffset = renderBatch->numActiveVertices;
    unit.numVertices = CIRCLE_VERTEX_COUNT;
    // TODO: Texture ID setting
    unit.textureId = 0;
    DynamicArrayAdd(&renderBatch->renderBatchUnits, &unit);

    renderBatch->numActiveVertices += CIRCLE_VERTEX_COUNT;
}

void Ignis_Graphics_DrawLine(Vector2 start, Vector2 end, float width, Color color)
{
    RenderBatch *renderBatch = &defaultRenderBatch;

    if ((renderBatch->vertices == NULL) || (renderBatch->vertexBuffer == VK_NULL_HANDLE))
    {
        printf("[Engine] Default render batch's vertex buffer is not initialized!\n");
        return;
    }

    if (renderBatch->numActiveVertices + 6 > MAX_VERTICES_PER_BATCH)
    {
        printf("[Engine] Not enough vertices in the default render batch to draw a new circle!\n");
        return;
    }

    Vector2 d;
    d.x = end.x - start.x;
    d.y = end.y - start.y;

    Vector2 normal;
    normal.x = -d.y;
    normal.y =  d.x;
    normal = Vector2Normalize(normal);

    float halfWidth = width / 2.0f;
    float normalMult[6] = { -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f };
    float dMult[6] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f };

    float u[6] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f };
    float v[6] = { 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };

    uint32_t index = renderBatch->numActiveVertices;
    for (uint32_t i = 0; i < 6; ++i)
    {
        renderBatch->vertices[index + i].x = start.x + d.x * dMult[i] + normal.x * normalMult[i] * width;
        renderBatch->vertices[index + i].y = start.y + d.y * dMult[i] + normal.y * normalMult[i] * width;
        renderBatch->vertices[index + i].z = 0.0f;

        renderBatch->vertices[index + i].r = color.r / 255.0f;
        renderBatch->vertices[index + i].g = color.g / 255.0f;
        renderBatch->vertices[index + i].b = color.b / 255.0f;

        renderBatch->vertices[index + i].u = u[i];
        renderBatch->vertices[index + i].v = v[i];
    }

    RenderBatchUnit unit = { 0 };
    unit.firstVertexOffset = renderBatch->numActiveVertices;
    unit.numVertices = 6;
    // TODO: Texture id setting
    unit.textureId = 0;
    DynamicArrayAdd(&renderBatch->renderBatchUnits, &unit);

    renderBatch->numActiveVertices += 6;
}

void Ignis_Graphics_DrawTexture(Vector2 position, Vector2 size, Vector2 origin, Texture texture, Rect uvRect, Color tint)
{
    RenderBatch *renderBatch = &defaultRenderBatch;
    if (renderBatch == NULL)
    {
        return;
    }

    if ((renderBatch->vertices == NULL) || (renderBatch->vertexBuffer == VK_NULL_HANDLE))
    {
        printf("[Engine] Default render batch's vertex buffer is not initialized!\n");
        return;
    }

    if (renderBatch->numActiveVertices + 6 > MAX_VERTICES_PER_BATCH)
    {
        printf("[Engine] Not enough vertices in the default render batch to draw a new rectangle!\n");
        return;
    }

    float dx[6] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f };
    float dy[6] = { 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f };
    for (uint32_t i = 0; i < 6; ++i)
    {
        uint32_t index = renderBatch->numActiveVertices + i;

        Vector3 vertexPosition = { 0 };
        vertexPosition.x = position.x + size.x * (dx[i] - origin.x);
        vertexPosition.y = position.y + size.y * (dy[i] - origin.y);
        vertexPosition.z = 0.0f;

        renderBatch->vertices[index].x = vertexPosition.x;
        renderBatch->vertices[index].y = vertexPosition.y;
        renderBatch->vertices[index].z = vertexPosition.z;
        renderBatch->vertices[index].r = tint.r / 255.0f;
        renderBatch->vertices[index].g = tint.g / 255.0f;
        renderBatch->vertices[index].b = tint.b / 255.0f;
        renderBatch->vertices[index].u = uvRect.x + uvRect.width * dx[i];
        renderBatch->vertices[index].v = uvRect.y + uvRect.height * dy[i];
    }

    RenderBatchUnit unit = { 0 };
    unit.firstVertexOffset = renderBatch->numActiveVertices;
    unit.numVertices = 6;
    unit.textureId = texture.id;
    DynamicArrayAdd(&renderBatch->renderBatchUnits, &unit);

    renderBatch->numActiveVertices += 6;
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

int Ignis_Graphics_LoadTextureFromData(const uint8_t *pixels, int width, int height, int numChannels)
{
    if (nextTextureId >= MAX_NUM_TEXTURE_DESCRIPTORS)
    {
        return -1;
    }

    VkDeviceSize imageSize = width * height * numChannels;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    CreateBuffer
    (
        vulkanContext.physicalDevice,
        vulkanContext.logicalDevice,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer,
        &stagingBufferMemory
    );

    void *data;
    vkMapMemory(vulkanContext.logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(vulkanContext.logicalDevice, stagingBufferMemory);

    // TODO: Image created below is forced to be RGBA, so need to handle texture data with less channels
    
    VkImage image;
    VkDeviceMemory memory;
    bool result = CreateImage2D
    (
        vulkanContext.physicalDevice,
        vulkanContext.logicalDevice,
        width, 
        height, 
        VK_FORMAT_R8G8B8A8_SRGB, 
        VK_IMAGE_TILING_OPTIMAL, 
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &image,
        &memory
    );

    int ret = -1;
    if (result)
    {
        // Create a one-time submit command buffer for the staging buffer transfer and for image layout transistion
        VkCommandBuffer commandBuffer = CreateCommandBuffer(vulkanContext.logicalDevice, vulkanContext.commandPool);
        BeginOneTimeCommands(commandBuffer);
        AppendTransitionImageLayoutCommand(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);
        AppendCopyBufferToImageCommand(stagingBuffer, image, width, height, commandBuffer);
        AppendTransitionImageLayoutCommand(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        SubmitCommandBuffer(commandBuffer, vulkanContext.graphicsQueue);
        FreeCommandBuffer(vulkanContext.logicalDevice, vulkanContext.commandPool, commandBuffer);

        ret = nextTextureId++;

        textureDataList[ret].width = width;
        textureDataList[ret].height = height;
        textureDataList[ret].numChannels = numChannels;

        textureDataList[ret].image = image;
        textureDataList[ret].memory = memory;
        
        CreateImageView(vulkanContext.logicalDevice, image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, &textureDataList[ret].imageView);

        // Update descriptor set
        VkDescriptorImageInfo info = { 0 };
        info.imageView = textureDataList[ret].imageView;
        info.sampler = vulkanContext.sampler;
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write = { 0 };
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = vulkanContext.textureDescriptorSets[ret];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &info;

        vkUpdateDescriptorSets(vulkanContext.logicalDevice, 1, &write, 0, NULL);
    }

    vkDestroyBuffer(vulkanContext.logicalDevice, stagingBuffer, NULL);
    vkFreeMemory(vulkanContext.logicalDevice, stagingBufferMemory, NULL);

    return ret;
}

void Ignis_Graphics_UnloadTexture(int textureId)
{
    if (textureId < 0)
    {
        return;
    }

    if (textureDataList[textureId].image != VK_NULL_HANDLE)
    {
        vkDestroyImageView(vulkanContext.logicalDevice, textureDataList[textureId].imageView, NULL);
        vkDestroyImage(vulkanContext.logicalDevice, textureDataList[textureId].image, NULL);
        vkFreeMemory(vulkanContext.logicalDevice, textureDataList[textureId].memory, NULL);
    }
}

bool InitVulkan(void)
{
    if (vulkanContext.instance != VK_NULL_HANDLE)
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

        VkResult result = vkCreateInstance(&instanceCreateInfo, NULL, &vulkanContext.instance);
        if (result != VK_SUCCESS)
        {
            printf("[Engine] Failed to create a Vulkan instance!\n");
            return false;
        }
    }

    { // Create surface
        VkResult result = glfwCreateWindowSurface(vulkanContext.instance, _windowHandle, NULL, &vulkanContext.surface);
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
        VkPhysicalDevice physicalDevice = GetSuitablePhysicalDevice(vulkanContext.instance, numRequiredDeviceExtensions, requiredDeviceExtensions);
        if (physicalDevice == VK_NULL_HANDLE)
        {
            printf("[Engine] Failed to find a suitable GPU!\n");
            return false;
        }

        vulkanContext.physicalDevice = physicalDevice;

        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
        printf("[Engine] Selected GPU: %s\n", physicalDeviceProperties.deviceName);
    }

    { // Create logical device and queues
        if (!GetQueueFamilyIndices(
                vulkanContext.physicalDevice,
                vulkanContext.surface,
                &vulkanContext.graphicsQueueFamilyIndex,
                &vulkanContext.presentQueueFamilyIndex))
        {
            printf("[Engine] Failed to find queue family indices for graphics and presentation");
            return false;
        }

        uint32_t queueFamilyIndices[2];
        queueFamilyIndices[0] = vulkanContext.graphicsQueueFamilyIndex;
        queueFamilyIndices[1] = vulkanContext.presentQueueFamilyIndex;

        uint32_t numQueuesToCreate = 2;
        if (vulkanContext.graphicsQueueFamilyIndex == vulkanContext.presentQueueFamilyIndex)
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
        if (vkCreateDevice(vulkanContext.physicalDevice, &logicalDeviceInfo, NULL, &vulkanContext.logicalDevice) != VK_SUCCESS)
        {
            free(queueCreateInfoStructs);
            printf("[Engine] Failed to create logical device!\n");
            return false;
        }

        free(queueCreateInfoStructs);

        // --- Get handles to the device queues that we just created ---
        vkGetDeviceQueue(vulkanContext.logicalDevice, vulkanContext.graphicsQueueFamilyIndex, 0, &vulkanContext.graphicsQueue);
        vkGetDeviceQueue(vulkanContext.logicalDevice, vulkanContext.presentQueueFamilyIndex, 0, &vulkanContext.presentQueue);
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
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanContext.physicalDevice, vulkanContext.surface, &surfaceCapabilities);

    // Query supported formats
    uint32_t numAvailableSurfaceFormats;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanContext.physicalDevice, vulkanContext.surface, &numAvailableSurfaceFormats, NULL);
    if (numAvailableSurfaceFormats == 0)
    {
        printf("[Engine] Selected physical device has no supported surface formats!\n");
        return false;
    }
    VkSurfaceFormatKHR *availableSurfaceFormats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * numAvailableSurfaceFormats);
    vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanContext.physicalDevice, vulkanContext.surface, &numAvailableSurfaceFormats, availableSurfaceFormats);

    // Query presentation modes
    uint32_t numAvailablePresentModes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanContext.physicalDevice, vulkanContext.surface, &numAvailablePresentModes, NULL);
    if (numAvailablePresentModes == 0)
    {
        free(availableSurfaceFormats);
        printf("[Engine] Selected physical device has no supported present modes!\n");
        return false;
    }
    VkPresentModeKHR *availablePresentModes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * numAvailablePresentModes);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanContext.physicalDevice, vulkanContext.surface, &numAvailablePresentModes, availablePresentModes);

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
        glfwGetFramebufferSize(_windowHandle, &width, &height);

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
    swapchainCreateInfo.surface = vulkanContext.surface;
    swapchainCreateInfo.minImageCount = numSwapchainImages;
    swapchainCreateInfo.imageFormat = selectedSurfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = selectedSurfaceFormat.colorSpace;
    swapchainCreateInfo.presentMode = selectedPresentMode;
    swapchainCreateInfo.clipped = VK_TRUE; // Clip pixels that are obscured, e.g., by other windows
    swapchainCreateInfo.imageExtent = swapchainImageExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Can use VK_IMAGE_USAGE_TRANSFER_DST_BIT for rendering to an offscreen buffer (e.g., post-processing)
    if (vulkanContext.graphicsQueueFamilyIndex != vulkanContext.presentQueueFamilyIndex)
    {
        // Graphics queue and present queue are different (in some GPUs, they can be the same).
        // In this case, we use VK_SHARING_MODE_CONCURRENT, which means swapchain images can be owned by
        // multiple queue famillies without the need for transfer of ownership.
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;

        // We also need to specify which queue families concurrently own a swapchain image 
        uint32_t indices[] = { vulkanContext.graphicsQueueFamilyIndex, vulkanContext.presentQueueFamilyIndex };
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
    if (vkCreateSwapchainKHR(vulkanContext.logicalDevice, &swapchainCreateInfo, NULL, &vulkanContext.swapchain) != VK_SUCCESS)
    {
        free(availableSurfaceFormats);
        free(availablePresentModes);
        printf("[Engine] Failed to create swapchain!\n");
        return false;
    }

    // Retrieve swapchain images, and store the image format and extent
    vkGetSwapchainImagesKHR(vulkanContext.logicalDevice, vulkanContext.swapchain, &numSwapchainImages, NULL);
    VkImage *swapchainImages = (VkImage*)malloc(sizeof(VkImage) * numSwapchainImages);
    vkGetSwapchainImagesKHR(vulkanContext.logicalDevice, vulkanContext.swapchain, &numSwapchainImages, swapchainImages);
    vulkanContext.swapchainImageFormat = selectedSurfaceFormat.format;
    vulkanContext.swapchainImageExtent = swapchainImageExtent;

    VkImageView *swapchainImageViews = (VkImageView*)malloc(sizeof(VkImage) * numSwapchainImages);
    vulkanContext.framesData = (FrameData*)malloc(sizeof(FrameData) * numSwapchainImages);
    for (size_t i = 0; i < numSwapchainImages; ++i)
    {
        vulkanContext.framesData[i].image = swapchainImages[i];
        CreateImageView(vulkanContext.logicalDevice, vulkanContext.framesData[i].image, vulkanContext.swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, &vulkanContext.framesData[i].imageView);
        if (vulkanContext.framesData[i].imageView == VK_NULL_HANDLE)
        {
            free(swapchainImages);
            free(availableSurfaceFormats);
            free(availablePresentModes);
            printf("[Engine] Failed to create swapchain image views!\n");
            return false;
        }
    }

    vulkanContext.maxFramesInFlight = numSwapchainImages;

    free(swapchainImages);
    free(availableSurfaceFormats);
    free(availablePresentModes);

    return true;
}

bool InitRenderPass(void)
{
    // Setup color attachment for the framebuffer
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = vulkanContext.swapchainImageFormat;
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

    if (vkCreateRenderPass(vulkanContext.logicalDevice, &renderPassCreateInfo, NULL, &vulkanContext.renderPass) != VK_SUCCESS)
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
        vulkanContext.physicalDevice,
        vulkanContext.logicalDevice,
        vulkanContext.swapchainImageExtent.width, 
        vulkanContext.swapchainImageExtent.height, 
        VK_FORMAT_D32_SFLOAT, 
        VK_IMAGE_TILING_OPTIMAL, 
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &vulkanContext.depthBufferImage,
        &vulkanContext.depthBufferImageMemory
    );
    if (!result)
    {
        printf("[Engine] Failed to create Vulkan image for the depth buffer!\n");
        return false;
    }

    CreateImageView(vulkanContext.logicalDevice, vulkanContext.depthBufferImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, &vulkanContext.depthBufferImageView);

    // TODO: Can transition image layout of the depth buffer image if we so choose,
    // but we'll already handle this in the render pass.

    return true;
}

bool InitFramebuffers(void)
{
    for (size_t i = 0; i < vulkanContext.maxFramesInFlight; ++i)
    {
        VkImageView attachments[2] = { 0 };
        attachments[0] = vulkanContext.framesData[i].imageView;
        attachments[1] = vulkanContext.depthBufferImageView;

        VkFramebufferCreateInfo framebufferCreateInfo = {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = vulkanContext.renderPass;
        framebufferCreateInfo.attachmentCount = 2;
        framebufferCreateInfo.pAttachments = attachments;
        framebufferCreateInfo.width = vulkanContext.swapchainImageExtent.width;
        framebufferCreateInfo.height = vulkanContext.swapchainImageExtent.height;
        framebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(vulkanContext.logicalDevice, &framebufferCreateInfo, NULL, &vulkanContext.framesData[i].framebuffer) != VK_SUCCESS)
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
    commandPoolInfo.queueFamilyIndex = vulkanContext.graphicsQueueFamilyIndex;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(vulkanContext.logicalDevice, &commandPoolInfo, NULL, &vulkanContext.commandPool) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create command pool!\n");
        return false;
    }

    VkCommandBufferAllocateInfo commandBufferInfo = {};
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferInfo.commandPool = vulkanContext.commandPool;
    commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferInfo.commandBufferCount = vulkanContext.maxFramesInFlight;

    VkCommandBuffer *commandBuffers = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer) * vulkanContext.maxFramesInFlight);
    if (vkAllocateCommandBuffers(vulkanContext.logicalDevice, &commandBufferInfo, commandBuffers) != VK_SUCCESS)
    {
        free(commandBuffers);
        printf("[Engine] Failed to create command buffers!\n");
        return false;
    }

    for (size_t i = 0; i < vulkanContext.maxFramesInFlight; ++i)
    {
        vulkanContext.framesData[i].commandBuffer = commandBuffers[i];
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
    if (vkCreateDescriptorSetLayout(vulkanContext.logicalDevice, &layoutInfo, NULL, &vulkanContext.descriptorSetLayout) != VK_SUCCESS)
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
    if (vkCreateDescriptorPool(vulkanContext.logicalDevice, &descriptorPoolInfo, NULL, &vulkanContext.descriptorPool) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create descriptor pool!\n");
        return false;
    }

    VkDescriptorSetLayout layouts[MAX_NUM_TEXTURE_DESCRIPTORS];
    for (uint32_t i = 0; i < MAX_NUM_TEXTURE_DESCRIPTORS; ++i)
    {
        layouts[i] = vulkanContext.descriptorSetLayout;
    }
    VkDescriptorSetAllocateInfo allocInfo = { 0 };
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = vulkanContext.descriptorPool;
    allocInfo.descriptorSetCount = MAX_NUM_TEXTURE_DESCRIPTORS;
    allocInfo.pSetLayouts = layouts;
    if (vkAllocateDescriptorSets(vulkanContext.logicalDevice, &allocInfo, vulkanContext.textureDescriptorSets) != VK_SUCCESS)
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
    pipelineLayoutCreateInfo.pSetLayouts = &vulkanContext.descriptorSetLayout;
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
    if (!CreateShaderModule("resources/shaders/main_vs.spv", vulkanContext.logicalDevice, &vertexShaderModule))
    {
        return false;
    }

    VkShaderModule fragmentShaderModule;
    if (!CreateShaderModule("resources/shaders/main_fs.spv", vulkanContext.logicalDevice, &fragmentShaderModule))
    {
        vkDestroyShaderModule(vulkanContext.logicalDevice, vertexShaderModule, NULL);
        return false;
    }

    vertexShaderCreateInfo.module = vertexShaderModule;
    fragmentShaderCreateInfo.module = fragmentShaderModule;

    VkPipelineShaderStageCreateInfo shaderStages[2] = { vertexShaderCreateInfo, fragmentShaderCreateInfo };
    if (vkCreatePipelineLayout(vulkanContext.logicalDevice, &pipelineLayoutCreateInfo, NULL, &vulkanContext.graphicsPipelineLayout) != VK_SUCCESS)
    {
        vkDestroyShaderModule(vulkanContext.logicalDevice, vertexShaderModule, NULL);
        vkDestroyShaderModule(vulkanContext.logicalDevice, fragmentShaderModule, NULL);
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
    pipelineCreateInfo.layout = vulkanContext.graphicsPipelineLayout;
    pipelineCreateInfo.renderPass = vulkanContext.renderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // In case we inherit from an old pipeline
    pipelineCreateInfo.basePipelineIndex = -1; // Optional

    if (vkCreateGraphicsPipelines(vulkanContext.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &vulkanContext.graphicsPipeline) != VK_SUCCESS)
    {
        vkDestroyShaderModule(vulkanContext.logicalDevice, vertexShaderModule, NULL);
        vkDestroyShaderModule(vulkanContext.logicalDevice, fragmentShaderModule, NULL);
        vkDestroyPipelineLayout(vulkanContext.logicalDevice, vulkanContext.graphicsPipelineLayout, NULL);
        return false;
    }

    vkDestroyShaderModule(vulkanContext.logicalDevice, vertexShaderModule, NULL);
    vkDestroyShaderModule(vulkanContext.logicalDevice, fragmentShaderModule, NULL);

    return true;
}

void ShutdownVulkan(void)
{
    if (vulkanContext.descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(vulkanContext.logicalDevice, vulkanContext.descriptorPool, NULL);
        vulkanContext.descriptorPool = VK_NULL_HANDLE;
    }
    if (vulkanContext.descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(vulkanContext.logicalDevice, vulkanContext.descriptorSetLayout, NULL);
        vulkanContext.descriptorSetLayout = VK_NULL_HANDLE;
    }
    // TODO: Move
    if (vulkanContext.sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(vulkanContext.logicalDevice, vulkanContext.sampler, NULL);
        vulkanContext.sampler = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < vulkanContext.maxFramesInFlight; ++i)
    {
        vkDestroySemaphore(vulkanContext.logicalDevice, vulkanContext.framesData[i].imageAvailableSemaphore, NULL);
        vkDestroySemaphore(vulkanContext.logicalDevice, vulkanContext.framesData[i].renderDoneSemaphore, NULL);
        vkDestroyFence(vulkanContext.logicalDevice, vulkanContext.framesData[i].renderDoneFence, NULL);
    }

    if (vulkanContext.graphicsPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(vulkanContext.logicalDevice, vulkanContext.graphicsPipelineLayout, NULL);
        vulkanContext.graphicsPipelineLayout = VK_NULL_HANDLE;
        vkDestroyPipeline(vulkanContext.logicalDevice, vulkanContext.graphicsPipeline, NULL);
        vulkanContext.graphicsPipeline = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < vulkanContext.maxFramesInFlight; ++i)
    {
        vkDestroyFramebuffer(vulkanContext.logicalDevice, vulkanContext.framesData[i].framebuffer, NULL);
    }

    if (vulkanContext.commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(vulkanContext.logicalDevice, vulkanContext.commandPool, NULL);
    }

    if (vulkanContext.depthBufferImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(vulkanContext.logicalDevice, vulkanContext.depthBufferImageView, NULL);
        vulkanContext.depthBufferImageView = VK_NULL_HANDLE;
        vkDestroyImage(vulkanContext.logicalDevice, vulkanContext.depthBufferImage, NULL);
        vulkanContext.depthBufferImage = VK_NULL_HANDLE;
        vkFreeMemory(vulkanContext.logicalDevice, vulkanContext.depthBufferImageMemory, NULL);
        vulkanContext.depthBufferImageMemory = VK_NULL_HANDLE;
    }

    if (vulkanContext.renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(vulkanContext.logicalDevice, vulkanContext.renderPass, NULL);
        vulkanContext.renderPass = VK_NULL_HANDLE;
    }

    for (uint32_t i = 0; i < vulkanContext.maxFramesInFlight; ++i)
    {
        vkDestroyImageView(vulkanContext.logicalDevice, vulkanContext.framesData[i].imageView, NULL);
    }

    // Destroy swapchain
    if (vulkanContext.swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(vulkanContext.logicalDevice, vulkanContext.swapchain, NULL);
    }

    free(vulkanContext.framesData);
    vulkanContext.framesData = NULL;

    // Destroy logical device
    if (vulkanContext.logicalDevice != VK_NULL_HANDLE)
    {
        vkDestroyDevice(vulkanContext.logicalDevice, NULL);
        vulkanContext.logicalDevice = VK_NULL_HANDLE;
    }

    // Destroy surface
    if (vulkanContext.surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(vulkanContext.instance, vulkanContext.surface, NULL);
        vulkanContext.surface = VK_NULL_HANDLE;
    }

    // Destroy Vulkan instance
    if (vulkanContext.instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(vulkanContext.instance, NULL);
        vulkanContext.instance = VK_NULL_HANDLE;
    }
}

bool InitSynchronizationTools(void)
{
    // --- Create semaphores that we use for rendering ---
    for (uint32_t i = 0; i < vulkanContext.maxFramesInFlight; ++i)
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo = {};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Set the flag to be already signalled at the start

        if ((vkCreateSemaphore(vulkanContext.logicalDevice, &semaphoreCreateInfo, NULL, &vulkanContext.framesData[i].imageAvailableSemaphore) != VK_SUCCESS) 
                || (vkCreateSemaphore(vulkanContext.logicalDevice, &semaphoreCreateInfo, NULL, &vulkanContext.framesData[i].renderDoneSemaphore) != VK_SUCCESS)
                || (vkCreateFence(vulkanContext.logicalDevice, &fenceCreateInfo, NULL, &vulkanContext.framesData[i].renderDoneFence) != VK_SUCCESS))
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
    vkGetPhysicalDeviceProperties(vulkanContext.physicalDevice, &physicalDeviceProperties);
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

    if (vkCreateSampler(vulkanContext.logicalDevice, &info, NULL, &vulkanContext.sampler) != VK_SUCCESS)
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
    vkEnumeratePhysicalDevices(vulkanContext.instance, &physicalDeviceCount, NULL);
    if (physicalDeviceCount > 0)
    {
        VkPhysicalDevice *physicalDevices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
        vkEnumeratePhysicalDevices(vulkanContext.instance, &physicalDeviceCount, physicalDevices);
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
    if (vkCreateImage(vulkanContext.logicalDevice, &imageInfo, NULL, &image) != VK_SUCCESS)
    {
        printf("[Engine] Failed to create image!\n");
        return ret;
    }

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(vulkanContext.logicalDevice, image, &memoryRequirements);

    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(vulkanContext.physicalDevice, &deviceMemoryProperties);

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
        vkDestroyImage(vulkanContext.logicalDevice, image, NULL);
        return ret;
    }

    VkDeviceMemory memory;
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    if (vkAllocateMemory(vulkanContext.logicalDevice, &allocInfo, NULL, &memory) != VK_SUCCESS)
    {
        printf("[Engine] Failed to allocate memory for the image!\n");
        vkDestroyImage(vulkanContext.logicalDevice, image, NULL);
        return ret;
    }

    vkBindImageMemory(vulkanContext.logicalDevice, image, memory, 0);

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

