#ifndef IGNIS_GRAPHICS_H
#define IGNIS_GRAPHICS_H

#include "dynamic_array.h"
#include "ignis/ignis.h"
#include "ignis/ignis_math.h"

#include <vulkan/vulkan.h>

#include <stdint.h>

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

void Ignis_Graphics_Draw(void);

void Ignis_Graphics_DrawCircle(RenderBatch *renderBatch, Vector2 position, float radius, Vector2 origin, Color color);
void Ignis_Graphics_DrawLine(RenderBatch *renderBatch, Vector2 start, Vector2 end, float width, Color color);
void Ignis_Graphics_DrawTexture(RenderBatch *renderBatch, Vector2 position, Vector2 size, Vector2 origin, Texture texture, Rect uvRect, Color tint);

#endif // IGNIS_GRAPHICS_H

