#include "ignis_graphics.h"

#include <math.h>

#define MAX_VERTICES_PER_BATCH 10000

void Ignis_Graphics_Draw(void)
{
}

void Ignis_Graphics_DrawCircle(RenderBatch *renderBatch, Vector2 position, float radius, Vector2 origin, Color color)
{
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

void Ignis_Graphics_DrawLine(RenderBatch *renderBatch, Vector2 start, Vector2 end, float width, Color color)
{
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

void Ignis_Graphics_DrawTexture(RenderBatch *renderBatch, Vector2 position, Vector2 size, Vector2 origin, Texture texture, Rect uvRect, Color tint)
{
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
